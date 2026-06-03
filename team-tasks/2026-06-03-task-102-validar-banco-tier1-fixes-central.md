---
id: TASK-102
title: "Validar en banco los 4 fixes Tier-1 de conducta de la auditoría CENTRAL (omega int16, anti-windup, data_valid gates)"
date_created: 2026-06-03
assigned: [virginia-viollaz, elias, enzo]
priority: P1
status: pending
estimated_hours: 1.5
blocks: ["dar por buenos en cancha los fixes #9/#29/#5/#13 de la auditoría"]
blocked_by: [placa CENTRAL + DOWN + batería cargada, idealmente TOP para el heading]
tags: [central-board, banco, pid, heading, data_valid, regresion, auditoria]
related:
  - research/in-progress/2026-06-03-auditoria-firmware-central.md
  - journal/2026-06-03-etapa2-tier1-fixes-central-host-tested.md
  - team-tasks/2026-06-03-task-101-banco-mitad-inferior-cinematica-y-fork-arquero.md
---

# TASK-102 — Validar en banco los fixes Tier-1 (conducta)

## Por qué existe

Etapa 2 de la auditoría: se implementaron 7 fixes host-testeables (commit `f545810`,
suite host 322 tests verde). **Cuatro de ellos cambian la CONDUCTA del robot** y por la
regla 1 (CLAUDE.md) **Claude NO los puede cerrar** — sólo el equipo, en banco. Los otros
tres (#25 telemetría, #15/#17 limpieza) no cambian conducta y no necesitan banco.

> Pre-requisito conceptual: el **#9 keystone es el signo de omega (#8 / TASK-101)**. El
> clamp ya evita el overflow, pero el SENTIDO de giro se confirma con el test de #8. Si
> podés, corré esto **junto con** el test de omega de TASK-101.

## Qué validar (criterios medibles)

**Setup:** robot SUJETO o ruedas al aire, batería cargada. CENTRAL flasheada
`central_robot1` (o robot2). Monitor serie 115200 (mirar `state`, `hdg`, y la nueva
columna `top[... rsy=]`). Ideal con TOP dando heading; si el BNO del TOP está congelado
(ver journal 2026-06-03) usar el OTOS como referencia visual.

1. **#9 — overflow de omega (no gira al revés).**
   - Forzar un error de rumbo GRANDE: girar el robot a mano ~150° respecto del setpoint
     (o setear un target lejano) de modo que el HeadingPID sature (>120°).
   - **Criterio:** el robot gira hacia el lado **CORTO** y converge. **NO** debe girar
     hacia el lado largo a casi máxima velocidad (eso sería el sign-flip que el fix
     elimina). Antes del fix, `omega*100` desbordaba int16 y se invertía.

2. **#29 — anti-windup (no sobrepasa).**
   - Llevar el robot lejos del setpoint un rato (que el output sature sostenido), después
     soltarlo para que vuelva.
   - **Criterio:** vuelve al setpoint **sin overshoot grande** ni oscilación lenta. El
     integral no debe quedar "cargado" empujando de más al cruzar el setpoint.

3. **#5 / #13 — gates de `data_valid` (no actúa con dato inválido).**
   - Forzar `data_valid=0` en DOWN: calibración sospechosa / saturación todo-blanco
     (tapar/iluminar fuerte el anillo) o el modo que ya dispara `EV_CALIB_SUSPECT`.
   - **Criterio #5:** el **arquero NO strafea** lateralmente con `data_valid=0`
     (antes el fallback por profundidad lo movía con dato basura).
   - **Criterio #13:** en `LINE_AVOID`, con `data_valid=0` y un ángulo residual, el robot
     **no retrocede hacia una dirección arbitraria**.

## Criterio de cierre

1. Los 4 puntos probados, con video/foto del setup y captura del Serial.
2. Entrada de journal `journal/2026-06-03-...` (o fecha real) con veredicto por punto.
3. Si alguno falla → NO revertir a ciegas: anotar el síntoma exacto y abrir el análisis
   (puede ser el signo de omega #8 pendiente, no el fix en sí).

## Notas

- Los fixes pasan **host-native** (lógica correcta en simulación); este test valida la
  **conducta física**, que el host no puede cubrir.
- Riesgo de los fixes: bajo. Son defensivos (saturar, no integrar de más, no usar dato
  inválido). El peor caso es "se mueve menos" (conservador), no "se descontrola".

## Atribución

Fixes + esta TASK: Claude Opus 4.8 (Anthropic), 2026-06-03 (requested-by Gustavo
Viollaz). La validación en banco la hace el equipo humano — Claude NO cierra TASKs de
hardware (regla 1 CLAUDE.md).
