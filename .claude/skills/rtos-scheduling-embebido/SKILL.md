---
name: rtos-scheduling-embebido
description: Use when designing or reasoning about an RTOS / multitasking firmware — tasks, priorities, preemptive scheduling, schedulability (RMS/EDF), priority inversion, mutex/semaphore/queue choice, ISR-to-task handoff, stack sizing, or deciding whether a project needs an RTOS at all. Triggers - "RTOS", "FreeRTOS / Zephyr / TeensyThreads / Zircon RTOS", "tareas / tasks", "prioridades", "scheduler / planificador", "preempción", "inversión de prioridad / priority inversion", "RMS / EDF / planificabilidad", "mutex / semáforo / cola / queue", "context switch", "stack overflow de una tarea", "ISR a tarea", "¿necesito un RTOS?". Covers scheduling theory + practical RTOS patterns and classic bugs. NOT for the general real-time/timing lens (tiempo-real-determinismo), NOT for control-loop discretization (control-embebido-tiempo-real), NOT for fault tolerance/redundancy (sistemas-criticos-tolerancia-fallas).
---

# RTOS y scheduling embebido — concurrencia con garantías

## Principio central

**Un RTOS no te da tiempo real — te da una forma ESTRUCTURADA de repartir el CPU
entre tareas de criticidad distinta, con garantías que podés ANALIZAR.** El valor no
es "multitarea", es **predictibilidad**: poder demostrar, antes de correr, que la
tarea hard SIEMPRE cumple su deadline aunque las soft se amontonen. Si no vas a
analizar la planificabilidad, un RTOS solo te suma context switches y bugs de
concurrencia sin la garantía que justifica su costo.

## Cuándo un RTOS gana su lugar (y cuándo no)

| Situación | Arquitectura correcta |
|---|---|
| Pocas tareas, todas no-bloqueantes, deadlines holgados | **Superloop / cyclic executive** (lo del robot). No metas RTOS. |
| Una tarea LENTA (descarga, log a flash) que no puede frenar una tarea HARD | **RTOS preemptivo** — la hard expropia a la lenta |
| Muchas tareas con períodos y criticidades muy distintas | **RTOS** + análisis RMS |
| Una sola cosa crítica + I/O por DMA | superloop alcanza; el DMA es tu "concurrencia" |

> **Veredicto honesto para este robot:** hoy **no** necesita un RTOS. El superloop
> bare-metal es correcto para su cantidad de tareas, y su problema real era I/O
> bloqueante (que el RTOS no cura solo — ver `tiempo-real-determinismo`). El RTOS es
> conocimiento de oficio + inversión 2027 (si el firmware crece en tareas asíncronas).
> Aun sin RTOS, las ideas de abajo (prioridades, ISR diferida, no compartir estado
> sin protección) aplican al superloop.

## Scheduling: las dos políticas que tenés que entender

- **RMS (Rate Monotonic Scheduling)** — prioridad FIJA por **frecuencia**: la tarea
  más frecuente, más prioridad. Óptimo entre los de prioridad fija. **Cota de
  Liouville-Layland:** un conjunto de N tareas es planificable si la utilización
  `U = Σ(Cᵢ/Tᵢ) ≤ N(2^(1/N) − 1)` (→ ~69% cuando N→∞). Si tu CPU está <69% ocupada
  con RMS, dormís tranquilo; entre 69% y 100% hay que analizar caso a caso.
- **EDF (Earliest Deadline First)** — prioridad DINÁMICA: corre la tarea cuyo deadline
  está más cerca. Llega al **100% de utilización** (óptimo absoluto), pero es más
  difícil de implementar y se degrada feo si te pasás (efecto dominó). La mayoría de
  los RTOS embebidos usan prioridad fija (RMS-style), no EDF.

`Cᵢ` = WCET de la tarea i (lo MEDÍS, ver `tiempo-real-determinismo`), `Tᵢ` = su
período. **El análisis vale lo que valen tus WCET** — un WCET subestimado invalida la
garantía.

## La trampa clásica: inversión de prioridad

