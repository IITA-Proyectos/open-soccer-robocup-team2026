---
title: "Control del arquero moviéndose de lado — el lazo, las frecuencias y las demoras"
date: 2026-06-14
author: "Claude (Anthropic - Claude Opus 4.8 1M) — coach"
requested-by: "Virginia Viollaz (@gviollaz)"
status: vivo
tipo: analisis
robot: arquero
area: control
tags: [control, analisis, arquero, alta]
scope: src/central/strategy.cpp · src/shared/pids.h · src/shared/pfm_heading.h · src/top/sensors_imu.cpp · src/down/down_model.cpp
---

# Control del arquero moviéndose de lado — el lazo, las frecuencias y las demoras

> **Para qué es este documento.** El arquero, cuando se mueve de lado para tapar
> el arco, **oscila** (hace una "medialuna" en vez de ir derecho). Vos sospechás
> que hay **demoras en el lazo de control**. Este documento traza el lazo
> completo a través de las 3 placas, pone los números reales (frecuencias,
> fórmulas, latencias) y explica **dónde están las demoras y por qué generan la
> oscilación**. Todos los números salen del código vivo, con su `archivo:línea`.
>
> **Conclusión adelantada (el titular):** el lazo de control corre a **100 veces
> por segundo**, pero el dato de rumbo (el giroscopio BNO) solo se actualiza
> **20 veces por segundo** y llega con **35 a 70 milésimas de atraso**. Y ese
> tope de 20 Hz es **software puesto al pedo**: el BNO que el robot usa de verdad
> está **solo en su propio bus** (`Wire2`, sin los sensores de distancia), así que
> **podría leerse a 100 Hz sin riesgo** y bajar el atraso a la mitad. Un lazo que
> corrige 5 veces más rápido de lo que puede "ver", **oscila** — y el atraso es la
> causa dominante. Tu sospecha es correcta. (Hay un segundo factor menor: el motor
> tiene **zona muerta** abajo — necesita un mínimo de potencia para arrancar. Pero
> **NO es "todo o nada"**: arriba de ese mínimo el PWM controla la potencia de
> forma casi continua. Ver punto 7.)

---

## 1. Qué hace el arquero cuando se mueve de lado (la esencia)

El arquero hace dos cosas **al mismo tiempo**:

1. **Se desplaza de costado** (de lado a lado, como un cangrejo — en inglés
   *strafe*) para tapar la boca del arco. Esto es la velocidad lateral (`vx`).
2. **Trata de mantener el frente mirando al arco rival** (rumbo 0°). Esto es la
   rotación (`omega`), y **es el lazo que tiene el PID**.

El problema vive en la mezcla de esas dos: cuando intenta corregir el rumbo
*mientras* se mueve de lado, con un dato de rumbo viejo, sobre-corrige, se pasa,
corrige al revés… y sale la medialuna.

> **Dato de diseño importante.** La patrulla "completa" (`PATROL`, la versión
> v3.3) ya aprendió esto en banco y por eso **mueve con `omega = 0`** (no corrige
> el rumbo mientras se desplaza) y solo se endereza **parada**, con pulsos. La
> versión de práctica (`GK_SIMPLE_STRAFE`) sí intenta corregir el rumbo en
> movimiento, con un control especial (PFM) — ahí es donde aparece la medialuna.
> Las dos están en `goalkeeper_tick()` de
> [`strategy.cpp`](../../software/teensy/Soccer%202026/src/central/strategy.cpp).

---

## 2. El lazo primario de control (diagrama de bloques)

Este es el camino que recorre un dato desde que el robot gira físicamente hasta
que los motores reaccionan. **El número arriba de cada flecha es a qué frecuencia
ocurre; el número abajo es cuánto atraso agrega.**

