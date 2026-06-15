---
title: "Arquitectura del lazo RT de la CENTRAL — FSM prolija arriba, plomería invisible debajo"
date: 2026-06-14
author: "Claude (Anthropic - Claude Opus 4.8 1M)"
status: vivo
tipo: diseno
area: control
scope: src/central
requested-by: "Gustavo Viollaz (@gviollaz)"
robot: central
tags: [control, arquitectura, tiempo-real, fsm, no-bloqueante, andamiaje, post-incheon]
---

# Arquitectura del lazo RT de la CENTRAL

> **Cómo leer este doc.** Es un DISEÑO + plan de andamiaje, no una reescritura.
> El cerebro que juega hoy (`strategy.cpp` + `main_central.cpp::loop()`) **no se
> toca antes de Incheon**. Lo que sigue es el patrón hacia el que queremos
> migrar, entregado como módulos PUROS gateados off-by-default. La integración
> al lazo vivo es post-Incheon, en banco. Honestidad permanente en este doc:
> cada sección separa **lo que YA existe** de **lo nuevo**.

---

## 1. La esencia, en una frase

> **La FSM va arriba, prolija y legible (la única capa que el alumno toca para
> tunear una jugada); la plomería de tiempo real va abajo, casi invisible; y el
> lazo de control NUNCA se demora — siempre lee los buffers y siempre puede
> frenar, pase lo que pase.**

Si esa frase se cumple, el resto del doc es detalle.

---

## 2. Las 4 capas — diagrama

El lazo se organiza en 4 capas, de la plomería más oculta (abajo en la pila de
abstracción) a la lógica más legible (arriba). Los datos suben; las prioridades
mandan de arriba (reflejos) hacia abajo (FSM normal).

```
                            ╔══════════════════════════════════════════════╗
                            ║   CAPA 3 — FSM PROLIJA   (lo que se TUNEA)    ║
   el alumno vive acá  -->  ║   estados chicos + TABLA de jugadas separada ║
                            ║   cada movimiento: "evento O timeout -> salí" ║
                            ╚═══════════════════▲══════════════════════════╝
                                                │ comando OBJETIVO (vx,vy,ω)
                            ╔═══════════════════╩══════════════════════════╗
                            ║   CAPA 2 — CONTROL SUAVE  (slew / rampa)      ║
                            ║   tasa fija 100 Hz · suaviza la TRANSICIÓN    ║
                            ║   default = passthrough (binario idéntico)    ║
                            ╚═══════════════════▲══════════════════════════╝
                                                │ (los REFLEJOS la SALTEAN)
                            ╔═══════════════════╩══════════════════════════╗
   PREEMPTAN con return -->  ║   CAPA 1 — REFLEJOS  (alta prioridad)        ║
                            ║   STOP del árbitro · toque de LÍNEA BLANCA    ║
                            ║   freno DIRECTO al H-bridge (sin rampa)       ║
                            ╚═══════════════════▲══════════════════════════╝
                                                │ FOTO coherente del mundo
                            ╔═══════════════════╩══════════════════════════╗
                            ║   CAPA 0 — PLOMERÍA RT  (oculta, barata)      ║
                            ║   RX por ISR del core -> ring -> parseo       ║
                            ║   -> PIZARRA doble-buffer -> foto del tick    ║
                            ╚═══════════════════▲══════════════════════════╝
                                                │
        Serial7 (TOP / WorldSnapshot 100 Hz)  ─┤
        Serial1 (DOWN / LineStatusV2 200 Hz + Pose/Vel 100 Hz) ─┘
```

Regla de oro del dibujo: **la flecha de prioridad baja con `return`** (un reflejo
que actúa corta el resto de la vuelta), pero **la plomería de Capa 0 corre
SIEMPRE primero** (drenar UARTs no se saltea nunca, ni siquiera frenando).

---

## 3. El esqueleto del `loop()` nuevo (pseudocódigo)

Orden FIJO por prioridad, sin un solo bloqueo. Esto es el destino post-Incheon,
no lo que corre hoy.

