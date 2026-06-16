---
title: "Glue de la cross-validación de salud del heading COMPLETADO (TASK-213): centinela dual-BNO @1Hz + feed primario/cámara/OTOS + telemetría, gateado"
date: 2026-06-15
author: "Claude (Anthropic - Claude Opus 4.8 1M) — coach"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M) — workflow de diseño del glue (8 agentes) + verificación adversarial"
status: final
tags: [firmware, top, bno055, imu, fusion-sensorial, centinela, otos, camaras, glue, gateado]
robot: ambos
area: control
tipo: implementacion
---

# Glue de la cross-validación del heading COMPLETADO (TASK-213)

## Contexto

Gustavo: "completá la PROGRAMACIÓN antes de que testee — no puedo compilar/banco no es
excusa para no programar". Aceptado. Esta sesión **completa el glue** que faltaba para que
la cross-validación de salud del heading (TASK-212/213) CORRA en el firmware, gateado
off-by-default. Lo único que le queda al equipo es `pio run` + banco — no esperar código.

> Un workflow (8 agentes + verificación adversarial) diseñó el glue **archivo por archivo**
> con los fixes de concurrencia. Hallazgo clave: la arquitectura no-bloqueante ISR/DMA/timer
> es **post-Incheon, NO de este glue** — el centinela+cross-validación corren ENTEROS en
> contexto de loop (sin ISR, sin IntervalTimer). El glue quedó acotado y de bajo riesgo.

## Qué se programó

**PURO (host-testeado, verificable):**
- `imu_cross_validate.h`: FIX — `sentinel_fresh_timeout_ms` (1300 ms). Sin esto la rama del
  centinela era CÓDIGO MUERTO (`fresh_timeout=300 < period=1000`). +test.
- `vel_lateral.h` (NUEVO): des-rotación de la velocidad → componente lateral (strafe), para
  el gate anti-strafe de la cámara (la cámara no mide ω si el arquero traslada). +test 6/6.
- `telemetry_top.h/.cpp`: 3 campos `xval` (verdict/score/n_indep) aditivos (sin schema bump,
  patrón "z") → la salud del heading se ve en el monitor. +test (golden actualizado).
- Gate host: **1003 tests / 0 fallos.**

**GLUE Arduino (gateado; lo compila el equipo con `pio run`):**
- `sensors_imu.cpp/.h` (BLOQUE 1): estado `g_xval`, cache del gyro_z del primario (in[0], CERO
  I²C extra), acumulador de rotación NETA en grados, `xval_feed_primary` + `xval_update` 1×/tick;
  **init del 2º BNO** al boot (begin IMUPLUS, `g_ready[1]` sigue false → byte-idéntico); getters
  de telemetría; `sensors_imu_sentinel_step()` (read @1Hz del 2º BNO + feed, con Δyaw envuelto).
- `main_top.cpp` (BLOQUE 3): feed cámara (goal_rate) + OTOS en `build_snapshot` con el gate
  anti-strafe (`vel_lateral` + `OTOS_VEL_FRAME_IS_FIELD` default robot-frame); y la llamada al
  `sentinel_step` en el branch R2 con una **ventana bus-quiet** (≥`TOP_BNO_TOF_GAP_MS` desde el
  último read de ToF en Wire) — el read del secundario NUNCA queda pegado a un getRangingData
  (la contención es justo lo que congelaba el BNO).
- `cameras_runtime.cpp/.h` (BLOQUE 3a): wrapper `cameras_goal_rate_update` (estado persistente
  del `GoalRateTracker`; la polaridad opp/own la resuelve main_top, no acá).
- `comm_down.cpp/.h` (BLOQUE 3b): accessors read-only `comm_down_get_omega_dps/omega_valid`.
- `top_telemetry_serial.cpp` (BLOQUE 3c): pobla los campos `xval` (único call-site, bajo #ifdef
  → con el flag OFF `--gc-sections` descarta los getters → binario byte-idéntico).
- `platformio.ini`: env de banco **`top_robot2_pri_xval`** (extiende `_posefusion` → hereda
  FREEZE_DETECT, así el veredicto CONGELADO funciona). `top_robot1/2/pri` byte-idénticos.

## Honestidad

Todo gateado off-by-default → **el binario de competencia es byte-idéntico** (con los flags
OFF, `--gc-sections` descarta el código nuevo). El glue Arduino NO lo compilé (sin toolchain
Teensy); su primera verificación es `pio run -e top_robot2_pri_xval` del equipo. La
matemática de decisión (el QUÉ) es 100% pura+host-testeada; el glue (el CÓMO con el hardware)
es lo compile-pending.

## Pendiente de banco (Claude NO cierra TASKs de HW) — TASK-213

1. `pio run -e top_robot2_pri_xval` (compila — caza cualquier typo del glue) + diff de `.bin`
   de `top_robot2_pri` con/sin los flags → confirmar byte-identidad.
2. **Analizador lógico en Wire** durante la ventana 1Hz: confirmar que el read del secundario
   (<10 ms) queda AISLADO (no reintroduce el freeze). ← el blocker #1, crítico.
3. Medir el piso de ruido del ω (primario + centinela) robot quieto → fijar `tol_base`/`consensus_k`.
4. Marco de `Velocity2D` del OTOS (cancha vs robot) → definir `-DOTOS_VEL_FRAME_IS_FIELD` o dejarlo.
5. Signo CCW+ del yaw del secundario y del omega OTOS.
6. Robot quieto 3-5 min → el sano NUNCA da MALO (falso-veto = riesgo #1).

## Próximos pasos

La arquitectura no-bloqueante (pizarra `sensor_slot`/`snapshot_assembler` por ISR/DMA/timer)
queda como capitalización **post-Incheon** — el loop ya está holgado (~190k/s) y no la necesita.
