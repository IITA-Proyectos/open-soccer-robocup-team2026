---
name: control-embebido-tiempo-real
description: Use when implementing/EXECUTING a closed-loop controller on an MCU and the question is about the REAL-TIME and NUMERICAL realization, not the gains — sample-rate choice, continuous→discrete conversion, fixed-point vs float, loop latency / sensor-to-actuator delay, jitter's effect on a loop, multi-rate loops, discrete anti-windup & derivative filtering. Triggers - "frecuencia de muestreo / sample rate", "discretizar el PID / continuo a discreto", "Tustin / Euler / backward difference", "punto fijo / fixed-point / Q15 / Q8", "el dt varía / jitter en el lazo", "latencia sensor a actuador", "lazos multi-rate / lazo rápido y lento", "PID en C para microcontrolador", "el control tiembla por timing". For automotive/aero CONTROL examples see sistemas-criticos-tolerancia-fallas. NOT for TUNING gains on the quantized actuator (control-pid-zona-muerta), NOT for the omni plant model (dinamica-omni-3-ruedas), NOT for the general timing lens (tiempo-real-determinismo).
---

# Control embebido en tiempo real — la REALIZACIÓN del lazo, no las ganancias

## Principio central

**Un lazo de control en un micro es una ecuación en diferencias que corre en un
instante real, con números finitos.** Tres cosas que el papel ignora y que deciden si
el lazo anda: **CUÁNDO muestreás** (período y su jitter), **CÓMO discretizás** el
controlador continuo, y **CON QUÉ números** lo computás (punto fijo vs float, overflow,
cuantización). Las ganancias (que ajusta `control-pid-zona-muerta`) recién importan
DESPUÉS de que estas tres estén bien. Un PID con ganancias perfectas falla si el `dt`
varía, si discretizaste mal la derivada, o si el integrador desborda.

> Esta skill es el lado **tiempo-real/numérico** del control. El **tuning** sobre la
> planta cuantizada del robot vive en `control-pid-zona-muerta`; la **planta medida**
> (pisos PWM, regímenes) en `dinamica-omni-3-ruedas`. No los repito — los uso.

## 1. Elegir la frecuencia de muestreo (la decisión que más se equivoca)

- **Regla práctica:** muestreá el lazo de control a **10–20× el ancho de banda** que
  querés controlar (Nyquist —2×— es el mínimo para *reconstruir*, NO para *controlar*;
  para control querés bastante más margen de fase).
- **Más rápido NO siempre es mejor:** muestrear demasiado rápido amplifica el ruido en
  el término derivativo y, en punto fijo, hace que `Δ` por ciclo caiga bajo la
  resolución (te quedás sin bits). Hay una ventana correcta.
- **Atá la tasa al sensor y al actuador.** No corras el PID a 1 kHz si el sensor te da
  dato nuevo a 100 Hz (estarías derivando ruido de cuantización) ni si el actuador no
  puede responder más rápido. En el robot: la cámara manda a ~100 Hz, el snapshot llega
  a 100 Hz → el lazo de heading no gana nada por encima de eso.

## 2. El período tiene que ser FIJO (el jitter es veneno del control)

La discretización asume un `dt` constante. Si el lazo corre como "lo que sobra del
loop" y el período varía, el integrador y la derivada se calculan con un `dt`
equivocado → ruido que **ninguna ganancia arregla**.

**Dos curas (elegí una):**
- **Disparar el lazo por timer/ISR** de período fijo (lo ideal para hard real-time).
- **Medir el `dt` real** (con `micros()`/CYCCNT) y **usarlo** en las fórmulas
  (`I += e*dt`, `D = (e−e_prev)/dt`) en vez de asumir una constante. Mitiga el jitter
  moderado sin un timer dedicado.

Conexión con `tiempo-real-determinismo`: el jitter es un problema de **timing**, no de
control. Si el lazo tiembla y las ganancias no lo arreglan, medí el período antes de
tocar `kp`.

## 3. Discretización: de la fórmula continua a la ecuación en diferencias

El PID "de libro" es continuo; en el micro corre una **ecuación en diferencias**. Cómo
aproximás la integral y la derivada importa:

| Método | Integral | Nota |
|---|---|---|
| **Backward Euler** | `I += e·dt` | el más común en MCU, estable, simple |
| **Tustin (trapezoidal)** | `I += (e+e_prev)/2·dt` | más preciso, mejor mapeo de fase |
| **Forward Euler** | `I += e_prev·dt` | puede inestabilizar — evitar para la integral |

La **derivada** discreta `(e − e_prev)/dt` amplifica ruido → casi siempre va con un
**filtro pasa-bajos** (derivada filtrada / "derivative on measurement" para evitar el
patear-set-point). La matemática completa (Tustin, mapeo s→z, derivada filtrada):
**`references/discretizacion-y-punto-fijo.md`**.

## 4. Punto fijo vs float (y por qué el robot usa Q-format)

- **Float** en un MCU con FPU (Teensy 4.x, Cortex-M7F) es barato y cómodo → usalo si
  tenés FPU y el WCET cierra.
- **Punto fijo** (enteros con escala implícita, "Q15"/"Q8") es obligatorio si NO hay
  FPU, o si querés determinismo de ciclo exacto, o para empaquetar en el protocolo. El
  robot ya lo usa: la ganancia del filtro de pose en **Q8** (`K=26 ≈ 0.10`), y
  `omega*100` viaja como **int16** (cuidado: clamp ≤327°/s o desborda con signo
  invertido — un bug de overflow clásico de punto fijo).
- **Lo que te muerde en punto fijo:** overflow (acumuladores chicos), pérdida de
  resolución (Δ por ciclo < 1 LSB → el integrador no avanza), y el orden de las
  operaciones (multiplicá antes de dividir, con un acumulador más ancho). Detalle:
  la referencia.

## 5. Latencia sensor→actuador (el retardo que desestabiliza)

Todo lazo embebido tiene un **retardo de transporte**: el sensor mide, el dato viaja
(UART, fusión), el control computa, el actuador recién entonces actúa. Ese retardo
**come margen de fase** → puede oscilar un lazo que en el papel era estable.

- Medilo (es timing: ver `tiempo-real-determinismo`) y **minimizalo** (no acumular
  buffers, no corras el control un ciclo después de tener el dato).
- Si es grande e irreducible: un **predictor** (Smith predictor) o un feedforward
  compensa parte. En el robot, el arquero "anticipa" la X predicha de la pelota
  (`ball_predict`) — eso es exactamente compensar latencia con predicción.

## 6. Multi-rate (lazos a distinta velocidad)

No todo corre a la misma frecuencia: el lazo interno de corriente/velocidad va rápido
(kHz), el externo de posición más lento (cientos de Hz), la lógica táctica/FSM aún más
lento. Patrón **cascada**: el lazo externo fija el set-point del interno. Reglas: el
interno ≥5–10× más rápido que el externo; cada uno con su `dt`; cuidado con la
interacción si se acercan en frecuencia. (En autos y aero esto es la norma — ver
`sistemas-criticos-tolerancia-fallas`.)

## Errores comunes

- Asumir `dt` constante en un lazo que corre con jitter → ruido inexplicable.
- Derivada sin filtro → amplifica el ruido de cuantización del sensor.
- Integrador sin **anti-windup**: satura el actuador y el integrador sigue cargando →
  sobrepaso enorme al revertir (cubierto en profundidad en `control-pid-zona-muerta`).
- Overflow de punto fijo (el caso `omega*100` int16) o Δ que cae bajo 1 LSB.
- Muestrear mucho más rápido que el sensor "por las dudas" → derivás ruido.
- Tunear ganancias antes de arreglar muestreo/discretización/latencia (orden invertido).

## Skills relacionadas

- **Tunear las ganancias sobre el actuador cuantizado (PFM, deadband, anti-windup):** `control-pid-zona-muerta`.
- **La planta MEDIDA del robot (pisos PWM, regímenes, deriva):** `dinamica-omni-3-ruedas`.
- **Timing/jitter/latencia como problema de tiempo real:** `tiempo-real-determinismo`.
- **Correr el lazo bajo un scheduler (multi-rate, ISR de control):** `rtos-scheduling-embebido`.
- **Cómo lo hacen autos/aero (inyección, FADEC, flight control):** `sistemas-criticos-tolerancia-fallas`.