Una tarea de **alta** prioridad queda bloqueada esperando un recurso (mutex) que tiene
una de **baja**, y una de **media** (que no usa el recurso) le pasa por encima a la
baja → la alta espera indefinidamente a la media. **Le pasó a la Mars Pathfinder en
1997** (se reseteaba sola en Marte). 

**Curas:**
- **Priority inheritance** — mientras la baja tiene el mutex que la alta espera, la
  baja HEREDA la prioridad de la alta (la media ya no le pasa por encima). Lo que usan
  FreeRTOS (`xSemaphoreCreateMutex`) y casi todos.
- **Priority ceiling** — el mutex sube a quien lo toma al "techo" de prioridad de
  todos sus usuarios. Más fuerte, evita deadlocks también.

> Regla: si una sección crítica la comparten tareas de prioridad distinta, el
> primitivo de exclusión **tiene que** tener herencia/techo. Un semáforo binario
> pelado NO lo tiene → no lo uses como mutex entre prioridades distintas.

## Sincronización: elegí el primitivo correcto

| Necesidad | Primitivo |
|---|---|
| Exclusión mutua entre tareas (proteger un dato compartido) | **mutex** (con herencia de prioridad) |
| Señalar "ocurrió un evento" de ISR→tarea | **semáforo** (binario) o task notification |
| Pasar DATOS de productor a consumidor sin compartir memoria | **cola/queue** (la opción más segura) |
| Contar recursos disponibles (N buffers) | **semáforo contador** |

**Regla de oro de concurrencia:** *no compartas estado mutable; pasá mensajes.* Una
cola productor-consumidor elimina clases enteras de bugs (races, corrupción) que un
mutex apenas mitiga.

## ISR → tarea: la regla de las interrupciones

**La ISR hace lo MÍNIMO y difiere el resto.** Lee el periférico, encola el dato,
señala una tarea, y vuelve. Nada de trabajo pesado, nada de I/O bloqueante, nada de
`malloc`, nada de esperar otro bus dentro de una ISR (ahí muere la latencia de todas
las demás). Patrón "deferred interrupt" / bottom-half. (Aplica también al superloop:
la ISR setea un flag/encola, el loop procesa.)

Más patrones (productor-consumidor, time-triggered, deferred ISR, watchdog task) y
los bugs clásicos (priority inversion, unbounded queue, stack overflow, deadlock,
prioridad mal asignada): **`references/patrones-rtos-y-trampas.md`**.

## Stack y memoria (lo que cuelga RTOS reales)

- **Cada tarea tiene su propio stack** → dimensionarlo es crítico. Stack chico =
  corrupción silenciosa del de al lado (un bug infernal de debuggear). Medí el
  *high-water mark* (FreeRTOS: `uxTaskGetStackHighWaterMark`) y dejá margen.
- En hard real-time: **todo estático**, sin heap en runtime. Tareas, colas y stacks se
  crean al boot y no se destruyen.

## Errores comunes

- Meter un RTOS "porque es más pro" cuando un superloop alcanza → más complejidad y
  bugs de concurrencia sin garantía nueva.
- Calcular planificabilidad con WCET inventados (la garantía es tan buena como el peor
  caso MEDIDO).
- Usar un semáforo binario como mutex entre prioridades distintas → inversión de prioridad.
- Trabajo pesado o I/O bloqueante dentro de una ISR.
- Compartir un struct entre tareas sin protección "porque es solo una lectura" (un
  `int` de 32 bits puede ser atómico; un `struct` de pose NO lo es → lectura rota).
- Stacks sin medir → overflow intermitente que aparece en sede.

## Skills relacionadas

- **La lente de tiempo real / por qué el problema casi nunca es "falta RTOS":** `tiempo-real-determinismo`.
- **El lazo de control corriendo bajo el scheduler (multi-rate, jitter):** `control-embebido-tiempo-real`.
- **Tareas de watchdog, redundancia, particionado de seguridad:** `sistemas-criticos-tolerancia-fallas`.
- **Medir WCET para el análisis de planificabilidad:** `tiempo-real-determinismo` → `references/medir-y-presupuestar-tiempo.md`.