```
   ┌─────────────────── PLACA TOP (Teensy 4.0) ───────────────────┐
   │                                                              │
   │   El robot gira  ──►  BNO055 (en Wire2,   ──►  WorldSnapshot  │
   │                       SOLO, sin ToF)           arma a 100 Hz  │
   │                       lee a 20 Hz ⚠ tope       ⏱ +0–10 ms     │
   │                       de software innecesario                 │
   │                       ⏱ atraso 25–50 ms (podría ser 5–10)     │
   └──────────────────────────────┬───────────────────────────────┘
                                   │ UART Serial @ 230400 baud
                                   │ ⏱ +~2 ms (31 bytes)
                                   ▼
   ┌──────────────── PLACA CENTRAL (Teensy 4.1) ──────────────────┐
   │                                                              │
   │   world_model  ──►  strategy_tick()  ──►  PID / PFM  ──►  motores
   │   (guarda el       corre a 100 Hz       calcula omega    aplica PWM
   │    rumbo)          ⏱ +0–10 ms           (las fórmulas)   a 100 Hz
   │                                                          sin rampa
   └──────────────────────────────────────────────────────────────┘

   ⏱ ATRASO TOTAL del rumbo, de la rueda al PID:
       HOY (BNO aislado, frenado a 20 Hz)  ≈ 35–70 ms
       SI SE LEE A 100 Hz                  ≈ 15–30 ms  ← fix de software, P0
```

La línea blanca (para frenar en el borde) viaja por un camino **distinto y más
rápido**: la placa de ABAJO la manda a 200 Hz y llega en ~5–10 ms (ver punto 5).
El cuello de botella **no es la línea ni la UART**: es el **rumbo del BNO**.

---

## 3. Frecuencias reales — la tabla clave

| Qué | Frecuencia real | Atraso que agrega | Dónde en el código |
|---|---|---|---|
| **Lazo primario** (`strategy_tick` + motores) | **100 Hz** (cada 10 ms) | 0–10 ms | `main_central.cpp:320` |
| **Lectura del BNO (rumbo)** | **20 Hz** (cada 50 ms) ⚠️ | 25–50 ms | `sensors_imu.cpp:103,365` |
| Envío del WorldSnapshot TOP→CENTRAL | 100 Hz (cada 10 ms) | 0–10 ms | `main_top.cpp:338` |
| Transporte UART (rumbo) | 230400 baud, 31 bytes | ~2 ms | `comm_central.cpp:26` |
| Lectura cruda de la línea (ABAJO) | 1000 Hz (cada 1 ms) | <1 ms | `config_down.h:138` |
| Envío de la línea ABAJO→CENTRAL | 200 Hz (cada 5 ms) | <5 ms | `main_down.cpp:42` |
| Aplicación de motores | 100 Hz, **sin rampa ni filtro** | 0 ms | `main_central.cpp:332` |
| "Snapshot viejo" (corta motores) | corta a 500 ms | — | `world_model.cpp:21` |

**La fila amarilla es la clave.** El lazo decide 100 veces por segundo, pero el
rumbo que usa solo cambia 20 veces por segundo. Es decir: **4 de cada 5 veces, el
PID corrige usando exactamente el mismo número de rumbo que ya usó** — un dato que
no se actualizó.

> **¿Por qué el BNO se lee a 20 Hz si el tick es de 100 Hz? (revisado contra el
> código — punto importante de Gustavo/Virginia).** El tope de 20 Hz es un parche
> que se puso por la **contención** del BNO que comparte cable con los sensores
> de distancia (ToF) en el bus `Wire`: leerlo muy seguido ahí corrompe el dato y
> **congela el rumbo**. **PERO el BNO que el robot usa de verdad no es ese.** El
> env que se flashea (`top_robot2_pri`) corre con `-DTOP_BNO_PRIMARY_ONLY`: el
> secundario (el del bus compartido) queda **apagado**, y el rumbo sale del **BNO
> primario, que vive SOLO en su propio bus `Wire2` (pines 24/25), sin ToF, sin
> contención** (`sensors_imu.cpp:13-17, 54, 271-294`).
>
> **El problema:** el tope de 50 ms está en un control **global** que frena TODO
> el `sensors_imu_tick()` (`sensors_imu.cpp:365`), así que **también frena al BNO
> primario aislado, que no lo necesita.** El propio comentario del código lo dice
> ("el secundario de Wire mantiene este band-aid" → el primario no debería),
> pero el código nunca separó los dos casos. Resultado: **estamos leyendo a 20 Hz
> un sensor que podría leerse a 100 Hz sin riesgo.** El BNO055 en modo fusión
> entrega rumbo a 100 Hz, así que leer cada 10 ms da dato fresco siempre. Esto es
> media latencia regalada — y se saca sin tocar hardware (ver P0 en el punto 8).

