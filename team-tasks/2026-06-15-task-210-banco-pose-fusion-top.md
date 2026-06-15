---
id: TASK-210
title: "Validar en banco pose_fusion+pose_filter del TOP (medir ruido + signo/eje del delta OTOS ANTES)"
date_created: 2026-06-15
assigned: [virginia-viollaz, elias, gustavo-viollaz]
priority: P2
status: pending
estimated_hours: 3
blocks: ["usar pose absoluta fusionada (ToF+OTOS) en el WorldSnapshot"]
blocked_by: [placa TOP + DOWN con 2 OTOS fluyendo + ToF que ancle (≥1 eje X y 1 eje Y) + BNO sano]
tags: [top-board, banco, pose, fusion, otos, tof, heading, gateado, post-incheon]
related:
  - journal/2026-06-15-integracion-rt-gateada.md
  - docs/firmware/ESTIMACION-FUSION-TOP.md
  - .claude/skills/fusion-pose-odometria-landmarks/SKILL.md
---

# TASK-210 — Banco: estimador de pose fusionada del TOP

## Por qué existe

Se cablearon `pose_fusion` (complementario ToF+OTOS) + `pose_filter` en
`main_top.cpp::build_snapshot`, gateados tras `-DTOP_ENABLE_POSE_FUSION`
(env `top_robot2_pri_posefusion`). **Default OFF → binario byte-idéntico.** Activarlo es un
cambio de binario y, sobre todo, **cambia de qué confía el robot para su posición** → lo
valida el equipo en banco.

> **INTERLOCK ya enforced en compilación:** el env enciende también
> `-DTOP_ENABLE_BNO_FREEZE_DETECT` (un `#error` lo exige). El heading es la raíz: un rumbo
> congelado rota todo el mapa. No se puede probar la fusión sin el freeze-detector.

## Pre-requisito DURO (hacer ANTES de confiar en x/y)

La skill `fusion-pose-odometria-landmarks` lo dice: **medir el ruido ANTES de tunear.**

1. **Signo/eje del DELTA OTOS vs marco de cancha.** `pose_fusion` integra el DELTA de la
   OTOS sin rotarlo al marco de cancha → asume que el marco OTOS ≈ marco cancha. Empujar el
   robot +X y +Y a mano y confirmar que el delta OTOS va en el sentido correcto. Si está
   rotado/invertido, la predicción "se va para el otro lado" — anotarlo (es fix de glue, no
   del módulo).
2. **¿El ToF ANCLA?** Hoy con ToF sólo en el eje Y, `localization` casi nunca da `valid` →
   `pose_fusion` NUNCA inicializa → `build_snapshot` cae al comportamiento de hoy (la fusión
   es inerte). Confirmar si con la disposición actual de ToF la pose llega a `valid` alguna
   vez. Si no, la fusión no aporta hasta tener un ToF de eje X.

## Qué validar (criterios medibles)

**Setup:** `pio run -e top_robot2_pri_posefusion -t upload`. Monitor del TOP / caja negra
de CENTRAL (mira `my_x/my_y/my_pose_confidence` del snapshot).

1. **Inercia segura:** con el ToF sin anclar, `my_x/my_y/confidence` deben ser **idénticos**
   a `top_robot2_pri_fastbno` (la fusión no toca nada hasta anclar). ← red de seguridad.
2. **Anclaje:** cuando el ToF da `valid`, la pose se "pega" al absoluto sin saltar (el gate
   de salto de `pose_fusion`/`pose_filter` corta teleports).
3. **Deriva acotada:** moviendo el robot, la pose fusionada sigue la real sin irse (la OTOS
   predice, el ToF corrige). Sin ToF, deriva pura de OTOS (esperado).

## Criterio de cierre

1. Pre-requisito (signo OTOS + ¿ancla el ToF?) resuelto y anotado.
2. Los 3 criterios probados con captura. Veredicto en journal.
3. Decisión: ¿se promueve a producción o queda pendiente de un ToF de eje X? (probablemente
   lo segundo, según `ESTIMACION-FUSION-TOP.md`).

## Atribución

Cableado + esta TASK: Claude Opus 4.8 (Anthropic), 2026-06-15 (requested-by Gustavo
Viollaz). Validación en banco = equipo humano; Claude NO cierra TASKs de hardware.
