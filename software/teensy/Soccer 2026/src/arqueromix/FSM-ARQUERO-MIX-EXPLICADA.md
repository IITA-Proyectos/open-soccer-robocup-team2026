---
title: "Cómo funciona el ARQUERO MIX (arqueromix) — explicación fiel de la máquina de estados"
date: 2026-06-21
status: vivo (explicativo)
audiencia: "Quien vaya a MODIFICAR la FSM del arquero (Virginia, Elías, alumnos 2027)"
author: "Claude Opus 4.8 (Anthropic), vía Claude Code"
requested-by: "Gustavo Viollaz (@gviollaz)"
tipo: documento-explicativo
fidelidad: "Escrito leyendo el código real al 2026-06-21 (commit 2558427). Verificado con pasada adversarial."
---

# El ARQUERO MIX, explicado para poder modificarlo

> **Para qué sirve este documento.** Es la **base** para tocar la máquina de estados del arquero sin
> romperla: explica, desde lo conceptual hasta el código, qué partes tiene el programa, **de dónde salen
> los datos** (pelota, arcos, línea del piso) y **qué hace cada estado**. No es el log de banco (eso está en
> [`DOCUMENTACION.md`](DOCUMENTACION.md)); esto es el "mapa del motor".

---

## 0. En una frase

`arqueromix` es el **arquero campeón 2025** (su máquina de estados y su manejo directo de motores)
**revivido sobre el robot 2026**: en vez de leer sus propios sensores, lee por cable lo que ya procesaron
las placas **TOP** (cámaras + rumbo) y **DOWN** (sensores de piso). Es un arquero **reactivo**: no planea,
solo reacciona a "veo la pelota / veo mi arco / piso la línea".

> ⚠️ **NO está testeado en hardware.** Compila ≠ anda: varios signos y umbrales se confirman en banco
> (marcados a lo largo del doc). Solo el equipo cierra eso.

---

## 1. Tres conceptos antes de entrar al código

**(a) Qué es una máquina de estados (FSM).** El robot está siempre en **un** estado (ej. "patrullando a la
derecha", "despejando"). En cada vuelta del programa (cada *tick*), según lo que ve, **hace algo** (mueve
motores) y **decide si cambia de estado**. Es como un juego de mesa: estás en una casilla, mirás los dados
(los sensores) y avanzás a otra casilla.

**(b) El ciclo del programa (`loop`).** El programa repite, miles de veces por segundo, dos pasos
(`main_arqueromix.cpp:32-35`):

```
loop():
   1) amix_comm_tick()  → leer los cables (TOP y DOWN) y actualizar "lo que el robot sabe"
   2) amix_fsm_tick()   → con eso, decidir y mover los motores
```

**(c) Reactivo, egocéntrico, por ÁNGULO.** El arquero no sabe "dónde está en la cancha". Razona en
**relativo a sí mismo**: "la pelota está a tantos grados a mi derecha", "mi arco está atrás". Y desde un fix
clave de banco, **sigue la pelota por su ÁNGULO**, no por distancia en milímetros (ver §3).

---

## 2. Las partes del programa (qué archivo hace qué)

```
   TOP  (Serial7) ── WorldSnapshot ──┐  (pelota + arcos + rumbo + árbitro)
                                      ├─► amix_comm ─► g_aio ─► amix_fsm ─► amix_motors ─► motores
   DOWN (Serial1) ── LineStatusV2 ────┘  (línea del piso)      (datos)    (decide)   (mueve)
                  ── Pose2D / Vel2D ──┘
```

| Archivo | Qué hace | Regla mental |
|---|---|---|
| `main_arqueromix.cpp` | El arranque: `setup()` inicializa todo; `loop()` hace comm→fsm. | No tiene lógica de juego. |
| `amix_comm.cpp/.h` | **El único que toca los cables (Serial).** Decodifica los mensajes de TOP y DOWN y **llena `g_aio`**. | "Traductor de cables a variables." |
| `amix_io.h` | Define `g_aio`: el **bloc de notas** con todo lo que el robot sabe (pelota, arcos, línea, rumbo, árbitro). | El "estado del mundo", plano y simple. |
| `amix_fsm.cpp/.h` | **El cerebro**: la máquina de estados. Lee `g_aio`, decide, llama primitivas de motor. | Acá se modifica el comportamiento. |
| `amix_motors.cpp/.h` | El **vocabulario de movimiento**: `avanzar`, `girar`, `patear_atras`, etc. Escribe los pines de motor. | "Cómo se mueve", no "cuándo". |
| `amix_config.h` | **Todas las perillas**: tolerancias, velocidades, tiempos, signos. | Acá se TUNEA sin tocar lógica. |

