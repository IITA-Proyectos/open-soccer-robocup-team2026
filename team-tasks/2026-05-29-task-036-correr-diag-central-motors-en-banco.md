---
id: TASK-036
title: "Correr diag_central_motors en banco — identificar motores 1/2/3 + cerrar conflicto pines 7/8"
date_created: 2026-05-29
assigned: [virginia-viollaz, elias, enzo]
priority: P0
status: pending
estimated_hours: 1
blocks: [hardware-up CENTRAL, "definición de movimientos" (próxima sesión Claude), migración eventual de Serial2 a Serial7 en comm_down]
blocked_by: [placa Zircon Rev v15 + Teensy 4.1 + batería cargada, excepción Avast aplicada (TASK-025)]
tags: [hardware, central-board, motors, pinout, conflicto-pines-7-8, diag]
related: [team-tasks/2026-05-15-task-011-confirmar-pin-kicker-solenoide-zircon.md, hardware/electronics/central-board-pack/01-pinout-y-hardware.md]
---

# TASK-036 — Correr `diag_central_motors` en banco

> ⚠️ **Actualización 2026-06-03.** (1) El **conflicto pines 7/8 YA está resuelto**
> por reasignación de UART (link DOWN→CENTRAL = **Serial1 0/1**, TOP→CENTRAL =
> **Serial7 28/29**; 7/8 quedan para el motor 2). **Ignorar** todo lo de abajo sobre
> "cerrar el conflicto 7/8 / migrar Serial2 a Serial7" — es histórico. (2) Sigue
> pendiente y es lo importante: **mapeo motor↔rueda + veredicto de la polaridad de
> M2** (2026-05-29 dio `{+1,+1,+1}` sin inversión, pero 2026-06-01 reportó M2
> INA/INB invertido por HW — **reconciliar en ESTE robot**). Contexto y secuencia:
> [TASK-101](2026-06-03-task-101-banco-mitad-inferior-cinematica-y-fork-arquero.md).

## Resumen

Sketch nuevo `diag_central_motors` listo en el repo el 2026-05-28
(sesión Claude, requested-by Gustavo). Hace falta que **el equipo humano
que tiene la placa Zircon Rev v15** lo flashee, lo corra en banco y
documente los resultados. Sin esto:

1. No sabemos qué motor del firmware (1/2/3) es qué rueda física del
   robot — lo necesitamos para la próxima sesión Claude de "definición de
   movimientos".
2. **No cerramos el conflicto P0 pines 7/8** (documentado en
   `01-pinout-y-hardware.md §8`). El test es el método más rápido para
   resolverlo empíricamente sin recurrir al schematic PDF.

## Pre-requisitos

- Placa Zircon Rev v15 + Teensy 4.1 enchufado.
- Cable USB al PC.
- Los 3 motores cableados a sus drivers del Zircon (U5, U17, U7) +
  batería cargada (los drivers NO se alimentan por USB).
- Robot SUJETO al banco o con las ruedas al aire (el sketch hace girar
  a 50% PWM máx — puede salir corriendo si está en el piso).
- Excepción Avast aplicada (TASK-025) si PlatformIO se traba al bajar
  paquetes. Este sketch NO usa libs externas, solo Arduino core — debería
  compilar 100% offline.

## Procedimiento

Detalles completos en
[`docs/firmware/DIAG-CENTRAL-MOTORS.md`](../docs/firmware/DIAG-CENTRAL-MOTORS.md).
Resumen:

1. `cd "software/teensy/Soccer 2026"`
2. `pio run -t clean -e diag_central_motors`
3. `pio run -e diag_central_motors -t upload`
4. `pio device monitor -b 115200` — confirmar banner inicial + LED parpadea
   2 Hz.
5. Apretar el botón físico (pin 9) o mandar `<enter>` por Serial Monitor.
6. Cada apretón pasa al siguiente motor. Cuarto apretón = fin.
7. Anotar la tabla del doc:

| Motor firmware | Pines | Driver | Rueda física | ¿Gira? | Sentido |
|---|---|---|---|---|---|
| Motor 1 | 2/5/3 | U5 | ____________ | ☐ S ☐ N | ____ |
| Motor 2 | 8/7/6 | **U17** ← conflicto | ____________ | ☐ S ☐ N | ____ |
| Motor 3 | 11/12/4 | U7 | ____________ | ☐ S ☐ N | ____ |

## Criterio de cierre

La TASK se cierra cuando:

1. El test corrió completo (los 3 motores se probaron o quedó claro cuál
   falló y por qué).
2. La tabla está completa con foto del setup.
3. Hay entrada de journal `journal/2026-05-29-diag-central-motors-<descripcion>.md`
   (o la fecha real de ejecución) con:
   - Veredicto del conflicto pines 7/8: **Caso A** (los 3 giran → migrar
     Serial2 a Serial7) / **Caso B** (motor 2 no gira → conflicto ficticio,
     limpiar warning) / **Caso C** (ninguno gira → debug más profundo) /
     **Caso D** (otro patrón).
   - Próxima TASK abierta según el caso (ver doc operativo).
4. Si veredicto = Caso A → abrir TASK nueva "Migrar Serial2 CENTRAL a
   Serial7 (pines 28/29)".
5. Si veredicto = Caso B → cerrar el warning del conflicto en
   `central-board-pack/01-pinout-y-hardware.md §8` + actualizar
   `FUENTES-DE-VERDAD.md`.
6. Si veredicto = Caso C → abrir TASK nueva de debug eléctrico
   (batería, alimentación drivers, osciloscopio sobre PWM).

## Por qué es P0

Bloquea:
- La próxima sesión Claude ("definición de movimientos") — sin saber qué
  motor es qué rueda, los vectores cinemáticos no se pueden mapear bien.
- El hardware-up completo de CENTRAL (regla 8 CLAUDE.md).
- La validación de que el firmware nuevo de `comm_down` (Serial2) es
  funcionalmente correcto.

Tiempo estimado: **1 h** entre flasheo, ejecución y journal entry.

## Atribución del sketch + docs

- Sketch + environment + docs: Claude Opus 4.7 (Anthropic), sesión
  2026-05-28 (requested-by Gustavo Viollaz @gviollaz).
- **La ejecución del test la hace el equipo humano que tiene la placa** —
  Claude NO cierra TASKs de hardware (regla 1 de CLAUDE.md).
