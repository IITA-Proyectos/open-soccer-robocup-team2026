---
id: TASK-211
title: "Validar en banco el freeze-detector del BNO con GUARDA DE GYRO (medir piso de ruido gyro ANTES)"
date_created: 2026-06-15
assigned: [virginia-viollaz, elias, gustavo-viollaz]
priority: P1
status: pending
estimated_hours: 2
blocks: ["re-activar TOP_ENABLE_BNO_FREEZE_DETECT en produccion", "habilitar pose_fusion (lo exige por #error)"]
blocked_by: [placa TOP + BNO sano + forma de simular/forzar un freeze del BNO girando]
tags: [top-board, banco, bno, heading, freeze, gyro, gateado, post-incheon]
related:
  - journal/2026-06-15-top-optimizacion-no-bloqueante.md
  - src/shared/imu_freeze.h
  - team-tasks/2026-06-15-task-210-banco-pose-fusion-top.md
---

# TASK-211 — Banco: freeze-detector del BNO con guarda de gyro

## Por qué existe

`TOP_ENABLE_BNO_FREEZE_DETECT` fue QUITADO de los envs el 2026-06-08 porque daba
**falso-DEAD con el robot QUIETO** (mataba el único heading sano). INC-1 (2026-06-15)
agregó una **guarda de gyro** (`imu_freeze_update_g`): solo declara congelado si el gyro
probó que el robot ESTABA GIRANDO mientras el heading quedó clavado → el robot quieto
queda a salvo. Host-testeado (`test_imu_freeze` 31/31), pero la regla 1 (CLAUDE.md) exige
banco: **Claude NO cierra esto**.

## Pre-requisito DURO (medir ANTES de re-activar en producción)

**Medir el piso de ruido de |gyro_z| con el robot QUIETO** (varios minutos, distintas
superficies/temperaturas). El umbral `IMU_FREEZE_GYRO_MOTION_CDPS` (default 500 cdeg/s =
5 deg/s) debe quedar BIEN por encima de ese piso. Si el BNO en NDOF reporta picos de
ruido > 5 deg/s estando quieto, subir el umbral (o el falso-DEAD podría volver por otra
puerta). Anotar el valor medido.

## Qué validar (criterios medibles)

**Setup:** `pio run -e top_robot2_pri_bnofreeze -t upload` (o `top_robot1_bnofreeze`).
Monitor del TOP mirando `heading_valid` / `hdg` del snapshot.

1. **NO falso-DEAD quieto:** robot perfectamente QUIETO ≥ 5 min → `heading_valid` NUNCA
   cae a 0, `hdg` no se clava en 0. (Este es el bug histórico que la guarda mata.)
2. **SÍ detecta freeze real girando:** con el robot GIRANDO a mano (> 5 deg/s), forzar/
   simular un BNO congelado (p.ej. contención de bus, o un mock que repita el yaw) →
   `heading_valid` cae a 0 (DEAD) dentro de ~1,5–2 s.
3. **Recuperación:** al descongelar (el heading vuelve a cambiar), el detector se limpia
   y `heading_valid` vuelve a 1.
4. **Aditividad:** el `.bin` de `top_robot2_pri` CON y SIN `-DTOP_ENABLE_BNO_FREEZE_DETECT`
   debe diferir SOLO por el detector (prueba de que el binario de competencia no cambia
   mientras el flag esté OFF).

## Criterio de cierre

1. Piso de ruido del gyro medido y umbral ajustado/confirmado.
2. Los 4 puntos probados con captura del Serial. Veredicto en journal.
3. Decisión: ¿se re-activa `TOP_ENABLE_BNO_FREEZE_DETECT` en producción? (habilita además
   la rama `pose_fusion` / TASK-210).

## Atribución

INC-1 + esta TASK: Claude Opus 4.8 (Anthropic), 2026-06-15 (requested-by Gustavo
Viollaz). Validación en banco = equipo humano; Claude NO cierra TASKs de hardware.