---

## 4. Las fórmulas del control

Hay **dos** controladores de rumbo distintos según la versión del arquero.

### 4.a. El PID clásico (HeadingPID) — `src/shared/pids.h`

Es el que usa la patrulla completa (`PATROL`) para enderezarse parada, y el
"despeje" (`CLEAR`).

```
error      = rumbo_objetivo − rumbo_actual      (envuelto a ±180°)
integral  += error × dt        (con tope ±50, y "anti-windup")
derivada   = (error − error_anterior) / dt
salida     = kp·error + ki·integral + kd·derivada
salida     = recortar(salida, −327, +327)  °/s
```

- **Ganancias por defecto:** `kp = 3.0`, `ki = 0.05`, `kd = 0.5` (`pids.h:48-66`).
- **`dt`** (el "paso de tiempo") usa el tiempo real entre llamadas, pero si no
  hay dato previo usa **10 ms** fijo, y lo recorta a un rango de 1 a 100 ms
  (`pids.cpp:62-69`).

> **Palabras difíciles:**
> - **kp/ki/kd** = las tres "perillas" del PID. **P** (proporcional) reacciona
>   al error de ahora; **I** (integral) acumula el error viejo y sirve para
>   vencer desvíos constantes; **D** (derivada) reacciona a qué tan rápido cambia
>   el error (frena el sobrepaso).
> - **anti-windup** = un freno para que la parte **I** no se "cargue" de más
>   cuando el motor ya está al máximo (si no, después da un latigazo).
> - **recortar a ±327°/s** = no se permite pedir más giro que eso (es un límite
>   técnico: multiplicado por 100 tiene que entrar en un número de 16 bits; con
>   360 se desbordaba e invertía el giro — fue un bug real de 2026-06-03).

### 4.b. El control PFM — `src/shared/pfm_heading.h`

Es el que usa la versión de práctica (`GK_SIMPLE_STRAFE`) para corregir el rumbo
**mientras** se mueve de lado. Está pensado para la **zona muerta** del motor: las
correcciones de giro MUY chicas (más chicas que el mínimo que arranca el motor)
caen en la zona muerta. El PFM las hace igual, "a pulsos".

```
u      = kp·error + integral                    (un PI normal)
duty   = recortar(u / 100°/s, −1, +1)           ← "qué fracción del tiempo girar"
on?    = (tiempo en la ventana) < |duty| × 160 ms
salida = ±100°/s  si on, 0 si off               ← gira al MÍNIMO físico, o nada
deadband: si |error| < 5°  →  salida 0          (no perseguir ruido)
```

- **Configuración por defecto:** `kp = 2.0`, `ki = 0.4`, deadband `5°`, giro fijo
  `100°/s`, ventana `160 ms` (`pfm_heading.h:44-46`).

> **La idea del PFM (importante) — y un matiz que corregimos el 2026-06-14:** el
> motor SÍ controla la potencia de forma continua con el PWM. Lo que NO puede es
> arrancar con **muy poca** potencia: abajo de un mínimo (la "zona muerta") la
> fricción lo frena y no se mueve. Para una corrección de giro **más grande que
> ese mínimo**, el motor la hace continua y fina, sin problema. El PFM es para las
> correcciones que caen **por debajo** del mínimo: en vez de pedir un giro
> imposiblemente chico, pide el giro **mínimo, pero solo una fracción del tiempo**
> (prende el giro unos 30 ms de cada ventana de 160 ms y lo apaga el resto); el
> **promedio** en el tiempo da la corrección fina. Es como regular un horno
> prendiéndolo y apagándolo. **Pero no es la única forma:** como el control es
> continuo arriba del mínimo, un PD continuo (con el término de amortiguación que
> sumamos, ver punto 8) también sirve cuando el robot ya se está moviendo.

