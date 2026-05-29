---
id: TASK-200
title: "Validar en hardware los 2 fixes de firmware del TOP (heading al CENTRAL + loop sin stall HC-SR04)"
date_created: 2026-05-29
date_due: 2026-06-07
assigned: [mariaviollaz, elias]
priority: P1
status: pending
estimated_hours: 2
blocks: [TASK-037]
blocked_by: [TASK-006]
tags: [firmware, top-board, heading, hc-sr04, timing, hardware-test]
---

# TASK-200 — Validar en hardware los 2 fixes de firmware del TOP

> Primera TASK del rango TOP (200-299). Surge de la auditoría independiente del
> 2026-05-29 (`journal/2026-05-29-auditoria-top-pre-incheon-top.md`).

## Por qué

En la auditoría se arreglaron 2 riesgos de mal funcionamiento en el firmware
del TOP. **Compilan limpio en ambos robots, pero NADIE los probó en hardware.**
Claude no puede cerrar esto — lo cierra el equipo con la placa en la mano.

Los 2 fixes:

1. **Heading al CENTRAL ahora viene del IMU** (`main_top.cpp`). Antes el TOP
   mandaba `pose.heading_centideg` de `localization`, que es **siempre 0** con
   el hardware actual (TOFs solo en eje Y → pose nunca válida). El CENTRAL
   navegaba creyendo que el robot siempre apunta al arco rival. Ahora manda
   `sensors_imu_get_heading_centideg()` (los 2 BNO055 reales).

2. **HC-SR04 apagado por default** (`sensors_tof.cpp`, `#ifdef TOP_ENABLE_HCSR04`).
   El `pulseIn` sobre el pin 7 (= Serial2 RX2, el uplink al CENTRAL) colgaba el
   loop 25 ms y metía basura en `min_obstacle_mm`. Sin el flag, no toca esos
   pines. (El período exacto del loop con osciloscopio sigue siendo TASK-014.)

## Qué necesito

### Parte A — Heading llega al CENTRAL y es real
1. Robot encendido **apuntando al arco rival** (+Y) al boot (calibra el offset).
2. Abrir el monitor serie del **CENTRAL** (o el debug que imprima
   `world_model_get_my_heading_deg()`).
3. Girar el robot a mano: **0° → +90° → +180° → -90°**.
4. **Criterio de aceptación:** el heading reportado por el CENTRAL **sigue el
   giro físico** con error ≤ ±5°. (Antes del fix quedaba clavado en ~0° sin
   importar cómo girabas el robot.)

### Parte B — El loop ya no se cuelga con el HC-SR04
1. Confirmar que el firmware se compiló **sin** `-DTOP_ENABLE_HCSR04` (default).
2. Con el robot corriendo, verificar en el debug del TOP que `min_obst` ya no
   muestra valores erráticos saltando (antes el HC-SR04 metía ruido).
3. **Criterio de aceptación:** el uplink TOP→CENTRAL mantiene su cadencia (el
   CENTRAL no reporta timeouts del snapshot > 500 ms) durante 2 min continuos.
4. La medición fina del período del loop (osciloscopio, peor caso) es **TASK-014**
   — no hace falta repetirla acá, pero si ya la hacen, anoten el número.

## Regresión (no romper lo vecino)
- Cámaras siguen detectando pelota/arco (el fix no las toca, pero confirmar).
- Odometría DOWN→TOP sigue llegando sin corromperse (contador CRC del enlace).

## Criterio de cierre
- [ ] Parte A: heading del CENTRAL sigue el giro físico (±5°). Foto/video o log.
- [ ] Parte B: uplink estable 2 min sin timeouts. Log.
- [ ] Regresión OK (cámaras + odometría).
- [ ] Resultado anotado en un journal `journal/YYYY-MM-DD-*.md`.
- [ ] Si algo falla → NO cerrar, escribir el síntoma y volver a Claude.

## Notas / decisiones
_(completar al ejecutar)_

## Cambios de estado
- 2026-05-29: creada por Claude (Opus 4.8) tras la auditoría independiente del
  TOP, a pedido de Gustavo Viollaz. Mejora la validez de TASK-037
  (diag_central_drive usa el heading que este fix corrige).
