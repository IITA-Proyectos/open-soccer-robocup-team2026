# Discretización y punto fijo — la matemática del lazo en el micro

> Referencia de `control-embebido-tiempo-real`. La parte numérica que decide si el
> lazo anda igual que en la simulación: cómo pasar de continuo a discreto, y cómo
> computarlo con enteros sin que desborde ni pierda resolución. Voz de estudiante:
> fórmulas con el "por qué", no derivaciones de pizarrón.

## Parte A — De continuo a discreto

### El PID continuo y su versión en diferencias

El PID continuo es `u(t) = kp·e + ki·∫e dt + kd·de/dt`. En el micro, cada término se
aproxima sobre el período `dt` (= tiempo entre muestras):

```
e   = setpoint - medicion
P   = kp * e
I  += ki * e * dt                  // integral por acumulación (backward Euler)
D   = kd * (e - e_prev) / dt       // derivada por diferencia hacia atrás
u   = P + I + D
e_prev = e
```

### Por qué el método de la integral importa

| Método | Fórmula del paso | Cuándo |
|---|---|---|
| **Backward Euler** | `I += ki·e·dt` (usa el error ACTUAL) | default en MCU: estable, simple |
| **Tustin / trapezoidal** | `I += ki·(e + e_prev)/2·dt` | más preciso; mejor preservación de fase; el "estándar" para discretizar bien |
| **Forward Euler** | `I += ki·e_prev·dt` (usa el error VIEJO) | evitalo para la integral: corre la fase y puede inestabilizar |

Tustin es el mapeo `s → (2/dt)·(z−1)/(z+1)`: la forma "correcta" de llevar un diseño
continuo a discreto preservando estabilidad y fase. Para la mayoría de los lazos de un
robot, backward Euler alcanza; Tustin gana cuando el `dt` no es chiquísimo frente a la
dinámica.

### La derivada SIEMPRE filtrada

`(e − e_prev)/dt` amplifica el ruido: un escalón de 1 LSB del sensor se convierte en un
pico enorme dividido por un `dt` chico. Dos arreglos que se combinan:

1. **Derivada filtrada** (pasa-bajos de primer orden sobre D):
   `D = (1−α)·D_prev + α·kd·(e − e_prev)/dt`, con `α` entre la dinámica y el ruido.
2. **Derivative on measurement** (derivar la medición, no el error): evita el "derivative
   kick" cuando cambia el setpoint de golpe. `D = −kd·(medicion − medicion_prev)/dt`.

### Anti-windup (acá solo el gancho)

Cuando el actuador satura, el integrador sigue acumulando un error que no puede
corregir → al salir de saturación hay un sobrepaso enorme. Se ataja con clamping o
back-calculation. El tratamiento completo + cómo se titula en banco está en
`control-pid-zona-muerta` (no lo repito).

### Elegir `dt`: la ventana correcta

- Demasiado lento → el lazo "ve" la planta a saltos, pierde margen de fase, oscila.
- Demasiado rápido → la derivada amplifica ruido y, en punto fijo, `e·dt` cae bajo la
  resolución (el integrador no avanza, "se duerme").
- Regla: **10–20× el ancho de banda** del lazo, y nunca más rápido que el sensor que lo
  alimenta. Y **fijo** (el jitter rompe la suposición de `dt` constante — si no podés
  fijarlo, medí el `dt` real y usalo en las fórmulas).

## Parte B — Punto fijo (Q-format) sin que explote

### Qué es Q-format

Un entero que representa un real con una escala implícita de potencia de 2. "Qn" = n
bits de fracción. `Q8`: el entero `256` representa `1.0` (256 = 2⁸); el `K=26` del
filtro de pose del robot es `26/256 ≈ 0.10`. Sumás/restás como enteros normales; al
**multiplicar** dos Qn el resultado es Q2n → hay que **re-escalar** (shift) de vuelta.

```
// multiplicar dos Q8 y volver a Q8, con acumulador ancho:
int32_t prod = ((int32_t)a_q8 * b_q8) >> 8;   // >>8 = dividir por 256
```

### Las tres trampas que te muerden

1. **Overflow del acumulador.** El producto de dos int16 entra en int32, no en int16.
   Si guardás en un tipo chico, desborda y cambia de signo. **Caso real del robot:**
   `omega*100` viaja en **int16** → hay que **clamp a ≤327°/s** (32767/100) o desborda
   con el signo invertido (el robot giraría al revés). Regla: el acumulador siempre
   más ancho que los operandos; saturá (clamp) antes de bajar de tamaño.

2. **Pérdida de resolución (underflow de paso).** Si `ki·e·dt` por ciclo es menor que
   1 LSB, el entero no cambia → el integrador **no avanza nunca** aunque haya error.
   Curas: acumular en más bits (Q16 para el integrador aunque la salida sea Q8), o
   guardar el "resto" fraccionario entre ciclos.

3. **Orden de operaciones.** `(a/c)*b` pierde precisión por la división temprana;
   `(a*b)/c` con acumulador ancho la preserva. **Multiplicá antes de dividir/shiftear**,
   siempre con un tipo lo bastante grande para el producto intermedio.

### Float vs punto fijo — la decisión

| Usá **float** si… | Usá **punto fijo** si… |
|---|---|
| el MCU tiene FPU (Cortex-M4F/M7F, Teensy 4.x) | no hay FPU (M0/M0+) |
| el WCET con float cierra el presupuesto | querés ciclos exactos / determinismo duro |
| la legibilidad importa más que el último ciclo | empaquetás en un protocolo de bytes (int16, etc.) |

El Teensy 4.x **tiene FPU** → para el control interno, float está bien y es más legible;
el punto fijo aparece donde el dato cruza un protocolo (el `omega*100` int16, el `K` Q8
del filtro). Regla sana: float para computar, punto fijo para **transportar** — y en
cada borde, clamp explícito.

## Checklist de implementación de un lazo

1. ¿`dt` fijo, o medido y usado en las fórmulas? (no asumido)
2. ¿Integral por backward Euler/Tustin, con anti-windup?
3. ¿Derivada filtrada y/o sobre la medición?
4. ¿Tasa atada al sensor/actuador (no más rápido por las dudas)?
5. ¿Acumuladores más anchos que los operandos, clamp antes de reducir tamaño?
6. ¿Latencia sensor→actuador medida y minimizada?
7. ¿Probado contra una corrida de referencia (no solo "parece andar")?
