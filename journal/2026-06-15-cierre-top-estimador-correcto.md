---
title: "Cierre host-testeable de la placa TOP: estimador pose_fusion numéricamente correcto (rot_lut + seed-gate + heading_valid) + clamp drive_straight"
date: 2026-06-15
author: "Claude (Anthropic - Claude Opus 4.8 1M) — coach"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M) — workflow de inventario (10 agentes) + verificación adversarial"
status: final
tags: [firmware, top, estimador, pose-fusion, odometria, host-test, gateado]
robot: ambos
area: control
tipo: implementacion
---

# Cierre host-testeable de la TOP — estimador numéricamente correcto

## Contexto

Gustavo pidió terminar el desarrollo de TODOS los módulos/funcionalidades de la placa
TOP. Un **workflow de inventario (10 agentes + verificación adversarial)** mapeó el TOP
completo y separó lo terminable host-testeable del glue Arduino de banco. **Verdad central
(honesta):** el pipeline de competencia ya está vivo y byte-idéntico; la mayor parte del
remanente es glue Arduino (ISR/DMA/timer/Wire) que SOLO el equipo valida en banco. Lo que
SÍ se puede terminar host-testeable son **3 agujeros numéricos del estimador `pose_fusion`**
(que está cableado pero **numéricamente MAL al girar** — deuda escondida) + un clamp.

> El review adversarial corrigió 2 cosas del plan: (1) `rot_lut` es un módulo NUEVO (la LUT
> seno+círculo-completo no existía; `localization.cpp` solo tiene cos[0,90] clampeado), no un
> "port"; (2) eliminó un H7 falso (consumir ball_vx/vy en CENTRAL **ya está vivo**, no era pendiente).

## Qué se hizo (PURO, host-testeado, gateado — binario de competencia byte-idéntico)

- **H1 — `src/shared/rot_lut.h` (NUEVO) + des-rotación del delta OTOS** (bug #8). `pose_fusion`
  integraba el delta OTOS **crudo** (`st.x_mm_q0 += dx`), pero el delta viene en el marco de la
  OTOS, no en el de la cancha → al GIRAR la pose se iba en la dirección equivocada. `rot_lut.h`
  da sin/cos Q12 de círculo completo + rotación de vectores en enteros; `pose_fusion.cpp` PASO 2
  ahora des-rota el delta por `net = bno_heading − otos_heading`. Con headings iguales/0 → net 0
  → identidad (byte-idéntico al caso viejo). Tests: `test_rot_lut` 8/8 + `test_pose_fusion` (2 nuevos).
- **H2 — gate de SEED por consenso** (bug #21). El seed anclaba el origen con la PRIMERA lectura
  ToF; un rival contra la pared al boot rotaba el mapa todo el partido. Ahora exige
  `seed_min_samples` (default 3) lecturas ToF consistentes (dentro de `seed_tol_mm`) antes de
  anclar; un outlier lejano reinicia el consenso. Pure, host-tested.
- **H3 — `heading_valid` en `PoseFusionInputs`** (bug #22). La pose ToF se calcula USANDO el
  heading; con heading muerto/congelado esa pose es basura. Ahora el seed y la corrección ToF se
  saltean si `heading_valid=false` (la pose se mantiene por predicción). Cableado en `main_top`
  (gateado) desde `sensors_imu_get_heading_valid()`. **Precondición multi-agente cumplida:**
  `git fetch` + `git log origin/main -- pose_fusion.h` confirmó que solo yo lo había tocado.
- **H6 — clamp de omega en `drive_straight`** (bug #24). `drive_straight_compute` producía
  `omega_deg_s = kp·err` sin tope; un target de heading REAL desbordaba el centideg int16 del
  caller (×100). Ahora clampea en FLOAT a ±`omega_max_deg_s` (327 °/s; sin `<cmath>`). Aditivo
  (error chico = sin cambio). Test nuevo.

## Honestidad central (del review)

**Aun con H1–H3/H6, el binario de competencia HOY sigue BYTE-IDÉNTICO y NADA cambia en cancha.**
Estos fixes NO encienden nada: dejan el estimador `pose_fusion` **numéricamente correcto para el
día de banco** (hoy estaba cableado pero mal al girar). El aporte real es eliminar deuda escondida,
NO cambiar comportamiento. Encender `TOP_ENABLE_POSE_FUSION` en vivo sigue siendo BANCO (validar
freeze-detect + signo/eje del OTOS, TASK-210/211).

## Qué queda (glue Arduino → banco, ya documentado)

El no-bloqueante ISR/DMA/timer (pizarra `sensor_slot`/`snapshot_assembler`), el centinela dual-BNO
@1Hz (TASK-213), el cableado de `imu_cross_validate`/`goal_rate_tracker` (necesita `vel_lateral`,
dato huérfano hoy), y la calibración de cámaras `CAMERA_UNIT_TO_MM` (TASK-022, bloqueante #1). Todo
es banco o capitalización 2027 — no host-testeable por Claude.

## Verificación

Gate host verde tras los increments (módulos PUROS + tests g++). `test_rot_lut` 8 +
`test_pose_fusion` 16 (11→16) + `test_drive_straight` 10 (9→10). El glue de `main_top.cpp`
(rellenar `otos_heading`/`heading_valid`) es Arduino, lo compila el equipo; es aditivo bajo
`TOP_ENABLE_POSE_FUSION` (off) → binario byte-idéntico.