**Clave de diseño:** la FSM **nunca toca Serial ni hace cuentas de cámara**. Solo lee `g_aio`. Eso permite
modificar el comportamiento (la FSM) sin meterse con la comunicación. Es lo mismo que hacía el arquero 2025
con sus "variables globales sueltas", pero ordenado.

---

## 3. De dónde vienen los datos (pelota, arcos, línea)

Todo lo que el robot "sabe" vive en `g_aio` (`amix_io.h`), y lo llena `amix_comm.cpp` a partir de **dos
cables** a 230400 baud. **El arquero NO tiene cámara ni sensores propios:** todo llega ya procesado.

### 3.1 Del cable de ARRIBA (TOP, Serial7) — `WorldSnapshot`

`amix_comm.cpp::apply_top_snapshot` (l.72-118) saca de un solo mensaje:

- **Pelota** (`ball_x_mm`, `ball_y_mm`, `ball_visible`): posición relativa al robot (+X=derecha, +Y=adelante)
  y si la cámara la ve. **Importante:** `amix_comm` calcula además el **ángulo a la pelota** con
  `atan2(x, y)` (l.82-83) → `angulo_pelota_deg` (0=adelante, >0=derecha). La FSM sigue la pelota por **ese
  ángulo**, no por los mm crudos (porque la escala mm está sin calibrar — ver el fix de §3.4).