```cpp
void loop() {
  // ---- CAPA 0: plomería RT (oculta, corre SIEMPRE, barata) ----
  watchdog_feed();                                 // YA EXISTE (gate CENTRAL_ENABLE_WDT)
  loop_monitor_update(g_loop_monitor, micros());   // YA EXISTE

  // (0a) RX: drenar los rings hacia la PIZARRA. HOY = polling cooperativo.
  comm_top_tick();    // -> blackboard_publish_snapshot()
  comm_down_tick();   // -> blackboard_publish_line()/pose()/vel()

  // (0b) FOTO del tick: una imagen coherente y CONGELADA del mundo para ESTA
  //      vuelta. Todas las capas de abajo leen de 'wm', no de los globales vivos.
  const WorldView wm = blackboard_begin_tick();

  // ---- CAPA 1: REFLEJOS (preemptan; NO pasan por la rampa) ----
  switch (reflex_arbitrate(g_reflex, reflex_inputs(wm), millis())) {
    case STOP_MOTORS:    motors_stop();  ctrl_reset(&g_ctrl); return;  // árbitro paró
    case BRAKE_EDGE:     motors_brake();                       return;  // borde inminente
    case RELEASE_TO_FSM: break;   // soltado -> dejá correr la FSM (ESCAPE despega)
    case NONE:           break;   // sin reflejo -> FSM normal
  }

  // ---- CAPA 2: CONTROL SUAVE (tasa fija, no bloqueante) ----
  if (g_since_ctrl_tick >= CTRL_PERIOD_MS) {       // 100 Hz
      g_since_ctrl_tick = 0;

      if (!wm.snapshot_fresh) {                     // TOP caído > 500 ms -> SAFE_NO_TOP
          motors_stop();
          ctrl_reset(&g_ctrl);
      } else {
          // ---- CAPA 3: FSM (la ÚNICA capa que el alumno lee/tunea) ----
          const MotorCommand target  = strategy_tick(wm);          // comando OBJETIVO
          const MotorCommand applied = ctrl_slew_step(&g_ctrl, target, dt);  // rampa
          motors_apply_command(applied);
      }
  }

  // ---- CAPA 0c: telemetría (gateada, nunca en el camino caliente) ----
  debug_print_if_due();
}
```

Reparto por capa — qué corre cada vuelta vs. gateado:

| Capa | Qué | Cadencia | Gateado |
|---|---|---|---|
| **0 plomería** | watchdog, monitor, drenar rings, foto del tick | cada vuelta (~kHz) | siempre |
| **1 reflejos** | STOP árbitro, freno/huida de línea | cada vuelta (<1–2 ms) | siempre; **preemptan con `return`** |
| **2 control** | rampa/slew del comando, SAFE_NO_TOP | fija 100 Hz | siempre; slew = passthrough por default |
| **3 FSM** | strategy_tick (jugadas, potencias, timeouts) | 100 Hz (dentro de Capa 2) | siempre |

**Cómo queda la FSM "oculta arriba" (lo que pide Gustavo).** `strategy.cpp` no
ve NADA de la plomería: no sabe de rings, ni de doble-buffer, ni de la rampa.
Recibe una FOTO del mundo y devuelve un comando OBJETIVO ("quiero ir a vx,vy,ω").
La rampa de Capa 2 decide CÓMO llegar suave. El alumno abre `strategy.cpp` y ve
la tabla de estados + la tabla de potencias/tiempos/timeouts, en español. La
plomería RT (Capa 0/1/2) vive en `main_central.cpp` + 3 módulos nuevos y NUNCA se
toca para tunear una jugada.

---

## 4. Capa por capa

### Capa 0 — Plomería RT: RX → pizarra

**Lo que YA existe (y se respeta):**
- El RX **ya es por interrupción** — del *core* de Teensy, no propio. El
  `IRQHandler` de cada `HardwareSerial` llena un ring; `comm_top_tick()`
  (`comm_top.cpp:47-56`) y `comm_down_tick()` (`comm_down.cpp:97-105`) solo lo
  VACÍAN con `while(available()) read()`. Nunca esperan bytes.
- DOWN ya amplió su ring a 512 B con `addMemoryForRead` (`comm_down.cpp:86-95`).
- El parseo (`FrameDecoder::feed`, byte a byte, CRC16 + resync) corre en el loop.
- La "pizarra" ya existe: globales planos en `world_model.cpp:9-19`
  (`g_snap`/`g_line`/`g_otos_*` + timestamps), con frescura `millis()-last < 500`
  (wrap-safe, `world_model.cpp:60-66`).

**Los 2 huecos respecto del objetivo (lo nuevo):**

1. **TOP sin colchón.** `comm_top_init()` (`comm_top.cpp:43-45`) **NO** llama
   `addMemoryForRead` → Serial7 sigue con el ring default de 64 B, que a 230400
   se llena en ~2.8 ms. Si el loop se atrasa, el WorldSnapshot del TOP se
   desborda en silencio. **Fix barato:** `Serial7.addMemoryForRead(buf, 512)`
   en `comm_top_init`, igual que DOWN. Esto cierra el agujero real de Capa 0
   sin tocar nada más.

