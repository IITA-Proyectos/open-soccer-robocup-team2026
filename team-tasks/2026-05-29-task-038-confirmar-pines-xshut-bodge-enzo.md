---
id: TASK-038
title: "Confirmar pines del Teensy del bodge XSHUT (R1 y R2)"
date_created: 2026-05-29
date_due: 2026-05-31
assigned: [Enzo]
priority: P1
status: pending
estimated_hours: 0.5
blocks: [sprint-b-extender-sensors-tof, task-035-validacion-hardware]
blocked_by: []
tags: [hardware, top-board, bodge, xshut, hal]
---

# TASK-038 — Confirmar pines del Teensy del bodge XSHUT

## Por qué

El HAL Sprint A dejó en `src/top/pinout_robot1.h` y `pinout_robot2.h`
los pines del Teensy para XSHUT como **PLACEHOLDER** (9, 11, 12, 22).
El bodge físico que hiciste puede usar otros pines (vos dijiste que ibas
a usar los que originalmente iban a INT, desconectándolos y poniéndolos
en LP).

Sin confirmar los pines reales, el firmware no puede inicializar los 4
TOFs correctamente.

## Qué necesito

Para cada robot (R1 arquero y R2 delantero) decime los 4 números:

- TOF[0] frontal (slot U2 del schematic): pin ___
- TOF[1] trasero (slot U3): pin ___
- TOF[2] izquierdo (slot U5): pin ___
- TOF[3] derecho (slot U17): pin ___

Si los 2 robots tienen los MISMOS pines, decime una sola lista. Si son
distintos, una lista por robot.

## Cómo se actualiza el firmware después

Editar `src/top/pinout_robot1.h` (y `pinout_robot2.h` si difiere) cambiando
el array `PIN_TOF_XSHUT[4] = {...}` con los pines reales. Después:

```
pio run -e top_robot1 -t upload   # o -e top_robot2 -t upload
```

Compilation falla si alguno de los pines elegidos colisiona con un uso
existente del Teensy (ej. si Enzo eligió pin 18 que es SDA0). En ese caso,
escribirle a Enzo + log en journal.

## Criterio de cierre

- TASK closed con los 4 (u 8 si difieren) números de pines.
- pinout_robot1.h y/o pinout_robot2.h actualizados con los valores reales.
- Journal entry con la decisión.