### 4.c. El PID lateral (LateralPID)

Mantiene la distancia a la línea (solo en la patrulla completa). `kp = 50`,
`ki = 5`, `kd = 10`, salida recortada a ±800 mm/s (`pids.h:96-110`). No es el que
genera la oscilación de rumbo, pero está en el mismo lazo.

---

## 5. Cómo se mide si "toca línea blanca"

La placa de ABAJO tiene un **anillo de 32 sensores ópticos** (miran el piso).
Resumen del camino (`down_model.cpp`, `line_geometry.cpp`):

- **Lee los 32 a 1000 Hz** (cada 1 ms) (`config_down.h:138`).
- **"Ve blanco" cada sensor** si su lectura supera un umbral = el punto medio
  entre "alfombra verde" (~100–300) y "blanco" (~600–900), ajustado por
  calibración (`line_calib.cpp:4-7`). Para no titilar usa **histéresis** (tiene
  que pasarse bien del umbral) y un **filtro espacial** (exige que un sensor
  vecino también vea blanco, así un sensor sucio solo no dispara).
- **Ángulo de la línea** = el "centro de masa" de los sensores que ven blanco,
  con `atan2`. Convención: **0° = adelante del robot, y el sentido horario es
  positivo** (`line_geometry.cpp:71-72`).
- **"Salida inminente" (`imminent_exit`)** = hay línea **y** la están viendo
  **6 o más sensores** a la vez (`comm_central.cpp:38`). Esa es la señal de
  "me estoy por salir" que dispara el freno de borde.
- **Llega a la CENTRAL** en un mensaje de 16 bytes, mandado a **200 Hz** por
  UART; el atraso de punta a punta es **~5–10 ms** en el peor caso
  (`main_down.cpp:42`, `down_tx.cpp`).

**La línea es rápida y no es el problema.** Llega 5–10× más fresca que el rumbo.

---

## 6. El problema: dónde están las demoras (el presupuesto de latencia)

Sumemos el atraso del **rumbo** (el dato con el que el PID decide cuánto girar),
desde que el robot gira físicamente hasta que el PID lo usa. Lo importante: con el
firmware de hoy (`top_robot2_pri`, BNO primario solo en `Wire2`) **NO hay
contención** — el atraso lo manda el **tope de 20 Hz aplicado al pedo** a ese
sensor aislado.

| Etapa | Hoy (BNO aislado, leído a 20 Hz) | Si se lee a 100 Hz |
|---|---|---|
| El BNO se lee recién cada 50 ms / cada 10 ms | 25–50 ms | 5–10 ms |
| Esperar a que el WorldSnapshot salga (100 Hz) | 0–10 ms | 0–10 ms |
| Viaje por UART (31 bytes @ 230400) | ~2 ms | ~2 ms |
| Esperar al próximo `strategy_tick` (100 Hz) | 0–10 ms | 0–10 ms |
| **TOTAL (atraso del rumbo en el lazo)** | **≈ 35–70 ms** | **≈ 15–30 ms** |

> **Nota honesta (corrección de la 1ª versión):** los **250–500 ms** que dije
> antes eran el régimen VIEJO/degradado (BNO compartido con los ToF, o el
> secundario congelado arrastrando al primario). El env `top_robot2_pri` **ya
> evita ese régimen** (apaga el secundario, usa solo el primario aislado). El
> atraso de HOY es ~35–70 ms, y **la mitad es el tope de 20 Hz que se puede sacar
> sin tocar hardware.**

Comparalo con lo rápido que gira el robot: la deriva parásita medida del strafe es
~**80°/s** y una corrección puede ir más rápido. A 80°/s, en **70 ms el robot rota
~5–6°**; a 150°/s, ~10°. No parece mucho, pero pasa en CADA ciclo: cuando el lazo
se entera de ese giro, ya lo arrastró, corrige, y para cuando ese comando hace
efecto el robot ya se pasó para el otro lado. Eso, repetido, **es la medialuna**.
Bajar el rumbo a ~15–30 ms (leer a 100 Hz) le da al lazo la mitad del atraso para
pelear: se entera antes, corrige menos de más.