2. **Pizarra single-buffer.** `world_model_apply_*` hace `memcpy` directo sobre
   los globales (`world_model.cpp:42,55`). HOY **no hay race** porque RX y
   lectura corren en el mismo hilo cooperativo: nadie interrumpe al lector. El
   doble-buffer es **seguridad-a-futuro** para el día que el RX/parseo pase a
   contexto async — recién ahí un `strategy_tick()` leyendo `g_snap` mientras
   una ISR lo reescribe daría un *torn read* (lectura cruzada de 2 frames).

**Cota de drain (defensa que conviene portar ya).** El TOP acota el drenado a
`MAX_BYTES_PER_TICK` por vuelta (`cameras_runtime.cpp`); replicarlo en CENTRAL
evita que una ráfaga monopolice una vuelta. Lo que sobra queda en el ring (con
512 B de colchón) y se drena la próxima vuelta.

### Pizarra doble-buffer — el patrón seqlock

En Teensy 4.1 (Cortex-M7, **mono-núcleo**) la única concurrencia posible es
**ISR-preempta-al-loop, en un solo sentido**: la ISR interrumpe al loop, el loop
nunca interrumpe a la ISR. Ese caso asimétrico es el libro de texto del
**seqlock** (contador de secuencia) — más barato que un doble-buffer con locks,
y libre de torn-read para esta preempción de una vía.

```cpp
template<typename T> struct SeqSlot {   // sensor_slot.h (PURO, host-testeable)
  volatile uint32_t seq;   // par = estable · impar = escritura en curso
  T data;                  // payload (copia entera de la struct)
  uint32_t stamp_ms;       // millis() del publish (frescura)
};
// PRODUCTOR (ISR o loop): seq++ (impar); dmb; data=v; stamp=now; dmb; seq++ (par)
// CONSUMIDOR (loop):       do { s=seq; if(impar) reintentá; dmb; copia=data;
//                              t=stamp; dmb; } while(seq != s)
```

Decisiones de diseño:
- **Un slot por dominio, NO un mega-slot.** `SeqSlot<WorldSnapshot>`,
  `SeqSlot<LineStatusV2>`, `SeqSlot<Pose2D>`, `SeqSlot<Velocity2D>`. Razón: ya
  llegan como frames separados con CRC propio y frescuras independientes
  (`world_model.cpp:129-133`). Un slot por frame = **un escritor por slot**
  (requisito del seqlock) y un frame perdido no invalida a los otros.
- **Cero cambio para `strategy.cpp`.** Los `world_model_apply_*` pasan a
  `publish()`; los 86 `world_model_get_*` siguen con la MISMA firma pero leen de
  un **cache local del tick**. Un `world_model_begin_tick()` hace `read_latest()`
  de los 4 slots UNA vez (no 86 veces) y refresca los `_last_ms`. Así el tick ve
  un snapshot **coherente y congelado** y la frescura sigue siendo
  `millis()-_last_ms`.
- **Gate off-by-default.** Bajo `-DCENTRAL_BLACKBOARD_SEQLOCK` (default OFF),
  `publish`/`read` usan las 2 barreras `dmb`; SIN el flag, son copia directa =
  **byte-idéntico** al `world_model.cpp` de hoy.

> ⚠️ El M7 reordena memoria. **Sin los 2 `dmb` el seqlock no protege nada** (el
> compilador/core puede mover la copia fuera de la ventana `seq`). El precedente
> `asm volatile("dsb")` ya está en `main_central.cpp:84` (init del WDT). La
> atomicidad se valida en banco (stress de RX por ISR, 0 campos cruzados) — no
> la cierra Claude.

### Capa 1 — Reflejos (preempción + devolución del control)

**Lo que YA existe (y es el patrón a formalizar):**
- El **freno de borde** se chequea CADA vuelta antes del tick de strategy
  (`main_central.cpp:296-317`), frena con `motors_brake()` y hace `return`. Ya
  cumple el espíritu de "reflejo": no pasa por rampa, reacciona en <1–2 ms.
- Tiene el **anti-latch de 350 ms** (fix banco María 2026-06-14,
  `main_central.cpp:293-312`): frena el momento y a los 350 ms SUELTA, para no
  congelar al arquero sobre su línea. Esto resolvió un deadlock de 5+ s.
- El **STOP del árbitro** hoy viaja por el camino LENTO: lo corta la propia FSM
  dentro del tick (`!world_model_match_running()` →`return cmd` con ceros,
  `strategy.cpp`), tras pasar por el gate de 100 Hz.

**Lo nuevo — un módulo PURO `reflex.h` (POD + funciones libres, como
`loop_monitor.h`) que es la máquina de prioridades:**

```cpp
enum ReflexAction { NONE, STOP_MOTORS, BRAKE_EDGE, RELEASE_TO_FSM };
struct ReflexState  { uint8_t owner; uint32_t edge_since_ms; bool edge_released; };
struct ReflexInputs { bool match_running; bool edge_now; };  // edge_now ya pre-AND con line_fresh
ReflexAction reflex_arbitrate(ReflexState&, const ReflexInputs&, uint32_t now_ms);
```

