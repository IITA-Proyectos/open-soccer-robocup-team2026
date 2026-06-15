---
title: "Optimización de la placa TOP (no-bloqueante): plan por workflow + INC-1 gyro-guard + INC-2 pose-age (gateados)"
date: 2026-06-15
author: "Claude (Anthropic - Claude Opus 4.8 1M) — coach"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M) — 2 workflows (plan + verificación adversarial)"
status: final
tags: [control, sensores, firmware, top, tiempo-real, gateado]
robot: ambos
area: control
tipo: implementacion
---

# Optimización TOP no-bloqueante — plan por workflow + INC-1/INC-2

## Contexto

Gustavo pidió continuar el desarrollo de los programas de la placa TOP "optimizados
100%", usando metodología superpowers (Brainstorm → Plan → Execute) y un workflow en
paralelo. Las reglas duras siguen: **gateado off-by-default, binario de competencia
byte-idéntico (aditividad del flag), NO reescribir `main_top.cpp` vivo, NO RTOS, host-
testeable lo que se pueda** (Claude NO tiene toolchain Teensy ni banco).

## Brainstorm + Plan: workflow de entendimiento (no re-derivar a ciegas)

Corrí un **workflow de 11 agentes** (6 lectores en paralelo + diseño + 3 lentes
adversariales + plan final) para mapear el firmware TOP REAL antes de tocar nada.
Hallazgos clave:

- **El loop del TOP YA está muy afinado** (no rehacer): round-robin ToF + payload
  recortado → ~190k pasadas/s; TX del snapshot no-bloqueante (`availableForWrite`);
  RX por polling acotado + `addMemoryForRead` (cámaras 256 B, DOWN 512 B); I²C 1 MHz
  init / 100 kHz runtime. El "cuello a 6 Hz" del repo ya está resuelto.
- **Corrección factual**: el binario de hoy = `top_robot2_pri` (ambos robots), que ya
  NO es byte-idéntico a un histórico (agrega `TOP_BNO_PRIMARY_ONLY`+`TOP_USB_MONITOR`).
  La regla operativa correcta es **ADITIVIDAD**: el `.bin` del MISMO env con y sin el
  flag nuevo debe diferir solo por la feature (lo verifica el equipo con `pio`).
- **Pendiente real de alto valor** (no la arquitectura ISR/DMA, que es post-Incheon):
  el detector de BNO congelado (`imu_freeze`) está APAGADO porque daba falso-DEAD con
  el robot quieto (2026-06-08), y eso BLOQUEA `pose_fusion` (que lo exige por `#error`).

El plan ordenó 6 increments de menor a mayor riesgo. INC-1/2/3 son pre-Incheon
host-testeables; INC-4/5/6 (sensor_slot ISR, RX-IRQ por cámara, emisor por timer) son
post-Incheon, alto riesgo y NO host-testeables (glue ISR/DMA/IntervalTimer).

## Ejecutado (gateado, host-testeado)

### INC-1 (P0) — guarda de gyro en el detector de BNO congelado
Arregla de raíz el **falso-DEAD del robot QUIETO** (por el que se desactivó el flag el
2026-06-08) y **desbloquea `pose_fusion`**. Nueva variante PURA `imu_freeze_update_g`
(la vieja `imu_freeze_update` queda intacta = byte-idéntica): declara congelado solo si
el heading queda clavado N lecturas / T ms **Y el gyro probó rotación real
(|gyro_z| ≥ umbral) MIENTRAS el heading ya estaba clavado**.

- **Discriminador = rotación, no quietud.** Robot quieto (gyro al piso de ruido) →
  nunca congela (mata el falso-DEAD). Robot girando + heading clavado → congela (no es
  inerte). NO se usó la firma "gyro también clavado": un BNO que filtra el gyro a 0 con
  el robot quieto la haría disparar = reintroducir el falso-DEAD.
- **Sutileza corregida** (la pillé al fallar un test): el movimiento en la lectura que
  SIEMBRA o CAMBIA el heading NO cuenta (instante "vivo"); solo cuenta con el heading ya
  clavado. Si no, un giro que cambia el rumbo y luego se detiene falsearía.
