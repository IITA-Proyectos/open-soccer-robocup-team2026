---
id: TASK-022
title: "Cámara operativa: sentinel, crash bytearray, exposición fija, calibración mm+LAB, 1 script por cámara"
date_created: 2026-05-18
assigned: [mariaviollaz]
priority: P0
status: pending
estimated_hours: 24
blocks: [percepción de pelota/arcos en Incheon]
tags: [vision, camara, openmv, firmware, calibracion]
depends_on: []
---

# TASK-022 — Cámara operativa

## Por qué importa (P0)

La auditoría independiente (`research/completed/2026-05-18-estado-firmware-robot-evaluacion-critica.md`)
encontró que **no hay 2 programas de cámara**: hay **1 script genérico de demo**
(`software/vision/enviar coordenadas 2 arcos y pelota`) con fallas P0:

- **Sentinel roto:** sin pelota, la OpenMV manda `(0,100)`; el parser del TOP
  (`src/top/cameras.cpp:14-16`) lo interpreta como **pelota visible en el
  origen** → el robot persigue una pelota fantasma permanente.
- **Crash:** `bytearray()` con coordenada negativa (homografía) → la cámara
  muere en partido (`enviar...:155`).
- **Auto-WB y auto-gain ENCENDIDOS** (`:31-32`): thresholds LAB se invalidan
  con la luz de Incheon.
- Homografía hardcodeada marcada "(ajustar)"; `CAMERA_UNIT_TO_MM=10.0`
  placeholder; `pixels_threshold=7` (ruido).

## Pasos concretos

1. Definir UN sentinel "no detectado" único y alinear OpenMV ↔ `cameras.cpp`
   (ej. OpenMV manda `Y_coded=0`→`Y=-100`, que es lo que el parser espera).
2. Clampear TODAS las coords a `[0,255]` antes de `bytearray` (anti-crash).
3. `set_auto_whitebal(False)`, `set_auto_gain(False)`,
   `set_auto_exposure(False, exposure_us=…)` con valor medido en cancha.
4. Calibrar thresholds LAB de pelota 2026 + arcos en la cancha de Incheon
   (ver skill `openmv-vision-tuning`); subir `pixels_threshold` a un valor sano.
5. Calibrar `CAMERA_UNIT_TO_MM` midiendo pelota a 30/50/80/100 cm con cada cámara.
6. **2 scripts (o 1 parametrizado) — uno por cámara**: homografía/mirror/FOV
   propios de frontal y trasera (hoy hay 1 genérico).
7. Quitar `print()` del loop; medir fps real.

## Criterio de cierre

- [ ] Cámara tapada → TOP marca `ball_visible=false` (sentinel OK, sin fantasma).
- [ ] Sin crash con coords extremas (test de inyección).
- [ ] Exposición/WB/gain fijos; detección estable bajo luz de Incheon.
- [ ] Distancias en mm verificadas contra regla (error < ~10%).
- [ ] Script por cámara con su homografía; fusión 2 cámaras correcta.

## Plan de prueba en hardware real

1. **Setup:** robot armado, 2 cámaras montadas, cancha + pelota 2026.
2. Tapar cada cámara → verificar `cameras_front/back_alive` y `ball_visible`.
3. Pelota a distancias conocidas → comparar mm reportados vs regla.
4. Variar iluminación (apagar/encender luces) → detección no se cae.
5. Regresión: fusión dual da posición de pelota coherente moviéndola por la cancha.

## Notas / decisiones

_(completar al ejecutar — registrar exposure_us y thresholds calibrados + foto)_

## Cambios de estado

- 2026-05-18: creada por Claude tras la evaluación crítica del firmware, a
  pedido de Gustavo Viollaz.