Prioridad estricta dentro de `reflex_arbitrate`:
- **P-ALTA — STOP del árbitro:** `if(!match_running) -> STOP_MOTORS`. Gana
  SIEMPRE, incluso sobre el borde (si el partido paró, motores OFF y listo).
  Resetea el estado del borde. **Cambio de conducta vs hoy:** hoy el STOP NO
  preempta el borde; con esto sí (es lo correcto). A validar en banco.
- **P-MEDIA — borde de línea:** réplica EXACTA del anti-latch de hoy: arranca
  `edge_since` si =0; a los 350 ms `edge_released=true`; si NO released →
  `BRAKE_EDGE`; si released → `RELEASE_TO_FSM`.
- **P-BAJA — `NONE`** → la FSM manda.

**Por qué esto es "tomar y devolver el control" bien hecho** (la lección del
deadlock): el reflejo es DUEÑO solo mientras `BRAKE_EDGE`, y ese dominio está
ACOTADO a 350 ms por construcción. Pasados los 350 ms emite `RELEASE_TO_FSM`: NO
hace `return` → deja correr `strategy_tick` → la FSM **ESCAPE** (`strategy.cpp:1362-1371`,
`GKS_ESCAPE_SPEED=470`, `GKS_ESCAPE_MS=1700`) saca al robot de la línea. **El
handoff es por el VALOR DE RETORNO** (`RELEASE` vs `BRAKE`), no por una bandera
que alguien tiene que acordarse de bajar. El estado se re-arma solo cuando
`edge_now` baja. El test de regresión obligatorio: portar el CSV de María como
caso host (tras `RELEASE` no re-frenar hasta que `edge_now` baje).

### Capa 2 — Control suave (slew con BYPASS para reflejos)

**Lo que YA existe:** NADA. El comando va SIN rampa ni filtro. `main_central.cpp:331-332`
aplica `strategy_tick()` crudo cada 10 ms; el doc de latencias lo dice textual:
"Aplicación de motores 100 Hz, **sin rampa ni filtro**"
(`CONTROL-ARQUERO-LATERAL-Y-LATENCIAS.md:107`). El único "suavizado" es el
kickstart (escalón de arranque, NO rampa) y los pisos (clamp, NO rampa).

**Lo nuevo — módulo PURO `motor_slew.h` (`ctrl_slew_step`):** limita la VARIACIÓN
permitida por tick de `vx`/`vy`/`omega` hacia el objetivo.

**Dónde vive el slew — decisión load-bearing:** en **espacio de velocidad**
(`vx/vy/omega` del `MotorCommand`, mm/s y centideg/s), **upstream de la
cinemática**. NO en espacio PWM. Razón: si suavizo el PWM por rueda peleo contra
los pisos `{70,70,107}` y el kickstart `{130,130,140}` — ellos LEVANTAN el PWM y
un slew sobre eso se anularía o haría dientes de sierra. Suavizando `vx/vy/omega`
ANTES, la cinemática + pisos + kickstart reciben un comando ya limpio y trabajan
IGUAL (la mezcla validada en banco se conserva exacta porque el slew no cambia la
dirección en régimen, solo la transición).

**No pelear con el kickstart:** orden = `[slew] → kinematics → pisos → kickstart →
PWM`. El kickstart se dispara en la transición `pwm==0 → pwm!=0` POR RUEDA; el
slew arranca `vx` desde 0 pero el primer paso (`accel·dt`) ya da `vx>0` → PWM
pequeño → el piso lo sube → el kickstart ve la transición y pega el golpe IGUAL.
El slew suaviza el RÉGIMEN posterior, no el primer escalón. Eso es lo deseado:
golpe para arrancar + rampa para no tironear.

**BYPASS para reflejos (3 vías):**
- `motors_brake()` (borde) y `motors_stop()` (sin snapshot) NO pasan por
  `motors_apply_command` → **ya bypassados por arquitectura**.
- El ESCAPE de la FSM SÍ pasa por `apply_command` → se marca como reflejo para
  saltear el slew. **Implementación recomendada: un `motors_apply_command_reflex()`
  separado, NO un campo nuevo en `MotorCommand`** (el struct hoy no tiene
  `static_assert` de tamaño, pero `spin_pwm` ya es un override interno frágil;
  agregar un campo cambia `sizeof` y roza la caja negra — mejor una segunda
  entrada que un flag en el struct).