- Cableado en `sensors_imu.cpp` (usa `in[i].gyro_z_dps` ya leído → CERO I²C extra), bajo
  el MISMO `#ifdef TOP_ENABLE_BNO_FREEZE_DETECT`. Envs de banco: `top_robot1_bnofreeze`
  (corregido: el flag NO estaba en el default de top_robot1) + nuevo
  `top_robot2_pri_bnofreeze`. Comentarios stale de `platformio.ini` corregidos.
- **Tests host: `test_imu_freeze` 13 → 30** (verde), incluyendo el blindaje de regresión
  del falso-DEAD (`cfg_from_rate` debe heredar el umbral), borde exacto del umbral,
  rotación negativa, saturación sin overflow, latch, jitter sub-umbral.

### INC-2 (P1) — edad fina del OTOS
`pose_age.h` (PURO): `pose_age_ms_pure` (nunca-recibido → `POSE_AGE_NEVER`=0xFFFFFFFF,
NO 0 — devolver 0 engañaría al gate creyendo el OTOS fresquísimo) + `pose_age_is_fresh`.
Getter `comm_down_pose_age_ms()` (glue). En el bloque `TOP_ENABLE_POSE_FUSION` de
`build_snapshot`, `in.otos_fresh` ahora gatea a `otos_stale_ms` (≈60 ms) en vez del
booleano grueso de 500 ms: a 100 Hz un OTOS sano llega cada ~10 ms; predecir contra un
delta de 200 ms es basura. Test host nuevo `test_pose_age` (5 casos).

### INC-3..6 — DIFERIDOS (documentado)
- **INC-3 (snapshot_assembler)**: el módulo ya está host-testeado, pero cablearlo
  reestructura `build_snapshot` y **choca con el bloque `pose_fusion`** (ambos escriben
  x/y); su valor real (centralizar el fail-safe) llega con el rewrite no-bloqueante.
- **INC-4/5/6 (sensor_slot ISR, RX-IRQ por cámara, emisor por timer)**: la arquitectura
  ISR+DMA+doble-buffer. Alto riesgo, glue NO host-testeable, y el review adversarial
  encontró un BLOCKER de concurrencia real (no leer slots en el ISR del timer si puede
  preemptar la RX-ISR que dejó `seq` impar → spin infinito). **Post-Incheon, en banco.**

## Verificación

- **Workflow de verificación adversarial (5 agentes, 4 lentes)** sobre el código nuevo:
  **0 blockers, 0 majors de corrección.** Confirmó byte-aditividad, latch, cuantización
  con signo, wrap-safety. Sus hallazgos de COBERTURA se aplicaron en el mismo commit
  (blindaje `cfg_from_rate`, bordes de umbral, rotación negativa, etc.).
- **Gate host** verde tras los cambios (los módulos puros + sus tests compilan g++; el
  glue Arduino — `sensors_imu.cpp`/`comm_down.cpp`/`main_top.cpp` — NO compila host y lo
  valida el equipo en banco).

## Pendiente de banco (Claude NO cierra TASKs de HW)

- **TASK-211** (TOP): validar el freeze-detector con guarda de gyro — medir el piso de
  ruido de |gyro_z| quieto y ajustar `IMU_FREEZE_GYRO_MOTION_CDPS`; confirmar 0 falso-DEAD
  quieto + DEAD al congelar girando.
- **TASK-210** (TOP, ampliada): titular `otos_stale_ms` (60 ms) bajo carga — si el loop
  se traba > 60 ms (p.ej. `pulseIn` HC-SR04, TASK-014), `pose_fusion` dejaría de predecir;
  medir p99 de la edad del OTOS y subir el umbral si hace falta.

## Próximos pasos

El rewrite no-bloqueante (INC-3→6) es trabajo de arquitectura post-Incheon, ya diseñado
en `ARQUITECTURA-SENSORIAL-TOP-NO-BLOQUEANTE.md` + módulos puros listos (`sensor_slot`,
`snapshot_assembler`). Requiere banco + toolchain Teensy para el glue ISR/DMA/timer.