- **Arcos por ROL, sin color** (`goal_opp_*` = rival, `goal_own_*` = propio): ángulo + distancia + visible
  (l.93-98). **Quién es el rival y quién el propio lo decidió la placa TOP** (módulo `goal_polarity`: "el
  arco al frente del robot es el rival"). El arquero **nunca pregunta el color** (amarillo/azul); solo usa
  el rol ya resuelto. *(Para el detalle ver `docs/firmware/DETECCION-DE-COLOR-COMO-FUNCIONA.md`.)*
- **Rumbo** (`heading_deg`, `heading_valid`, `heading_error_deg`): el ángulo de orientación, del BNO de la
  placa TOP (l.100-112). No hay BNO local en esta placa. `heading_error_deg` = cuánto se desvió respecto del
  rumbo de arranque (se "sella" la primera vez que hay rumbo válido). Es el `error` que usa la corrección de
  la patrulla.
- **Árbitro** (`match_running`): si el partido está en juego (l.115). **La FSM solo se mueve si es true.**

### 3.2 Del cable de ABAJO (DOWN, Serial1) — la línea del piso

`amix_comm.cpp::apply_down_line` (l.132-137) saca del mensaje `LineStatusV2`:

- `line_present` (¿hay línea bajo el robot?), `line_angle_deg` (ángulo de la línea), `line_depth` (**cuántos**
  sensores de los 32 ven blanco, 0..32).

> ⚠️ **Lo que NO llega (importante para modificar):** el arquero recibe un **conteo** de sensores
> (`line_depth`), NO **cuáles** sensores ni la distancia perpendicular a la línea (`cross_track_mm`). Esos
> datos existen en el cable pero `apply_down_line` no los copia. Si querés un seguidor de línea fino, falta
> exponerlos. (Análisis completo: `research/in-progress/2026-06-21-arquero-seguidor-linea-cross-track-que-sube-down.md`.)

También llega el **heading del OTOS** (`Pose2D`, l.139-150) como alternativa de rumbo (solo activa con
`-DARQMIX_HEADING_OTOS`), y `Velocity2D` solo para saber que el cable está vivo.

### 3.3 Frescura de enlaces

`update_link_freshness` (l.165-173) marca `top_link_fresh` / `down_link_fresh` si llegó algo en los últimos
500 ms. (Hoy la FSM **no** los usa para decidir — son para diagnóstico / futuro.)

### 3.4 El fix que hay que entender sí o sí: seguir la pelota por ÁNGULO

En el banco (2026-06-21) pasó esto: **la cámara veía la pelota pero el robot no se movía.** Causa: la FSM
decidía con los mm crudos y umbrales en mm, pero la escala del snapshot está sin calibrar → la pelota caía
en una "banda muerta" y el robot hacía `parar()`. **Solución:** decidir por el **ángulo** (`atan2`), que no
depende de la escala. Por eso hoy los umbrales de pelota son **angulares** (8°, 30°) más una distancia
(250 mm) que es el knob principal a tunear. (Detalle en `amix_config.h:142-150`.)

---

## 4. El vocabulario de movimiento (primitivas de motor)

Antes de la FSM, hay que conocer "cómo se mueve". El robot es **omni de 3 ruedas**. `amix_motors.cpp` no usa
cinemática: cada función fija PWM + sentido de los 3 motores **directo** (como el 2025). `amix_set_motor(idx,
pwm_con_signo)` maneja un motor (signo = sentido). Las primitivas:

| Primitiva | Qué hace | Detalle |
|---|---|---|
| `parar()` | Frena los 3 motores. | **Además resetea la rampa del golpe** (`s_kick_active=false`). |
| `adproporcional(pd, error)` | Strafe a la **derecha** + corrección de rumbo. | 3 bandas según `error`. `pd` = fuerza. |
| `aiproporcional(pd, error)` | Strafe a la **izquierda** + corrección de rumbo. | Espejo del anterior. |
| `avanzar()` | Avance recto (M1=+100, M2=−100, M3=0). | Usado tras el despeje. |
| `avanzar_inicio()` | Avance recto del homing, **más lento** (PWM 75). | Igual sentido que `avanzar`, solo baja la velocidad. |
| `avanzar_patear()` | Golpe de despeje con **rampa** (0→180 en ~90 ms). | NO bloqueante; arranca de 0 cada vez. |
| `girar(pwm)` | **Rotación pura** en el lugar (3 ruedas mismo sentido). | Para apuntar al arco rival antes de patear. |
| `patear_atras()` | Retroceso recto (M1=−120, M2=+120). | Retroceso del despeje. |
| `retroceder_inicio()` | Retroceso del homing (PWM propio, sentido flippable). | Para ir a buscar la línea del área al arrancar. |

> ⚠️ **El sentido físico de CADA primitiva se RE-VERIFICA en banco.** Con el cableado 2026 una primitiva
> puede salir invertida o lateral. Por eso hay **perillas de signo** (`AMIX_*_SIGN`, ver §6).
>
> **La corrección de rumbo** (en `ad/aiproporcional`): mira `error` (cuánto se desvió del rumbo de arranque)
> y modula la rueda trasera en 3 bandas para **enderezar** mientras strafea. El signo correcto (`-1`) está
> **validado en banco**; con `+1` el arquero se daba vuelta 180°. **Solo funciona si hay rumbo válido del TOP.**

---

## 5. La máquina de estados (12 estados)

> El comentario del `amix_fsm.h` todavía dice "10 estados" (quedó viejo): hoy son **12** — se agregaron
> `ALINEAR_arco_opp` y la separación de la salida de línea. Esto es lo FIEL.

Los 12 estados se agrupan en **3 fases**: **INICIO** (acomodarse en el arco), **PATRULLA** (ir y venir
siguiendo la pelota), **DESPEJE** (sacar la pelota). El flujo:

```
        ┌──────────────────── al GO (árbitro) o flanco STOP→GO ────────────────────┐
        ▼                                                                          
  inicio_retroceder ──ve la línea (o safety 50s)──► inicio_avanzar ──(400ms)──► moverce_derecha
        (va atrás)                                   (avanza a ciegas, lento)             │
                                                                                          ▼
     ── PATRULLA (ir y venir, siguiendo la pelota) ──────────────────────────────────────
        moverce_derecha ──borde──► salir_linea_izq ──(450ms ciego)──► moverce_izquierda
             ▲                                                              │
             └────────── salir_linea_der ◄──(450ms ciego)──── borde ◄───────┘
        (entre moverce_derecha ↔ moverce_izquierda también se salta DIRECTO según el lado de la pelota)
     ─────────────────────────────────────────────────────────────────────────────────────
                                  │ pelota CERCA + AL FRENTE (desde cualquier moverce_*)
                                  ▼
        PATEANDO_pausa_inicial ─(200ms)─► ALINEAR_arco_opp ─(apuntó/timeout)─► PATEANDO_adelante
                                          (gira al arco rival)                  (golpe 450ms)
                                                                                     │
        moverce_derecha ◄─(1000ms)─ avanzar_despues_de_patear ◄─(línea/4s)─ PATEANDO_atras ◄─(1000ms)─ PATEANDO_pausa
```

### Fase INICIO (homing al arco) — `inicio_retroceder`, `inicio_avanzar`

Idea: al arrancar, **acomodarse en el arco** antes de patrullar.
- **`inicio_retroceder`** (`amix_fsm.cpp:133-142`): `retroceder_inicio()` (va hacia atrás) hasta que
  `linea()` detecta el blanco del área. Red de seguridad: si nunca la ve, sale igual tras
  `AMIX_T_INICIO_RETRO_SAFETY` (**hoy 50 s temporal**, para observar; baja a ~4 s cuando ande). → `inicio_avanzar`.
- **`inicio_avanzar`**: `avanzar_inicio()` (avance lento, recto al frente) para SALIR de la línea del
  área. **FIX 2026-06-21:** ya no termina por reloj fijo — sale cuando cumplió el impulso mínimo
  (`AMIX_T_INICIO_AVANCE_MIN`=400 ms) **Y** ya no pisa la línea (`!linea()`), o por tope de seguridad
  (`AMIX_T_INICIO_AVANCE_SAFETY`=1200 ms). Así no arranca a patrullar pisando el área. → `moverce_derecha`.

### Fase PATRULLA — `moverce_derecha`, `moverce_izquierda`, `salir_linea_der`, `salir_linea_izq`

Esta es la fase principal: el arquero **va y viene** delante del arco, **siguiendo la pelota** y
**rebotando** en los bordes. `moverce_derecha` y `moverce_izquierda` son **espejo** (una strafe a la derecha
con `adproporcional`, la otra a la izquierda con `aiproporcional`). En cada una se decide **dos cosas**:

**(1) Qué hacer con la pelota** (`amix_fsm.cpp:156-172`, idéntico espejo en :197-213):
- **¿Cerca y al frente?** (`ball_para_despejar()` = distancia ≤ 250 mm **y** |ángulo| ≤ 30°) → arranca el
  **DESPEJE** (va a `PATEANDO_pausa_inicial`).
- **¿Recién salió de un borde?** (ventana "commit") → sigue patrullando **este** lado, no vuelve.
- **¿Pelota desviada?** (no alineada, |ángulo| > 8°) → **sigue la pelota**: se va al estado del lado donde
  está (`ball_a_la_derecha()` → derecha o izquierda), con más fuerza (`pd=1.5`).
- **¿Alineada pero lejos?** → `parar()` (mantiene posición; banda muerta angosta).
- **¿Sin pelota?** → patrulla base (`pd=1.0`).

**(2) Cuándo rebotar en el borde** (`:173-191`, espejo en :214-231) — **acá está el cambio reciente de
Virginia**:
- **Por DEFAULT (`AMIX_PATRULLA_POR_ARCO = true`):** el borde se detecta por el **ÁNGULO del arco propio**
  (la cámara trasera lo ve por el snapshot). Cuando el arco propio se desvía de "directamente atrás" (180°)
  más que `AMIX_TOL_ARCO_OWN_DEG` (30°), **llegó al borde** → rebota al otro lado. **Si la cámara NO ve el
  arco propio**, cae al **fallback de LÍNEA** (`linea()`). Helpers: `rear_goal_dev()`, `borde_arco_der/izq()`
  (`:93-103`).
- **Con `-DARQMIX_PATRULLA_LINEA`:** vuelve al esquema viejo — rebota **solo** por la línea.
- El rebote va a `salir_linea_izq` (desde la derecha) o `salir_linea_der` (desde la izquierda), y está
  **gateado por el commit** (no rebota dos veces seguidas).

**`salir_linea_der` / `salir_linea_izq`** (`:235-252`): salen al lado opuesto **a ciegas** (sin leer
sensores) y con fuerza (`pd=1.9`) durante `AMIX_T_SALIR_LINEA` (450 ms), y arman la ventana **commit**
(`AMIX_T_PATRULLA_COMMIT` = 1 s) para **no volver enseguida** a la línea que dejaron. Después vuelven a
`moverce_*` del lado nuevo.

> 🧭 **Por qué tanto cuidado con el rebote:** sin el "commit", el arquero se quedaba **enganchado**
> oscilando en la línea (la tocaba, rebotaba, la volvía a tocar). El commit + la salida a ciegas lo despegan.

### Fase DESPEJE — la secuencia de 5 pasos

Cuando la pelota está cerca y al frente, se dispara esta secuencia (cada paso es un estado con un timer):

1. **`PATEANDO_pausa_inicial`** (`:256-262`, 200 ms): `parar()` para dejar pasar la inercia lateral.
2. **`ALINEAR_arco_opp`** (`:265-283`, **nuevo, pedido Gustavo**): **gira** (`girar()`) para apuntar el
   frente al **arco rival** (`goal_opp`), así el golpe va dirigido. Sale cuando: ya está alineado
   (|ángulo arco| ≤ 12°), **o** no ve el arco rival, **o** se acaba el tiempo (`AMIX_T_ALINEAR_OPP` = 300 ms,
   conservador) → en esos casos patea igual (recto, como el 2025). **No se cuelga.**
3. **`PATEANDO_adelante`** (`:286-293`, 450 ms): `avanzar_patear()` — el golpe con rampa, recto al frente
   (que ahora apunta al arco rival).
4. **`PATEANDO_pausa`** (`:296-302`, 1000 ms): `parar()`.
5. **`PATEANDO_atras`** (`:305-315`): `patear_atras()` (retrocede) hasta volver a ver la línea **o** un
   safety de 4 s. → `avanzar_despues_de_patear`.
6. **`avanzar_despues_de_patear`** (`:318-324`, 1000 ms): `avanzar()` para reposicionarse, y **cierra el
   ciclo** volviendo a `moverce_derecha` (patrulla).

---

## 6. Las perillas (qué se tunea sin tocar la lógica)

Todo en `amix_config.h`. Las más importantes:

| Constante | Valor hoy | Qué controla |
|---|---|---|
| `AMIX_TOL_CENTRADO_DEG` | 8° | banda muerta de la pelota: dentro = quieto, fuera = la sigue |
| `AMIX_TOL_KICK_DEG` | 30° | ángulo máximo de la pelota para despejar |
| `AMIX_TOL_CERCANIA_MM` | 250 | distancia para despejar (**knob principal**, escala sin calibrar) |
| `AMIX_TOL_ARCO_OPP_DEG` | 12° | cuán fino apunta al arco rival antes de patear |
| `AMIX_TOL_ARCO_OWN_DEG` | 30° | ancho de la patrulla (desvío del arco propio = borde) |
| `AMIX_T_ALINEAR_OPP` | 300 ms | tope de giro al apuntar (conservador) |
| `AMIX_T_INICIO_RETRO_SAFETY` | **50 s (temporal)** | safety del homing — **bajar a ~4 s** |
| `AMIX_PD_BASE / BALL / SALIR` | 1.0 / 1.5 / 1.9 | fuerza de la patrulla / siguiendo pelota / saliendo de línea |
| `AMIX_KICK_VEL_FINAL` | 180 | potencia pico del golpe |

**Perillas de compilación (flags `-D…`)** — cambian comportamiento sin editar código:

| Flag | Efecto | Estado |
|---|---|---|
| `ARQMIX_HEADING_SIGN_OLD` | rumbo a +1 (viejo) | **NO usar**: el −1 default está validado |
| `ARQMIX_FLIP_INICIO_RETRO` | invierte el sentido del retroceso del homing | si va para adelante al GO |
| `ARQMIX_FLIP_GIRO_ALINEAR` | invierte el giro de alineación al arco | si gira al lado contrario (env `_giroflip`) |
| `ARQMIX_PATRULLA_LINEA` | patrulla rebota por LÍNEA (esquema viejo) | fallback si el arco propio falla |
| `ARQMIX_FLIP_ARCO_OWN` | invierte el lado del borde del arco propio | si rebota en el borde equivocado |
| `ARQMIX_HEADING_OTOS` | rumbo del OTOS (DOWN) en vez del TOP | A/B de banco |

---

## 7. Cómo modificar la FSM (guía práctica)

**El patrón de un estado** (mirá cualquiera en `amix_fsm.cpp`):
```cpp
case Estado::mi_estado:
    hacer_algo();                                  // 1) acción (mover motores)
    if (condicion_de_salida) {                     // 2) ¿cambio de estado?
        millis_inicio_estado = millis();           //    resetear el timer al entrar al próximo
        estado = Estado::otro_estado;
    }
    break;
```

**Para agregar un estado nuevo:**
1. Agregalo al `enum Estado` en `amix_fsm.h` (y actualizá el comentario del conteo).
2. Agregá su `case` en el `switch` de `amix_fsm_tick`.
3. Si necesita una primitiva de movimiento nueva, agregala en `amix_motors.{h,cpp}`.
4. Si necesita un umbral/tiempo, ponelo en `amix_config.h` (no hardcodees números en la FSM).

**Trampas a evitar (lecciones que ya costaron banco):**
- **El timer:** `millis_inicio_estado` se debe **resetear al ENTRAR** a un estado con timeout, si no el
  timeout mide desde antes.
- **`parar()` resetea la rampa del golpe.** Si metés un estado entre el giro y el golpe, ojo con eso.
- **Los signos NO están confirmados:** lado de la pelota (`ball_a_la_derecha`), giro de alineación, rebote
  del arco propio. Hay perillas para invertirlos en banco — usalas, no toques la lógica.
- **La corrección de rumbo necesita `heading_valid`.** Sin rumbo del TOP, el arquero no endereza y deriva.
- **El árbitro manda:** el tick arranca con `if (!match_running) { parar(); return; }`. Y en cada flanco
  STOP→GO el FSM **se reinicia al homing**. Cualquier estado nuevo debe sobrevivir a eso.

---

## 8. Lo que NO está validado (honestidad para no confiarse)

- **Nada testeado en hardware.** Todos los signos físicos (motores, lado de pelota, giro, rebote) y los
  umbrales en mm se confirman en banco. Es la regla #1 del repo: lo cierra el equipo, no Claude.
- **`goal_own` (arco propio) no está validado:** la patrulla por arco depende de la cámara **trasera** y de
  la calibración LAB. Riesgo aceptado por Virginia, con fallback a línea. Si el arco propio falla y tampoco
  hay línea, el arquero podría irse del arco.
- **El safety del homing está en 50 s** (temporal, para observar). Hay que bajarlo a ~4 s.
- **El comentario "10 estados"** del `amix_fsm.h` quedó viejo (son 12). Conviene corregirlo cuando se toque.
- **La línea llega recortada** (present/ángulo/conteo): no hay `cross_track` ni estado por-sensor. Para un
  seguidor de línea fino falta exponer más datos (ver el doc de seguidor de línea).

---

## 9. Referencias

- **El código:** este directorio `src/arqueromix/`.
- **Log de banco + decisiones** (qué se cambió y por qué, cronológico): [`DOCUMENTACION.md`](DOCUMENTACION.md).
- **Selección de arco / detección de color:** `docs/firmware/DETECCION-DE-COLOR-COMO-FUNCIONA.md`.
- **Qué info de línea sube de la DOWN:** `research/in-progress/2026-06-21-arquero-seguidor-linea-cross-track-que-sube-down.md`.
- **El arquero 2025 original (la base del port):** `software/_deprecated-2025/robot-arquero/definitivo-arquero_6-9-2026` + `docs/internal/ANALISIS-FIEL-ARQUERO-2025.md`.
- **Hermano delantero (mismo patrón):** `src/centralmix/`.

---

*Documento explicativo, escrito leyendo el código real al 2026-06-21 (commit `2558427`) y verificado con
pasada adversarial. Apoyo de Claude; atribución según `AI-INSTRUCTIONS.md`.*