**Default = passthrough.** `motor_slew` arranca con límite infinito → el comando
pasa igual → **binario byte-idéntico** hasta titrar en banco con la caja negra.
`omega` casi pasante (el lazo de rumbo ya sufre 35–70 ms de atraso; rampar omega
de más suma latencia justo ahí). `decel_factor > 1`: frenar/invertir puede ir más
rápido que acelerar (clave para que el arquero cruce a tiempo al cambiar de lado).

`dt` entra como parámetro (medido con `elapsedMicros`, clamp 1–50 ms como hace
`HeadingPID`) → la rampa en mm/s² es correcta aunque el loop tiemble.

### Capa 3 — FSM prolija (ver §5, es la estrella)

`strategy_tick()` ya es PURO respecto del tiempo: lee la pizarra y `millis()`,
devuelve un `MotorCommand`, **cero `delay()`** (verificado), y sus esperas ya son
timeouts no-bloqueantes. La transición clave del patrón es **recibir la foto `wm`
como argumento** en vez de leer los globales — eso desacopla la FSM de la
plomería. Hoy `strategy_tick()` no toma argumentos (`strategy.cpp:1819`); el
patrón los introduce gradualmente, post-Incheon.

---

## 5. El patrón de FSM prolija (la estrella)

El objetivo de Gustavo: que `strategy.cpp` quede como la capa de ARRIBA legible,
donde **tunear una jugada = cambiar un número en una tabla**, y donde **cada
movimiento tiene su watchdog "espero un sensor O salgo por timeout"**.

**Lo que YA existe:** los timeouts de la FSM **ya son no-bloqueantes** —
`now_ms - t0 >= TIMEOUT_MS` con tabla de constexpr (`GK_GOTO_LINE_TIMEOUT_MS=4000`
`strategy.cpp:264`, `GK_ADVANCE_TIMEOUT_MS=1500` :305, chequeos en :1266/:1283).
El patrón que Gustavo pide YA está, parcialmente. **Lo que falta es PROLIJIDAD**,
y existe el andamiaje para construirla: `src/shared/strategy_transitions.{h,cpp}`
define structs PUROS (`AtkWorldView`/`GkWorldView` = la pizarra de entrada,
`AtkTuning`/`GkTuning` = la tabla) host-testeados (35 tests), a propósito NO
cableados al cerebro vivo.

**Tres dolores concretos de `strategy.cpp` hoy (1846 líneas):**
1. **Mecanismo y tabla mezclados** — los ~80 constexpr de tuning viven sueltos
   en mitad del archivo (`strategy.cpp:142-461`). No hay UNA tabla.
2. **Cada estado reinventa su timeout** — ~15 timers hand-rolled, cada uno con su
   global `g_*_started_ms` (`:97/:100/:115/:126`) y su `if (now - start >= T)`
   copiado a mano. Fácil olvidar el fallback → estado colgado (justo lo que
   queremos prohibir).
3. **Variantes por `#ifdef`** hinchan el archivo (GK_SIMPLE_STRAFE ~285 líneas
   es una 2ª FSM de arquero tapando a la de PATROL).

### Pieza 1 — Tabla de jugadas (separada del mecanismo)

Un solo struct, todo junto, comentado en español. Una constante = una línea.

```cpp
struct JugadasGK {              // gk_tuning.h
  float    patrulla_vel_mm_s;   // potencia del strafe lateral
  uint32_t escape_ms;           // cuánto huye al tocar línea
  float    escape_vel_mm_s;     // potencia de la huida
  uint32_t goto_line_timeout_ms;// si no encuentra su línea en esto -> patrulla igual
  // ...
};
constexpr JugadasGK JUGADAS_GK = { /*patrulla*/ 200, /*escape*/ 1700, /*vel*/ 470, ... };
```

Tunear en banco = cambiar UN número en `JUGADAS_GK`. El mecanismo no se toca.

### Pieza 2 — Helper uniforme de espera-no-bloqueante (mata las ~15 copias)

`state_timer.h` (PURO, mismo molde que `loop_monitor.h`/`heading_rate.h`):

```cpp
struct StateTimer { uint32_t start_ms; bool armed; };   // .bss zero-init = desarmado
inline void     state_timer_arm    (StateTimer& t, uint32_t now){ t.start_ms=now; t.armed=true; }
inline void     state_timer_reset  (StateTimer& t){ t.armed=false; }
inline bool     state_timer_expired(const StateTimer& t, uint32_t now, uint32_t timeout_ms){
                   return t.armed && (now - t.start_ms) >= timeout_ms;  // wrap-safe (resta unsigned)
                }
```

Detalle clave: **`expired` de un timer NO armado devuelve `false`**. Esto arregla
de raíz el bug latente de hoy (un `started_ms == 0` compara contra un `now` grande
y "vence" al instante). Es fail-SOFT a propósito.

