---
title: "TOP de competencia = firmware COMPLETO y rápido (RT) — decisión de Gustavo + fallback 'anterior'"
date: 2026-06-16
author: "Claude (Anthropic — Opus 4.8 1M), sesión con Gustavo"
status: vivo
tipo: decision + journal
robot: robot2 (R1 pendiente)
related: [platformio.ini, docs/firmware/ROBOT-GEOMETRY.md, IMPL-PIZARRA-Y-EMISOR-TOP-2026-06-16.md]
---

# El TOP de competencia ahora corre el firmware COMPLETO (RT)

## Decisión (Gustavo, 2026-06-16)
"Poné todas las mejoras en el TOP de competencia y dejá el anterior versionado, por las
dudas; las pruebas se hacen con el firmware completo y rápido." → El env de competencia
`top_robot2_pri` (default_envs) pasa a incluir **todas** las mejoras RT; el stock anterior
queda como `top_robot2_pri_anterior` (fallback).

## Qué cambió (platformio.ini, familia top_robot2_pri)
- **`top_robot2_pri`** (COMPETENCIA, default) ahora trae:
  `-DTOP_ENABLE_SNAPSHOT_TIMER -DTOP_BNO_FAST -DTOP_ENABLE_HCSR04_ASYNC -DTOP_ENABLE_TOF_SCHED
   -DTOP_ENABLE_BNO_SENTINEL -DTOP_ENABLE_BNO_FREEZE_DETECT` (+ los previos PRIMARY_ONLY + USB_MONITOR).
  = emisor @100Hz desacoplado (seqlock endurecido volatile+__DMB+reader acotado) + frescura
  por-sensor + BNO@100Hz (latencia rumbo ~50→~10ms) + HC-SR04 no-bloqueante + round-robin con
  skip + centinela del 2º BNO + freeze-detector del primario. (NO incluye pose_fusion ni xval —
  siguen experimentales en sus envs.)
- **`top_robot2_pri_anterior`** (NUEVO, fallback) = el stock anterior (primario-solo, sin RT).
  Byte-idéntico al viejo top_robot2_pri. Para flashear si el completo se porta mal.
- **`top_robot2_pri_rt`** = ahora ALIAS de top_robot2_pri (idéntico; queda por compat de refs).
- El enmascarado de zonas ToF (A2.2) ya estaba ungated → está en AMBOS (no-op por default).

## ⚠️ Riesgo asumido (honesto)
El camino RT (ISR/IntervalTimer del emisor + freeze-detector) **NO está validado en hardware**.
Ponerlo de default = la validación es **por uso en banco**. El más riesgoso es el
**freeze-detector**: un falso-CONGELADO marca heading inválido → CENTRAL deja de navegar por
rumbo. Mitigaciones: (1) el fallback `top_robot2_pri_anterior` es el escape inmediato;
(2) Gustavo prueba con el completo (esa ES la validación). **Plan de prueba obligatorio antes de
confiar en un partido:** T1-T7 del IMPL-PIZARRA doc + el make-or-break del freeze-detector
(robot quieto 3-5 min → "Rumbo" NUNCA debe pasar a CONGELADO; girando → trackea y un freeze real
sí lo marca).

## Verificación (host/compilación; NO hardware — regla #1)
`pio run` SUCCESS: `top_robot2_pri` (completo), `top_robot2_pri_anterior` (stock),
`top_robot2_pri_rt` (alias), `_fastbno`, `_xval`. Host suite previa verde (1194 + geometría).
Tamaños: competencia 535,817 B vs anterior 524,297 B (la diferencia = el código RT).

## R1 — PENDIENTE
El equivalente para ROBOT1 queda PENDIENTE (R1 no estaba a mano). ⚠️ NO es copiar los flags:
R1 hoy NO tiene BNO sano → TOP_BNO_FAST/centinela/freeze-detector (dependen del BNO) NO aplican
igual; sí aplicarían el emisor/HC-SR04 async/round-robin. Ver team-task TASK-217. Hacerlo CON R1
a mano para validar.

## Cómo flashear R2 (lo que tiene Gustavo enfrente)
- Completo (default): `cd "software/teensy/Soccer 2026"; pio run -e top_robot2_pri -t upload`
  (o el .hex `software/teensy/Soccer 2026/TOP_R2_competencia.hex` en Teensy Loader).
- Fallback: `pio run -e top_robot2_pri_anterior -t upload` (o `TOP_R2_anterior.hex`).
