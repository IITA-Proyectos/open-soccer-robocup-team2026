---
name: tiempo-real-determinismo
description: Use when reasoning about REAL-TIME behavior of embedded firmware — whether a system meets deadlines deterministically, why a loop runs slow or stutters, latency/jitter/worst-case timing, blocking I/O in the control loop, superloop vs cyclic-executive vs RTOS choice. Triggers - "tiempo real / real-time", "determinismo", "el loop corre lento / a 6 Hz", "jitter", "latencia", "WCET / peor caso", "se cuelga / se congela el loop", "I/O bloqueante", "hard vs soft real time", "deadline", "cuánto tarda mi loop", "el sensor frena todo". The foundational real-time lens; grounds in this robot's real timing failures (BNO freeze, 6 Hz TOP loop). NOT for RTOS task/scheduling design itself (rtos-scheduling-embebido), NOT for control-loop math/discretization (control-embebido-tiempo-real), NOT for fault tolerance (sistemas-criticos-tolerancia-fallas), NOT for tuning a PID (control-pid-zona-muerta) or the omni plant (dinamica-omni-3-ruedas).
---

# Tiempo real y determinismo — la respuesta correcta, A TIEMPO

## Principio central

**En un sistema de tiempo real, llegar tarde es una respuesta INCORRECTA, aunque
el número sea perfecto.** La corrección tiene dos mitades: el *valor* y el
*instante*. Un valor perfecto que llega 3 ms tarde puede tirar la pelota, quemar un
motor, o estrellar un avión. Por eso la métrica que importa **no es la velocidad
promedio — es el PEOR CASO acotado.** Un sistema que corre a 1 MHz "casi siempre"
pero una vez cada mil ciclos tarda 200 ms **no es de tiempo real**: es rápido y no
determinista, que es otra cosa.

> Regla de oro: **determinismo > rendimiento promedio.** Preferí un lazo que SIEMPRE
> tarda 4,9 ms a uno que tarda 1 ms el 99,9% del tiempo y 50 ms el resto.

## Las clases (define la consecuencia de perder el deadline)

| Clase | Si se pierde el deadline… | Ejemplo |
|---|---|---|
| **Hard** | falla del sistema / catástrofe | inyección por cigüeñal, control de vuelo, airbag |
| **Firm** | el resultado tardío es inútil (se descarta) | un frame de visión que llegó tarde al control |
| **Soft** | degrada la calidad, no falla | refrescar un display, loguear telemetría |

El robot mezcla las tres: el lazo de heading/motores es **firm-a-hard** (tarde =
trompo o gol en contra), la telemetría USB es **soft** (se puede dormir en partido).
Diseñá cada tarea sabiendo su clase — no le des trato hard a lo soft ni viceversa.

## Las 3 métricas (medí esto, no el promedio)

- **Latencia** — desde el evento (interrupción, sensor nuevo) hasta la respuesta.
- **Jitter** — la VARIACIÓN del período/latencia ciclo a ciclo. El enemigo silencioso
  del control: un lazo que muestrea con jitter alto introduce ruido que ninguna
  ganancia arregla (ver `control-embebido-tiempo-real`).
- **WCET** (Worst-Case Execution Time) — cuánto tarda la tarea en su **peor** camino,
  no en promedio. El presupuesto de tiempo se hace con WCET, nunca con el promedio.

Cómo medirlas en hardware real (GPIO + osciloscopio, contador de ciclos DWT,
histograma de período) y armar el presupuesto: **`references/medir-y-presupuestar-tiempo.md`**.

## Qué MATA el determinismo (la checklist de sospechosos)

1. **I/O bloqueante dentro del lazo de control.** El #1. Una lectura I²C/SPI/UART que
   espera al periférico congela todo lo demás. ← *la causa raíz de los dos bugs de
   tiempo real más caros de este robot* (ver abajo).
2. **ISR largas o con I/O.** Una interrupción que hace trabajo pesado (o peor, espera
   un bus) bloquea todas las de menor prioridad → latencia descontrolada. Las ISR
   hacen lo mínimo y difieren el resto (patrón en `rtos-scheduling-embebido`).
3. **Asignación dinámica** (`malloc`/`new`) en el camino crítico: tiempo no acotado +
   fragmentación. En hard real-time se prohíbe (MISRA, DO-178C). Todo estático/pool.
