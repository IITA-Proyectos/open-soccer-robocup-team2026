---
title: "Predicción de rumbo (predict step): transmitir heading_crudo + ω·Δt en vez del dato quieto (cableado gateado, off=byte-idéntico)"
date: 2026-06-17
author: "Claude (sesión coach — Opus 4.8)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: código aplicado + host-tested + compila (ambos envs SUCCESS); cierre = banco (equipo, regla #1)
tipo: research + implementacion-gateada
---

# Resumen

Pedido de Gustavo: en vez de mandar a la CENTRAL el último heading medido "quieto", mandar el
dato viejo MEJORADO con una estimación de la posición actual — `ángulo ≈ ángulo_anterior + ω·Δt`
(extrapolación lineal). Es el **paso de predicción** de un estimador predict/correct (dead
reckoning de 1er orden / filtro alpha-beta). Se analizó (workflow 11 agentes + datasheets +
verificación adversarial) y se **implementó para el HEADING**, gateado off-by-default → competencia
byte-idéntica. **Nada validado en banco (regla #1): lo cierra el equipo (TASK-218).**

# Por qué solo el heading (y no la pelota / línea / obstáculo)

- **Heading: SÍ, el mejor candidato.** La ω viene del giroscopio (sensor que mide la tasa DIRECTO)
  → derivada LIMPIA. Además mitiga el freeze del BNO (si el heading se congela pero el gyro vive,
  puentea el rumbo). Velocidad MEDIDA = la regla #1 le da vía libre.
- **Pelota: NO se tocó.** Su velocidad se DIFERENCIA de posiciones de cámara ruidosas; ya existe
  `ball_predict` (el arquero anticipa la X). Extrapolar la ENTRADA encima sería doble-conteo con el
  término D del PID → temblor. Mejora futura (P2): compensar la edad DENTRO de `ball_predict` + gate
  de salto de dirección.
- **Línea / obstáculo: NUNCA.** Señales de seguridad sin velocidad física → extrapolar inventa
  cruces/obstáculos fantasma. ZOH siempre.
- **Pose XY:** ya está `pose_fusion` gateado aparte (otro predict step).

# Qué se hizo (host-tested + compila)

- **Módulo PURO `src/shared/heading_predict.h`** con las 3 reglas duras horneadas:
  (a) ω medida de entrada (clamp ±600°/s), (b) **CAP `max_extrap_ms`~60 ms** — hallazgo P0: el único
  guard de dato-muerto era `SNAPSHOT_TIMEOUT_MS=500`, demasiado grueso para un extrapolador,
  (c) deadband 2°/s + re-anclaje en cada muestra fresca. Host-tested: `test_heading_predict` **14/14**.
- **Cableado gateado `-DTOP_ENABLE_HEADING_PREDICT` (default OFF):**
  - `sensors_imu.cpp`: alimenta el predictor en el tick con el heading fusionado + `in[0].gyro_z_dps`
    (la ω del primario YA leída → cero I²C extra) + getter `sensors_imu_get_heading_centideg_predicted()`
    que extrapola al instante de uso + reset en `recalibrate_zero`.
  - `main_top.cpp` (build_snapshot) y `snapshot_emitter.cpp` (emisor por timer de competencia): los
    **dos** sitios de transmisión usan el heading predicho bajo el flag; el `#else` es la línea de hoy.
  - El heading CRUDO queda INTACTO para `imu_freeze` (necesita igualdad exacta) y `localization`.
  - Env de banco `top_robot2_pri_hpredict` en `platformio.ini`.

# Verificación

- **`test_heading_predict` 14/14** (la matemática: extrapola, cap, deadband, clamp, re-anclaje,
  freeze-bridge, wrap, inválido, Δt real). Suite host completa sin regresión.
- **`pio run -e top_robot2_pri` (competencia, flag OFF): SUCCESS.** FLASH code:79408 data:102584.
- **`pio run -e top_robot2_pri_hpredict` (flag ON): SUCCESS.** FLASH code:79792 (+384 B) data:102584
  (IDÉNTICA) — RAM +32 B. → competencia **byte-idéntica por construcción** (todo bajo `#ifdef`/`#else`-original);
  el flag agrega solo el módulo.

# Lo que NO resuelve (honestidad)

Extrapolar descuenta LATENCIA, NO crea información nueva: el techo de dato fresco lo fija el sensor
(BNO fusión 100 Hz) y el bus (ToF ~8 Hz). No reemplaza subir la tasa de sensado ni el fix del freeze
del BNO. La predicción se hace en el TOP (compensa el staleness intra-TOP + freeze-bridge); la versión
consumidor-side (CENTRAL, que compensa también el transporte) requiere subir ω al snapshot → post-Incheon.

# Pendiente (banco, equipo — TASK-218)

A/B de la "medialuna" del strafe del arquero (período/amplitud de oscilación) con `top_robot2_pri` vs
`top_robot2_pri_hpredict`; girar 360° y ver el rumbo más fresco sin overshoot; titrar `max_extrap_ms`/
`deadband`. Escape: flashear `top_robot2_pri`. Doc: `research/completed/2026-06-17-extrapolacion-predict-step.md`.

# Nota de higiene del repo

Al empezar había un cambio NO commiteado ajeno en `src/central/strategy.cpp` (no de esta sesión).
NO se tocó ni se incluyó en el commit (se stagearon solo los archivos de esta tarea).