---

## 7. Por qué eso genera la oscilación (el diagnóstico)

La oscilación tiene **dos causas que se potencian**. Las dos son reales; arreglar
una sola no alcanza.

### Causa A — El lazo "ve" más lento de lo que actúa (tu sospecha: las demoras)

- El lazo corre a 100 Hz pero el rumbo se actualiza a 20 Hz → **4 de cada 5
  ticks corrigen sobre un dato que no cambió**. Y ese 20 Hz es un **tope de
  software puesto al pedo** sobre el BNO bueno, que está solo en su bus y podría
  leerse a 100 Hz (ver punto 3 y P0).
- Encima ese dato llega con 35–70 ms de atraso.
- En control, un lazo con **atraso grande respecto de lo que tarda en actuar** se
  vuelve inestable: corrige tarde, se pasa, corrige al revés. No importa cuánto
  toques las ganancias — el atraso es el que manda.
- **Detalle fino que empeora la parte D:** la derivada se calcula como
  `(error − error_anterior)/dt`. Como el rumbo solo cambia cada 50 ms, 4 de cada
  5 ticks la derivada da **0**, y en el tick que el rumbo salta de golpe da un
  **pico**. Con `kd = 0.5` ese pico es un tirón de giro. El problema acá es que la
  **señal de rumbo llega a escalones** (no el motor): derivar una señal escalonada
  mete ruido. (Por eso el módulo nuevo `heading_rate.h` deriva solo en muestras
  frescas, no tick a tick — ver punto 8.)

### Causa B — Zona muerta del motor (control imperfecto, NO "sin control")

> **Corrección 2026-06-14 (tenías razón).** La 1ª versión de este documento decía
> que el motor era "todo o nada / cuantizado". **Es falso.** El código mapea el
> comando a PWM de forma **continua y proporcional** (`wheel_speed_to_pwm`,
> `analogWrite`), y el bloque `CENTRAL_FLOOR_SCALE` que usa el arquero dice
> textual: *"las correcciones finas se conservan… sin bang-bang"*
> (`motors_zircon.cpp:154-174`). El motor tiene **control continuo de potencia**.

- Lo único imperfecto es **abajo**: hay una **zona muerta** — el motor necesita un
  mínimo de PWM (los pisos `{70, 70, 107}`) para vencer la fricción y arrancar.
  Abajo de eso no se mueve. El firmware sube los comandos chicos a ese piso (o, con
  `FLOOR_SCALE`, escala TODO el comando conservando la dirección y las correcciones
  finas).
- **Cuándo molesta de verdad:** (a) arrancar de quieto (por eso existe el
  *kickstart*), y (b) correcciones de rumbo **más chicas que el piso**, que se
  pierden o saltan al mínimo. Pero **una vez que el robot se mueve, las ruedas ya
  están arriba del piso y las correcciones de rumbo son continuas y finas** — eso
  ya lo resuelve `FLOOR_SCALE`.
- O sea: la zona muerta es un factor **secundario** (afina), no la causa de la
  oscilación. **La causa dominante es la latencia (Causa A).** Con control continuo
  arriba del piso, un buen PD (latencia baja + amortiguación) controla bien.
- El PFM (punto 4.b) sigue siendo útil para las correcciones que caen por debajo
  del piso, pero **no es obligatorio**: con la latencia baja, un PD continuo
  también anda.

> **En una frase:** el robot **corrige tarde** (Causa A — el problema grande) y la
> **zona muerta** le afina mal las correcciones más chicas (Causa B — secundario).
> El motor NO está "sin control": tiene control continuo de potencia con un mínimo
> para arrancar. Bajá la latencia y sumá amortiguación, y el mismo motor controla
> bien.

---

## 8. Qué se puede hacer — temas a analizar (con prioridad y plan de banco)

