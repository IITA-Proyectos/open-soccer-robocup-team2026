---
title: "Geometría física del robot (TOP) — fuente única: offsets, alturas, orientación de sensores"
date: 2026-06-16
author: "Claude (Anthropic — Opus 4.8 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: vivo
tipo: contrato-geometria
robot: ambos (medir por robot)
related: [src/shared/robot_geometry.h, tools/monitor-base/monitor_base/robot_geometry.py, docs/CONVENCION-EJES-ROBOT.md]
---

# Geometría física del robot — dónde está cada sensor

> **Fuente única.** Los números viven en `src/shared/robot_geometry.h` (firmware) y su
> espejo `tools/monitor-base/monitor_base/robot_geometry.py` (monitor). Este doc es el
> contrato humano. Si cambia el montaje: editar el header **y** el espejo (test de paridad
> `tests/test_robot_geometry.py` + `test_robot_geometry` host).

## Por qué importa
Los sensores **no están en el centro**. Un ToF a ~6 cm del centro que mide `d` a una pared
no da la distancia del **centro**; hay que corregir por el offset, que **rota con el
heading**. Y para que los **dibujos** del monitor no mientan, el origen de cada rayo se
dibuja en la posición **real** del sensor, no en el centro.

## Frame (CONVENCION-EJES-ROBOT.md)
Origen = **centro** del robot. **+X = derecha, +Y = frente**. **+heading = CCW (antihorario)**;
heading 0 = el frente (+Y robot) mira al arco rival (+Y cancha). Un offset (marco robot) se
lleva al marco cancha rotándolo por el heading (rotación CCW estándar; ver `geom_rotate_offset`).

## ⚠️ VALORES APROXIMADOS — MEDIR con calibre
Gustavo dio estimaciones ("unos 6/7 cm", Ø "unos 17 cm", alturas "12/17 cm"). Son semilla.
La corrección de distancia es tan buena como estos números → **medir** para precisión.

| Sensor | offset desde centro | altura sobre piso | bearing (mira a) |
|--------|--------------------:|------------------:|------------------|
| Robot (Ø) | radio ~85 mm (Ø ~170 mm) | — | — |
| ToF0 | ~60 mm al frente (0,+60) | ~170 mm | frente (0°) |
| ToF1 | ~60 mm atrás (0,−60) | ~170 mm | atrás (180°) |
| ToF2 | ~60 mm derecha (+60,0) | ~170 mm | derecha (90°) |
| ToF3 | ~60 mm izquierda (−60,0) | ~170 mm | izquierda (270°) — montado a 180° (ver `tof_zone_orient.h`) |
| Cámara frontal | ~70 mm al frente | ~120 mm | frente |
| Cámara trasera | ~70 mm atrás | ~120 mm | atrás |
| HC-SR04 | ~70 mm al frente | ~100 mm (no medido) | frente |

> El offset se asume **a lo largo del bearing** (un ToF que mira al frente está adelantado).
> Si el montaje real difiere (sensor corrido a un costado), medir x/y reales.

## La matemática (en el header, host-testeada)
- `geom_rotate_offset(ox, oy, heading) → (fx, fy)`: offset robot → cancha.
- `geom_center_dist_to_wall(d_sensor, Δfx, Δfy, n̂x, n̂y) = d_sensor − Δ·n̂`: distancia del
  CENTRO a la pared a partir de la que mide el sensor (Δ = offset rotado; n̂ = normal interior
  de la pared). Caso canónico: ToF frontal adelantado 60 mm → el centro está 60 mm más lejos.

## Estado / pendientes (honesto)
- **Fundación (este header + espejo + tests):** ✅ hecha.
- **Corrección offset→centro en la trilateración:** ⬜ NO cableada (paso aparte, gateado +
  banco). El signo de n̂ debe casar con la convención de paredes de `localization.cpp`.
- **Render a tamaño real en el monitor:** ⬜ BLOQUEADO por un conflicto de convención de
  heading: `gui_field.robot_rel_to_field` usa **CW+** mientras el canónico (y este header) es
  **CCW+**. Hay que reconciliar a UNA convención (confirmando el signo real del heading del
  firmware) ANTES de dibujar, o el render sale espejado. **Tema-a-analizar.**
- **Proyección por-zona (rayos diagonales):** ⬜ Fase B — requiere medir FOV/tilt reales.
- **Caveat de odometría:** la geometría fina es necesaria pero NO suficiente para XY preciso;
  el estimador sigue frágil/no-validado (≥1 ToF por eje + heading estable + ruido medido en banco).
