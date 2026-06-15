# Patrones de RTOS y trampas clásicas

> Referencia de `rtos-scheduling-embebido`. Los patrones que se repiten en todo
> firmware multitarea bien hecho, y los bugs que hundieron sistemas reales. Cada
> patrón: qué problema resuelve y cómo se ve. Cada trampa: el síntoma y la cura.
> Casi todos aplican también a un superloop bien estructurado.

## Patrones que querés tener en el dedo

### 1. Productor-consumidor por cola (el patrón base)

**Problema:** una ISR/tarea genera datos a su ritmo; otra los procesa al suyo. Si
comparten una variable, hay races y se pierden o corrompen datos.

**Patrón:** la ISR/productor **encola** (no procesa); la tarea consumidora **desencola
y procesa**. La cola es el único punto de contacto → no hay estado compartido mutable.

```
ISR_uart():        q.push(byte)           // mínimo, no bloquea
tarea_parser():    while(q.pop(&b)) parse(b)   // a su propio ritmo
```

**Por qué:** desacopla ritmos, absorbe ráfagas (el buffer), y elimina races. Es la
forma segura de cruzar el límite ISR↔tarea y tarea↔tarea.

### 2. Deferred interrupt (bottom-half)

**Problema:** un evento necesita procesamiento pesado, pero hacerlo en la ISR bloquea
todas las demás interrupciones.

**Patrón:** ISR = "top-half" (lee el registro, limpia el flag, señala) → tarea =
"bottom-half" (hace el trabajo). La ISR dura microsegundos; el trabajo corre con
prioridad normal y puede ser expropiado.

### 3. Time-triggered / tabla de tiempos (cyclic executive)

**Problema:** querés períodos garantizados sin un scheduler preemptivo.

**Patrón:** un tick periódico dispara tareas según una tabla de slots
(tarea A cada tick, B cada 2, C cada 10). Determinista y trivial de analizar. **El
robot ya lo aproxima** con `if (millis() - last >= periodo)`. Es lo que usan muchos
sistemas de seguridad porque el comportamiento temporal es totalmente predecible.

### 4. Watchdog task / supervisor

**Problema:** una tarea puede colgarse o entrar en bucle sin que nadie se entere.

**Patrón:** cada tarea crítica "saluda" (setea su bit) periódicamente; una tarea
supervisora verifica que todos saludaron dentro de la ventana y, si no, patea el
**watchdog hardware** (que resetea el MCU) o entra en estado seguro. Ver
`sistemas-criticos-tolerancia-fallas` para el watchdog windowed.

### 5. Doble buffer / ping-pong

**Problema:** el productor escribe mientras el consumidor lee el mismo buffer → datos
inconsistentes (mitad viejos, mitad nuevos).

**Patrón:** dos buffers; el productor llena uno mientras el consumidor lee el otro, y
se intercambian con un swap atómico de puntero. El robot tiene un primo de esto en el
WorldSnapshot (single-producer/single-consumer "lock-free").

## Trampas clásicas (con su caso real)

### Inversión de prioridad — Mars Pathfinder (1997)

La nave se reseteaba sola en Marte. Una tarea de alta prioridad (bus de datos)
esperaba un mutex que tenía una de baja (meteorología), y una de media (comunicaciones)
acaparaba el CPU → la alta nunca corría → watchdog → reset. **Cura:** activar
**priority inheritance** en el mutex (JPL lo habilitó por patch remoto). Lección:
todo mutex compartido entre prioridades distintas necesita herencia o techo.

### Unbounded queue / unbounded buffer

Una cola sin límite que el productor llena más rápido de lo que el consumidor vacía →
crece hasta agotar la RAM → corrupción o crash. **Cura:** colas de tamaño FIJO + una
política explícita de overflow (descartar el más viejo, descartar el nuevo, o
backpressure). El robot ya hace backpressure en los UART con `availableForWrite()` +
contadores `frames_dropped` — eso es manejar el overflow a propósito en vez de reventar.

### Stack overflow de una tarea

Cada tarea tiene su stack; si una recursa hondo o declara un buffer grande local, pisa
la memoria de al lado → corrupción intermitente, el peor bug. **Cura:** medir el
high-water mark de cada tarea, dejar margen, evitar buffers grandes en el stack
(usar estáticos), y activar la detección de overflow del RTOS si la tiene.

### Deadlock por orden de locks

Tarea A toma el mutex 1 y espera el 2; tarea B toma el 2 y espera el 1 → abrazo
mortal. **Cura:** **orden global de adquisición** de locks (siempre 1 antes que 2), o
mejor: no usar dos locks (pasar mensajes). Priority ceiling también lo previene.

### Prioridad mal asignada (no monótona en frecuencia)

Darle más prioridad a una tarea lenta-pero-"importante" rompe RMS y hace que las
rápidas pierdan deadline. **Cura:** RMS dice **más frecuente = más prioridad**, sin
sentimentalismos. "Importante" no es lo mismo que "frecuente".

### Compartir un struct multi-palabra sin protección

Leer un `struct pose {x,y,θ}` mientras otra tarea lo escribe → lectura "rota" (x nuevo,
y viejo). Un `int32` alineado puede ser atómico; un struct **no**. **Cura:** mutex,
doble buffer, o snapshot atómico por puntero.

### Jitter de muestreo por scheduling

Si el lazo de control corre como tarea y el scheduler lo posterga de forma variable,
el `dt` real varía → el control "ve" ruido. **Cura:** disparar el lazo de control por
un timer/ISR de alta prioridad (no por el scheduler general), y/o medir el `dt` real y
usarlo en la discretización (ver `control-embebido-tiempo-real`).

## La regla que resume todo

**Minimizá el estado mutable compartido; cuando tengas que compartirlo, protegelo con
el primitivo correcto; mantené las ISR cortas; y no creas en una garantía de
planificabilidad construida sobre WCET que no mediste.**
