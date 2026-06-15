# Medir y presupuestar el tiempo en embebido — sin esto, el determinismo es fe

> Referencia de `tiempo-real-determinismo`. Cómo se MIDE el timing de firmware en
> hardware real (no se estima de memoria) y cómo se arma un **presupuesto de tiempo**
> que garantice los deadlines. Voz de estudiante: cada técnica con qué mide, cómo, y
> su trampa.

## Por qué medir y no estimar

El WCET (peor caso) casi nunca es obvio leyendo el código: un `for` que "casi
siempre" itera 3 veces a veces itera 64; una lectura I²C que "tarda poco" se cuelga
60 ms bajo contención (el bug del loop a 6 Hz del robot). **El peor caso vive en los
caminos que no mirás.** Por eso se mide, con carga real, buscando el máximo — no el
promedio bonito.

## Técnica 1 — GPIO toggle + osciloscopio / analizador lógico (la regla de oro)

**Qué mide:** latencia, período y jitter, con resolución de nanosegundos, sin
perturbar casi nada.

**Cómo:** levantás un pin GPIO al ENTRAR a la tarea y lo bajás al SALIR. En el
osciloscopio ves el ancho del pulso (= tiempo de ejecución) y el período entre
pulsos (= cadencia del lazo). Para latencia de interrupción: pin arriba en el evento,
pin abajo cuando la ISR responde.

```
void tarea_critica() {
    digitalWriteFast(PIN_PROBE, HIGH);   // <-- entra
    // ... trabajo ...
    digitalWriteFast(PIN_PROBE, LOW);    // <-- sale
}
```

**Trampa:** medí con CARGA REAL y el peor escenario (todos los sensores hablando,
buffers llenos), no con el robot quieto. El máximo ancho de pulso que veas en varios
minutos ≈ tu WCET observado (cota inferior del WCET verdadero).

> En este robot ya existe el equivalente software: el `Δloop=` del panel `[TOP]`
> mostró el lazo a ~4-6 Hz y luego a ~190k/s. El GPIO+scope es la versión de
> nanosegundos para los caminos finos (ISR, sección crítica). TASK-014.

## Técnica 2 — Contador de ciclos en chip (DWT CYCCNT en Cortex-M)

**Qué mide:** ciclos de CPU exactos de un tramo de código, sin hardware externo.

**Cómo:** el Cortex-M7 (Teensy 4.x) tiene el contador `DWT->CYCCNT`. Lo leés antes y
después; la resta son ciclos → dividís por la frecuencia (600 MHz) = tiempo.

```
uint32_t t0 = ARM_DWT_CYCCNT;
// ... tramo ...
uint32_t ciclos = ARM_DWT_CYCCNT - t0;   // ns = ciclos * 1000 / 600
```

**Trampa:** mide tiempo de CPU, NO el tiempo de pared si la tarea espera un periférico
(el bus I²C no consume ciclos de CPU mientras esperás el ACK → el CYCCNT *subestima*
una lectura bloqueante). Para I/O bloqueante usá el GPIO+scope (Técnica 1). Cuidado
con el wrap de 32 bits (~7 s a 600 MHz).

## Técnica 3 — Histograma de período (cazar el jitter)

**Qué mide:** la distribución del período del lazo, no solo el promedio. El jitter
vive en la COLA.

**Cómo:** en cada vuelta, guardá `dt = now - last`. Acumulá min/max/histograma sobre
miles de vueltas. El número que importa es **max − min** (jitter pico a pico) y la
cola del histograma, no la media.

**Trampa:** promediar esconde el problema. "Período medio 5 ms" puede ocultar
picos de 40 ms una vez por segundo — y ese pico es el que pierde el deadline.

## Técnica 4 — Stress / inyección de peor caso

**Qué mide:** el WCET de verdad, forzando los caminos caros.

**Cómo:** provocá a propósito lo peor: todos los sensores reportando, todas las
ramas del `if` activas, buffers de UART al borde del overflow, máxima tasa de
interrupciones. Medí con las técnicas 1-3 bajo ese estrés.

**Trampa:** si solo probás el caso feliz, el WCET real te espera en competencia
(otra iluminación, más tráfico de bus, un rival generando más detecciones).

## Armar el presupuesto de tiempo (timing budget)

Una vez que tenés el WCET de cada tarea del lazo, el presupuesto es aritmética:

1. **Listá las tareas** del lazo con su WCET medido y su período requerido.
2. Para un superloop/cyclic executive: **Σ WCET de todo lo que corre en una vuelta
   ≤ período objetivo**, con **margen** (apuntá a usar ≤70% del período — el headroom
   absorbe lo que no mediste).
3. Si no entra: (a) sacá I/O bloqueante (round-robin, DMA, no-bloqueante), (b) bajá
   la tasa de las tareas soft (multi-rate: la telemetría no necesita 100 Hz), (c)
   movés a un RTOS si la criticidad lo justifica (`rtos-scheduling-embebido`).
4. Para un RTOS preemptivo: el presupuesto es un **análisis de planificabilidad**
   (RMS/EDF), no una suma simple — ver esa skill.

**Ejemplo (robot, estilo):**

| Tarea | WCET medido | Período | Notas |
|---|---|---|---|
| Leer 1 ToF (round-robin) | ~60 ms ❌ → 1/tick | 100 Hz | era el asesino; round-robin lo reparte |
| Fusión cámaras + build_snapshot | medir | 100 Hz | firm (pelota) |
| TX WorldSnapshot (Serial7) | medir | 100 Hz | con `availableForWrite()` (no bloquear) |
| Telemetría USB | medir | dormida en partido | soft |

La lección del 6 Hz en una línea: **una tarea con WCET de 60 ms × 4 en una vuelta no
entra en un período de 10 ms.** El presupuesto lo habría predicho antes del banco.

## Salida

Una tablita WCET/período/margen que va al journal junto a la traza del scope. Con
eso, "el robot anda" se vuelve "el lazo cierra en X ms con Y% de margen, medido bajo
carga" — que es lo que se puede defender ante un jurado y reproducir en sede.
