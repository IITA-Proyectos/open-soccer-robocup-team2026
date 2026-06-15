---
id: TASK-104
title: "Validar en banco la integración RT gateada de CENTRAL (debug-serial OFF, RX bigbuf, motor_slew)"
date_created: 2026-06-15
assigned: [virginia-viollaz, elias, gustavo-viollaz]
priority: P2
status: pending
estimated_hours: 1.5
blocks: ["activar en competencia los quick-wins RT de la CENTRAL (sacar jitter, colchón de RX)"]
blocked_by: [placa CENTRAL + TOP dando snapshot + batería cargada]
tags: [central-board, banco, rt, jitter, uart, motor-slew, gateado, post-incheon]
related:
  - journal/2026-06-15-integracion-rt-gateada.md
  - docs/firmware/ARQUITECTURA-LAZO-CENTRAL-RT.md
  - docs/firmware/HANDOFF-INTEGRACION-RT.md
---

# TASK-104 — Banco: integración RT gateada de la CENTRAL

## Por qué existe

Se cableó parte del análisis RT al firmware de la CENTRAL, **todo gateado off-by-default**
(binario de competencia byte-idéntico hoy). Activar cada mejora en competencia es un cambio
de binario → por la regla 1 (CLAUDE.md) **Claude NO lo cierra; lo valida el equipo en banco.**

## Qué validar (criterios medibles)

**Setup:** CENTRAL flasheada, TOP mandando snapshot, batería cargada, monitor serie.

1. **A1 — `CENTRAL_DEBUG_SERIAL` (sacar el jitter de 500 ms).**
   - Hoy `central_robot1/2` DEFINEN el flag (byte-idéntico). Para activar la mejora:
     **borrar `-DCENTRAL_DEBUG_SERIAL`** del env de competencia y reflashear.
   - **Criterio:** sin la telemetría USB de 500 ms, el robot se comporta igual y el
     `loop_us(max)` (si lo medís con el flag puesto antes) baja su pico periódico. Decidir
     si la telemetría hace falta en cancha (probablemente NO: en partido no hay USB).

2. **A2 — `CENTRAL_TOP_RX_BIGBUF` (colchón del link TOP→CENTRAL).**
   - Agregar `-DCENTRAL_TOP_RX_BIGBUF` al env y reflashear (sube el buffer RX de Serial7
     de 64 a 512 B).
   - **Criterio:** bajo carga (loop ocupado / freno de borde activo), `rsy=` (resyncs del
     link TOP, panel debug) **baja o queda en 0** vs sin el flag. Cero regresión esperada
     (más buffer solo ayuda; cuesta ~512 B de SRAM).

3. **motor_slew — `central_robot2_strafe_slew_bb` (rampa de arranque).**
   - Flashear el env, correr el strafe del arquero con caja negra.
   - **Criterio conducta:** el arranque es **parejo** (sin patinazo/tirón inicial); en el CSV,
     `vx` sube en **pendiente**, no en escalón. Los **reflejos siguen instantáneos**: el freno
     de borde y el STOP del árbitro NO se demoran (bypasean la rampa por diseño).
   - **Tuning:** bajar `-DCENTRAL_SLEW_DVX/DVY/DW` = más suave; subir = más reactivo.
   - **⚠️ Interacción con el kickstart** (impulso 130 PWM × 40 ms): verificar que se
     complementan (el kickstart rompe fricción estática, el slew evita el tirón posterior)
     y no se pelean. Si el arranque se siente "trabado", revisar esa interacción primero.

## Criterio de cierre

1. Los 3 puntos probados, con captura del Serial / CSV de caja negra.
2. Entrada de journal `journal/2026-06-15-...` (o fecha real) con veredicto por punto.
3. Decisión registrada: ¿se promueve A1/A2 a los envs de competencia? (motor_slew queda
   en banco hasta tunear).

## Atribución

Cableado + esta TASK: Claude Opus 4.8 (Anthropic), 2026-06-15 (requested-by Gustavo
Viollaz). La validación en banco la hace el equipo — Claude NO cierra TASKs de hardware.