### Pieza 3 — Un estado = una función chica, con forma FIJA

Contrato uniforme: recibe la pizarra (`WorldView`) y la tabla (`Jugadas`),
decide SOLO su salida (`MotorCommand`) y su próximo estado, y SIEMPRE tiene la
trampa "evento O timeout → fallback".

**Un estado de ejemplo escrito lindo, en español:**

```cpp
// BUSCAR: giro lento en el lugar hasta VER la pelota. Si pasan BUSCAR_TIMEOUT
// sin verla -> me reposiciono (avanzo un toque) y sigo buscando. Nunca me clavo.
EstadoAtk buscar_tick(const VistaAtk& w, const JugadasAtk& J,
                      StateTimer& timeout, MotorCommand& cmd) {
    cmd.spin_pwm = J.buscar_spin_pwm;                 // giro lento (tabla)

    if (w.pelota_confirmada)                          // EVENTO -> persigo
        return EstadoAtk::ACERCARSE;

    if (state_timer_expired(timeout, w.now_ms, J.buscar_timeout_ms)) {  // TIMEOUT -> fallback
        return EstadoAtk::REUBICAR;                   // no me quedo girando para siempre
    }
    return EstadoAtk::BUSCAR;                          // sigo (no bloquea: solo devuelvo cmd)
}
```

El despachador de arriba queda como una tabla legible, un renglón por conducta:

```cpp
switch (estado) {
  case BUSCAR:    estado = buscar_tick(w, J, timeout, cmd);    break;
  case ACERCARSE: estado = acercarse_tick(w, J, timeout, cmd); break;
  case EMPUJAR:   estado = empujar_tick(w, J, timeout, cmd);   break;
  // ... la táctica se lee de corrido
}
// Al ENTRAR a un estado nuevo, en la transición: state_timer_arm(timeout, now)  -> el watchdog se re-arma solo.
```

> **La regla de oro pedagógica:** *state_timer = "esta jugada no funcionó, probá
> otra"; WDOG1 = "el robot entero se congeló, reiniciate".* Ver §6.

---

## 6. Timeout de estado (lógico) vs watchdog de HW (WDOG1) — la diferencia

Son **dos niveles distintos y complementarios**. Confundirlos es el error
pedagógico más común.

| | **Timeout de estado** (`state_timer.h`) | **Watchdog de hardware** (WDOG1) |
|---|---|---|
| Qué vigila | Que un MOVIMIENTO logre su objetivo | Que TODO el loop siga girando |
| Quién lo maneja | La FSM, en software | El i.MX RT1062, en silicio |
| Su "castigo" | Una TRANSICIÓN de fallback (cambiar de jugada) | RESET duro del Teensy a 1 s |
| Dónde corre | Dentro del mismo loop | Independiente del loop |
| Cubre un cuelgue total | **NO** (vive dentro del loop colgado) | **SÍ** (es su única razón de ser) |
| Cubre "no logro mi objetivo" | **SÍ** | **NO** (el loop sigue feliz) |
| Estado | NUEVO (a andamiar) | YA EXISTE — `main_central.cpp:76-92`, gate `CENTRAL_ENABLE_WDT`, default OFF |

El `state_timer` **alimenta implícitamente** al WDOG1: como nunca bloquea, el
loop sigue llegando a `watchdog_feed()` cada vuelta. Un loop colgado se lleva
puesto al `state_timer` también — por eso hace falta el WDOG1 por encima.

---

## 7. Presupuesto de tiempo — por qué NUNCA demora

Ninguna de las 4 capas bloquea, **por construcción**:

- **Capa 0 (RX):** la captura byte-a-byte ya ocurre en el ISR del core, async al
  loop. El drain es `while(available())` (nunca espera bytes) + cota
  `MAX_BYTES_PER_TICK`. El `begin_tick` del blackboard es O(1) (read de índice +
  copia de struct chico). El seqlock es **wait-free** para el productor (ISR) y
  **lock-free con reintento acotado** para el lector (1–2 reintentos en el peor
  caso, microsegundos).
- **Capa 1 (reflejos):** `reflex_arbitrate` es aritmética pura (2 if + restas),
  sub-µs. `motors_brake()`/`motors_stop()` son escritura-de-registros, no
  esperan. El `return` reacciona en <1–2 ms (lo que tarda la vuelta).
- **Capa 2 (rampa):** `ctrl_slew_step` es un clamp aritmético (3 ejes × resta +
  min/max), <1 µs en el M7 con FPU. A tasa fija por `elapsedMillis`, NO por
  busy-wait.
- **Capa 3 (FSM):** ya es no-bloqueante hoy (0 `delay()` en `strategy.cpp`); sus
  esperas son timeouts (comparaciones), no sleeps.