4. **Bucles sin cota** (esperar "hasta que" sin timeout) y `delay()` ocupado.
5. **Contención de recurso compartido** (un bus, un mutex) bajo carga → esperas
   variables. Caso real del robot: BNO + 4 ToF en el mismo `Wire`.
6. **Variabilidad de HW**: cache misses, predicción de saltos, DMA robando ciclos.
   En MCU chicos importa menos; en aeroespacial se desactiva/acota la cache para
   poder calcular el WCET.

## Anclaje en este robot (timing real, no teoría)

- **El loop del TOP corría a ~6 Hz y se arregló a ~190.000/s.** Causa raíz:
  los 4 `getRangingData()` del VL53L7CX por pasada, cada uno trayendo el bloque
  COMPLETO por `Wire`@100 kHz (~60 ms/sensor) = **I/O bloqueante en el lazo
  (sospechoso #1)**. Fix: **round-robin** (UN ToF por tick) + payload recortado →
  el WorldSnapshot volvió a 100 Hz. (`docs/ESTADO-ACTUAL.md`, banco 2026-06-10.)
  Esto es el ejemplo de libro: el promedio mentía, el **peor caso** (4 lecturas
  bloqueantes/pasada) definía el período.
- **El "freeze" del heading del BNO** = el mismo veneno: contención del BNO055 con
  los ToF en el bus `Wire` compartido bajo carga → la lectura se cuelga y el heading
  queda clavado. No es "el sensor roto": es **arquitectura de tiempo real** (bus
  compartido + acceso bloqueante). Fix correcto: bus aparte (TASK-207). El detector
  `imu_freeze.h` es una red, no la cura.
- **TASK-014**: "loop TOP no-bloqueante medido con osciloscopio" — exactamente la
  disciplina de esta skill. Sin medir, el determinismo es fe.
- **`omega*100` en int16** (clamp ≤327°/s o desborda con signo invertido): el tiempo
  real también es de TIPOS y saturación, no solo de relojes.

## Superloop vs cyclic executive vs RTOS (la decisión de arquitectura)

| Arquitectura | Qué es | Cuándo |
|---|---|---|
| **Superloop** (lo que corre hoy) | un `while(1)` que llama tareas en orden, cooperativo, sin preempción | pocas tareas, deadlines holgados, todo "rápido y no bloqueante". Simple y predecible **si ninguna tarea bloquea**. |
| **Cyclic executive** | superloop con slots de tiempo fijos (frames) y multi-rate | cuando querés garantías de período sin un RTOS. El robot ya lo aproxima (`if millis()-last >= 33`). |
| **RTOS preemptivo** | tareas con prioridad, el scheduler expropia | muchas tareas de criticidad MUY distinta, o una tarea hard que no puede esperar a otra lenta. Ver `rtos-scheduling-embebido`. |

> **Veredicto honesto para este robot:** el superloop bare-metal alcanza para
> Incheon — el problema NUNCA fue "falta un RTOS", fue **I/O bloqueante en el lazo**
> (que un RTOS *tampoco* arregla solo). Primero sacá lo bloqueante; el RTOS es
> inversión 2027 / entender el oficio. Detalle: `rtos-scheduling-embebido`.

## Errores comunes

- Optimizar el promedio mientras el **peor caso** rompe el deadline (el bug del 6 Hz).
- Meter una lectura de sensor bloqueante en el lazo "porque es solo un sensor".
- "Anda en el banco" sin haber **medido** jitter/WCET → falla bajo carga en sede.
- Tratar la latencia variable como ruido del control en vez de un problema de timing
  (ningún PID arregla jitter de muestreo — ver `control-embebido-tiempo-real`).
- Confundir "rápido" con "de tiempo real". Son ortogonales.

## Skills relacionadas

- **Concurrencia, prioridades, scheduling, sincronización:** `rtos-scheduling-embebido`.
- **El lazo de control en tiempo real (muestreo, discretización, latencia):** `control-embebido-tiempo-real`.
- **Fail-safe / watchdog / redundancia cuando el deadline ES seguridad:** `sistemas-criticos-tolerancia-fallas`.
- **Medir en hardware real (obligatorio):** `hardware-test-protocol`.
- **Tuning del lazo sobre la planta cuantizada:** `control-pid-zona-muerta` + `dinamica-omni-3-ruedas`.