> Formato del coach: cada tema con qué pasa si **no** se hace, qué se arriesga al
> hacerlo, y una estimación honesta. **Nada de esto lo puede cerrar Claude: hay
> que probarlo en el robot.** Todos se miden con la **caja negra v1.2** (grabar
> rumbo, omega y tiempo, y mirar el período y la amplitud de la oscilación).

### P0 — Leer el BNO primario a 100 Hz (¡el hardware ya está, es software!)

- **Qué es:** el BNO que usa el robot **ya está en su bus propio** (`Wire2`, sin
  ToF) y el secundario está apagado en el env de producción (`-DTOP_BNO_PRIMARY_ONLY`).
  Lo único que falta es **dejar de frenarlo a 20 Hz**: el tope `BNO_READ_INTERVAL_MS
  = 50` (`sensors_imu.cpp:103,365`) es un control **global** que se aplica también
  al primario aislado, que no lo necesita. Bajarlo a ~10 ms (100 Hz) **en el build
  `*_pri`** (donde el secundario, el del bus compartido, está apagado) corta el
  atraso del rumbo de ~35–70 ms a ~15–30 ms. Mejora opcional: subir `Wire2` a
  400 kHz (`sensors_imu.cpp:251`), porque ahí no hay ToF que obliguen a 100 kHz →
  cada lectura del BNO es ~4× más rápida.
- **risk-no-fix:** la oscilación NO se va con tuneo solo — el atraso es media
  causa. Seguís peleando síntomas con la mano atada.
- **risk-fix:** **bajo, pero hay que respetar la separación.** El tope de 20 Hz
  SÍ es necesario para el BNO secundario (el del bus compartido con los ToF): si
  alguien baja el tope global sin distinguir, en un build con el secundario
  encendido (`top_robot2`, sin `_pri`) **vuelve la congestión y el freeze**. Por
  eso el cambio debe ser **por-sensor** (rápido el de `Wire2`, lento el de `Wire`)
  o **gateado al build `*_pri`**. Además, leer más seguido cuesta tiempo de loop
  (~1,5 ms por lectura a 100 kHz) — medir que el loop del TOP siga holgado.
- **tiempo:** el código es chico (separar el gate por bus). **Banco obligatorio:**
  medir la cadencia real del rumbo con la caja negra antes/después. ~medio día.

### P1 — No corregir el rumbo mientras se mueve de lado (mover–parar–corregir)

- **Qué es:** lo que ya hace la patrulla completa (`PATROL`): desplazarse con
  `omega = 0` y enderezarse **parado**, en pulsos. Llevar esa misma idea a la
  versión simple, en vez de corregir en continuo con el PFM durante el strafe.
- **risk-no-fix:** mezclar giro + strafe con estos pisos de PWM degenera (las
  ruedas delanteras van chicas y cualquier giro las desborda) → medialuna.
- **risk-fix:** el movimiento se ve más "a tirones" (avanza, se endereza, avanza);
  hay que tunear los tramos para que no se note feo.
- **tiempo:** 1 sesión de banco (la lógica ya existe en `PATROL`, es portarla).

### P1 — Que el lazo NO recompute sobre dato viejo (lazo por evento)

- **Qué es:** hoy el PFM se llama con un paso de tiempo **fijo de 10 ms**
  (`strategy.cpp:1184`) aunque el rumbo no haya cambiado. Opción: recalcular la
  corrección **solo cuando llega un rumbo nuevo** (o usar el `dt` real). Así el
  lazo deja de "inventar" 4 correcciones falsas por cada dato real.
- **risk-no-fix:** el PFM integra y reparte pulsos sobre datos repetidos → ruido.
- **risk-fix:** cambia el ritmo del PFM; hay que re-afinar la ventana.
- **tiempo:** chico de código, pero **necesita banco** para re-afinar.

### P2 — Afinar el PFM por titración (skill control-pid-zona-muerta)

- **Qué es:** con la latencia ya baja (P0), afinar `deadband` (5→8° si tiembla),
  `ki` (0.4→0.8 si deriva sin corregir) y la ventana, midiendo con caja negra
  hasta `|error| < 10°` sostenido.
- **tiempo:** 2–3 corridas de banco.

