---
id: TASK-203
title: "Mapear orientación interna de las zonas de los 4 ToF (para barrido lidar-360)"
date_created: 2026-05-31
date_due: 2026-06-12
assigned: [Enzo, elias]
priority: P2
status: pending
estimated_hours: 1.5
blocks: [lidar-360, sensors_tof-sprint-b]
blocked_by: []
tags: [hardware, top-board, tof, zonas, lidar, orientacion, hardware-test]
---

# TASK-203 — Orientación interna de las zonas de los 4 ToF

> **✅ ACTUALIZACIÓN 2026-05-31 — caracterización HECHA en banco (Gustavo).**
> Resultado: **TOF0/1/2 (frente/atrás/derecha) comparten orientación**; el
> **TOF3 (izquierdo)** está **rotado 180°** respecto a los otros 3. La
> corrección quedó implementada en `src/shared/tof_zone_orient.h` (rota 180°
> solo al izquierdo) + `test_tof_zone_orient` (7 tests host). El
> `diag_top_tof_zonemap` ahora tiene vista corregida (tecla `c`) para confirmar
> visualmente. **Lo que queda de esta task** (menor): correr el zonemap en vista
> corregida y confirmar que los 4 sensores coinciden, y repetir la
> caracterización en R2. Ver `journal/2026-05-31-top-tof-orientacion-zonas-izquierdo-180.md`.

## Por qué

Los 4 ToF ya enumeran y tienen posición confirmada (TOF0=frente, TOF1=atrás,
TOF2=derecha, TOF3=izquierda). Pero cada ToF entrega una grilla 8×8 y el orden
de sus 64 zonas depende de **cómo está montado el chip**. El ToF **IZQUIERDO es
de otro fabricante y se montó mirando hacia abajo** → puede tener su grilla
invertida en arriba/abajo y/o izquierda/derecha respecto a los otros 3.

Para el **barrido tipo lidar-360** (combinar los 4 sensores en un mapa angular
continuo alrededor del robot) cada zona tiene que mapear al ángulo real
correcto. Si un sensor tiene las columnas espejadas, su cuadrante del lidar sale
invertido. (Para el promedio de zonas actual no importa; para el lidar sí.)

Convención de referencia: `docs/CONVENCION-EJES-ROBOT.md` (izq/der primera persona).

## Herramienta

`diag_top_tof_zonemap` (ya en el repo). Imprime la grilla 8×8 cruda de cada ToF
y marca con `[###]` la zona más cercana. Comandos serie: `0/1/2/3` (un sensor),
`a` (los 4).

```
pio run -e diag_top_tof_zonemap -t upload
# QUITAR y reponer energía (las direcciones I2C persisten)
pio device monitor -b 115200
```

## Qué medir (por cada uno de los 4 ToF)

Poné un objeto (mano) en una posición CONOCIDA del campo de visión del sensor y
anotá qué fila/columna se enciende:

| Estímulo | TOF0 frente | TOF1 atrás | TOF2 derecha | TOF3 izq |
|---|---|---|---|---|
| objeto ARRIBA del FoV | fila? | | | |
| objeto ABAJO del FoV | fila? | | | |
| objeto IZQUIERDA del FoV | col? | | | |
| objeto DERECHA del FoV | col? | | | |

Con eso se deduce, por sensor, si hay flip vertical (arriba/abajo) y/o flip
horizontal (izq/der), y cuál es el "0" de columnas.

**Foco principal:** comparar TOF3 (izquierdo, otro fabricante, mirando abajo)
contra TOF0 (frontal "normal"). Si TOF3 tiene la grilla flippeada, anotarlo
explícito.

## Criterio de cierre
- Tabla de arriba completa para los 4 sensores.
- Para cada sensor: ¿flip-H? ¿flip-V? ¿rotación? (sí/no + cuál).
- Conclusión: la transformación zona→ángulo de cada sensor para el lidar-360.
- Journal con las grillas capturadas (copiar/pegar del monitor).

## Lo que viene después (firmware, no parte de esta task)
- HAL Sprint B: `sensors_tof.cpp` enumera y lee los 4 ToF.
- Lidar-360: usar las transformaciones de esta task para mapear cada zona de
  cada sensor a un ángulo absoluto alrededor del robot.

## Cambios de estado
- 2026-05-31: creada por Claude (Opus 4.8). El sensor izquierdo de otro
  fabricante montado mirando abajo motiva esta verificación. A pedido de Gustavo.