**La regla dura se cumple:** el loop SIEMPRE drena rings (Capa 0) y SIEMPRE puede
frenar (Capa 1) en cada vuelta, pase lo que pase arriba. Jamás un `delay`; las
esperas son `elapsedMillis`/`Micros`. **El `LoopMonitor` (ya vivo,
`main_central.cpp:214`) es la prueba empírica:** con todas las capas activas, su
`max_us` debe quedar pegado a la cadencia real (sin picos). Si sube, una capa
introdujo bloqueo — y eso se ve antes de que sea un cuelgue.

**Viabilidad Teensy 4.1 (i.MX RT1062 @600 MHz, M7 + FPU) — todo soportado y ya en
uso en este firmware:** RX por ISR + `addMemoryForRead` (ya en `comm_down.cpp:91`);
`elapsedMicros`/`IntervalTimer` (PIT) en el core; store de `uint32` atómico + `dsb`
para el seqlock (precedente `main_central.cpp:84`); FPU para slew/PID; WDOG1 ya
portado. Coherente con el precedente del TOP: interrupciones + doble-buffer, **NO
RTOS, NO threads**. Los 3 módulos nuevos se escriben PUROS → compilan y se testean
host con g++ (toolchain del equipo / g++ de Webots) ANTES de tocar la placa.

---

## 8. Plan por fases (gateado off-by-default) + plan de banco

Cada fase entrega módulos PUROS + tests host + gate OFF. **El binario de Incheon
no cambia** mientras los flags estén OFF.

| Fase | Entregable | Gate | Cierre (banco — lo cierra el EQUIPO) |
|---|---|---|---|
| **F0 (ya)** | `loop_monitor.h` vivo + WDOG1 gateado | `CENTRAL_ENABLE_WDT` | hecho |
| **F1** | Colchón TOP: `Serial7.addMemoryForRead(512)` + cota de drain | (directo, bajo riesgo) | 0 `badsz`/desbordes con `loop_us` sano |
| **F2** | `state_timer.h` + `gk_tuning.h`/`atk_tuning.h` + helper · plantilla de 1 estado OFF | (sin gate; no cableado) | tests host verdes; migración real estado-por-estado post-Incheon, cada uno con regresión de caja negra |
| **F3** | `reflex.h` (máquina de prioridades) | `CENTRAL_REFLEX_LAYER` | freno de borde byte-equivalente + STOP preempta + CSV de María sin re-frenar |
| **F4** | `motor_slew.h` (passthrough por default) | `CENTRAL_MOTION_SLEW` | titrar `accel`/`decel_factor` con caja negra; kickstart y `{70,70,107}` intactos |
| **F5** | `sensor_slot.h` + `blackboard` doble-buffer | `CENTRAL_BLACKBOARD_SEQLOCK` | **solo junto con RX por ISR**; 0 torn-reads bajo stress |

> **Orden correcto (mitigación del riesgo del doble-buffer):** F5 (doble-buffer)
> se integra al loop SOLO en el mismo paso que el RX pase a ISR/DMA. Antes de eso
> es complejidad sin beneficio (hoy no hay race). Primero ISR, en la misma sesión
> el doble-buffer.

**Plan de banco (regla dura del repo — nada de esto lo cierra Claude):**
1. **No-demora:** medir `loop_us(max)` del `LoopMonitor` con cada capa activa.
   Debe quedar pegado a la cadencia real, sin picos.
2. **Reflejos:** confirmar con la caja negra que el freno de borde sigue
   byte-equivalente y que el anti-latch de 350 ms no se perdió (replay del CSV de
   María como regresión).
3. **Rampa:** titrar `accel`/`decel_factor` midiendo el período del cruce del
   arquero antes/después; verificar que el kickstart no se aplanó.
4. **Doble-buffer:** stress de RX por ISR, verificar 0 campos cruzados de 2
   frames.

---

## 9. Riesgos