### Plan de prueba en hardware real (obligatorio)

1. **Medir la planta primero** (lazo abierto): mover de lado sin corregir rumbo y
   grabar cuánto deriva solo (eso da la perturbación a vencer) y cada cuánto
   cambia el rumbo del BNO de verdad (confirmar los 20 Hz / 50 ms).
2. Aplicar **P0** y volver a medir el atraso del rumbo (debería caer a ~10–15 ms).
3. Aplicar **P1** (mover–parar–corregir o lazo por evento) y grabar período y
   amplitud de la oscilación con la caja negra.
4. Criterio de cierre: el arquero recorre su arco de lado a lado con `|error de
   rumbo| < 10°` sostenido y **sin medialuna visible**. Solo el equipo, con el
   robot, cierra esto.

---

## 9. Implementación (2026-06-14) — qué quedó codeado

Las dos mejoras ya están **escritas y gateadas (apagadas por default → el binario
de competencia es byte-idéntico)**. Falta SOLO el banco (Claude no cierra tareas
de hardware).

| Mejora | Qué se cambió | Flag / env para probar |
|---|---|---|
| **P0 — menos latencia** | El BNO primario aislado (Wire2) se lee a **100 Hz** en vez de 20 (`sensors_imu.cpp`, `BNO_READ_INTERVAL_MS`). Un `#error` impide usarlo sin `TOP_BNO_PRIMARY_ONLY` (no rompe el secundario del bus compartido). | `-DTOP_BNO_FAST` · env **`top_robot2_pri_fastbno`** |
| **P1 — amortiguación (D)** | Módulo puro nuevo `src/shared/heading_rate.h` (velocidad de giro de muestras frescas) + término `kd_rate` en `pfm_heading.h`, cableado en el strafe del arquero (`strategy.cpp`, `GK_PFM_KD_RATE=0,30`). | `-DGK_PFM_RATE_DAMP` · env **`central_robot2_arquero_strafe_cam_ratedamp`** |

- **Por qué gateado y no por default:** regla del repo — una mejora de control
  **se valida en banco antes** de entrar al binario de partido. Con los flags
  apagados, todo queda exactamente como hoy.
- **Tests host nuevos:** `test_heading_rate` (8) + 3 tests del término de
  amortiguación en `test_pfm_heading`. **Suites afectados: 19/19 verdes**
  (`bash scripts/run-host-tests.sh heading`).
- **Combinar las dos:** flashear `top_robot2_pri_fastbno` (TOP) **y**
  `central_robot2_arquero_strafe_cam_ratedamp` (CENTRAL) = el ataque completo a la
  oscilación (menos atraso + amortiguación).

### Plan de banco para estas dos (además del general de arriba)

1. **Caja negra**: grabar rumbo, omega y tiempo en los 3 casos — hoy
   (`_strafe_cam_bb`), solo P0 (`_fastbno`), P0+P1 (`_fastbno` + `_ratedamp`).
2. **P0**: confirmar con el panel del TOP que el rumbo trackea el giro a mano sin
   congelarse y que el loop del TOP sigue holgado (no se cae el snapshot).
3. **P1**: titrar `GK_PFM_KD_RATE` (arrancar 0,30; subir si sigue sobrepasando,
   bajar si se vuelve lento/tieso) hasta `|error de rumbo| < 10°` sostenido y sin
   medialuna visible.
4. **Criterio de cierre**: el arquero recorre su arco de lado a lado derecho. Solo
   el equipo, con el robot, cierra esto.

---

### Para profundizar

- La planta medida (pisos, deriva parásita, regímenes): skill `dinamica-omni-3-ruedas`.
- El método de control con actuador de **zona muerta** (potencia continua arriba
  de un mínimo): skill `control-pid-zona-muerta`.
- Cómo está armada la FSM del arquero: [`STRATEGY-CPP-COMO-FUNCIONA.md`](STRATEGY-CPP-COMO-FUNCIONA.md).
- El historial del loop lento del TOP y el fix: `journal/2026-06-10-banco-arquero-juez-pc-patrulla-v32-v33-top-lento.md`.