| # | Prioridad | Riesgo | Mitigación |
|---|---|---|---|
| R1 | **P2** | **Doble-buffer prematuro.** Hoy el RX es cooperativo (un hilo) → el torn-read que el doble-buffer previene NO existe aún. Integrarlo antes que el RX sea ISR = complejidad + superficie de bug sin beneficio. | Entregar `sensor_slot.h` PURO testeado y gateado OFF; integrar SOLO en el mismo paso que el RX pase a ISR (F5, post-Incheon). |
| R2 | **P1** | **La rampa puede romper reflejos finos del arquero.** El strafe depende de `FLOOR_SCALE` + kickstart; un slew que suavice el arranque MATA el kickstart y reintroduce la zona muerta. | `motor_slew` arranca passthrough (byte-idéntico); el kickstart queda AGUAS ABAJO de la rampa, no aguas arriba; titrar en banco con caja negra. |
| R3 | **P1** | **Perder el anti-latch al formalizar el reflejo.** El `return` del borde ya congeló al arquero una vez (deadlock 5+ s, fix María 2026-06-14). | `reflex_edge_brake` porta EXACTO la máquina `s_edge_brake_since/_released`; test host con el CSV de María como regresión. |
| R4 | **P1** | **`motors_brake()` puede quedar en COAST** (no freno activo) según el chip del Zircon (`motors_zircon.h:24-29`). Si es COAST, el reflejo de borde es decorativo. | Identificar el integrado (Enzo/datasheet) + medir banco brake-vs-stop (1–2 h). NO marcar Capa 1 `done` sin esto. |
| R5 | **P1** | **STOP que ahora preempta el borde = cambio de conducta.** Si el árbitro para mientras el robot frena un borde, ahora corta `motors_stop()` (coast) en vez de seguir el brake → ¿desliza hacia la línea por inercia? | Verificar en banco que el corte por STOP es el deseado; incluido en el banco de validación de Capa 1. |
| R6 | **P2** | **`sensor_slot.h` del TOP no existe aún** (0 hits en `git ls-files`). Si el TOP lo crea con otra semántica, divergen dos doble-buffers. | Coordinar UNA firma común (o reusar el header literal del TOP cuando lo cree) antes de codear `blackboard`; documentarlo en `FUENTES-DE-VERDAD.md`. |
| R7 | **P2** | **Sobre-ingeniería a 2 semanas de Incheon.** El loop vivo YA cumple la regla dura. Reescribirlo ahora arriesga el binario de competencia por una mejora que rinde post-mundial. | Alcance pedido: SOLO diseño + plantilla + módulos puros gateados; NO tocar `loop()` ni `strategy.cpp` vivos antes del mundial. |
| R8 | **P1** | **Verificación: nada de esto lo cierra Claude.** La "no demora" se PRUEBA con el `LoopMonitor` en banco; sin ese banco el diseño queda en backlog (regla no-negociable del repo). | Plan de banco §8; el EQUIPO con la placa es el único que cierra TASKs de hardware. |
| R9 | **P2** | **Tocar `MotorCommand` para el flag `reflex`** cambia `sizeof` y roza la caja negra. | Usar un `motors_apply_command_reflex()` separado en vez de un campo en el struct (ya recomendado en §4 Capa 2). |

---

## 10. Honestidad — lo que YA existe vs lo nuevo (resumen)

| Pieza | Estado |
|---|---|
| Freno de borde cada vuelta + anti-latch 350 ms | **YA EXISTE** (`main_central.cpp:293-317`) — patrón a formalizar como Capa 1 |
| WDOG1 (watchdog de HW) gateado | **YA EXISTE** (`main_central.cpp:76-92`, default OFF) |
| `LoopMonitor` (observador de la no-demora) | **YA EXISTE** (`loop_monitor.h`, vivo, sin flag) |
| Timeouts no-bloqueantes de la FSM (tabla constexpr) | **YA EXISTE** (`strategy.cpp:264/305/1266/1283`) |
| Gate de 100 Hz de strategy | **YA EXISTE** (`main_central.cpp:320`) |
| Cero `delay()` en el loop y en `strategy.cpp` | **YA SE CUMPLE** (verificado) |
| RX por ISR del core + ring 512 B en DOWN | **YA EXISTE** (`comm_down.cpp:86-95`) |
| Colchón 512 B en TOP (Serial7) | **NUEVO** (hueco real — F1) |
| `sensor_slot.h` / blackboard doble-buffer | **NUEVO** (no existe — F5) |
| `reflex.h` (máquina de prioridades) | **NUEVO** (F3) |
| `motor_slew.h` (rampa, passthrough default) | **NUEVO** (F4) |
| `state_timer.h` + `gk_tuning.h`/`atk_tuning.h` | **NUEVO** (F2) |
| `strategy_transitions.{h,cpp}` (andamiaje de tabla) | **YA EXISTE** (puro, 35 tests, NO cableado) |

---

## Docs hermanos

- `docs/firmware/STRATEGY-CPP-COMO-FUNCIONA.md` — la FSM de hoy (rampa de entrada
  a `strategy.cpp`).
- `docs/firmware/CONTROL-ARQUERO-LATERAL-Y-LATENCIAS.md` — el lazo de rumbo y sus
  latencias (fuente del "sin rampa ni filtro" y del presupuesto de latencia).
- `docs/firmware/ESTIMACION-FUSION-TOP.md` — la capa de estimación/fusión del TOP
  (de donde se reusa el patrón del slot doble-buffer; coordinar la firma).
