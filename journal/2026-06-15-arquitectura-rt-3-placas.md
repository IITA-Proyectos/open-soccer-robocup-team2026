---
title: "Análisis de arquitectura RT de las 3 placas (TOP/CENTRAL/DOWN) + módulos puros gateados"
date: 2026-06-15
author: "Claude (Anthropic - Claude Opus 4.8 1M) — coach"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M) — 6 workflows en paralelo"
status: final
tags: [control, sensores, comunicacion, analisis, ambos, alta]
robot: ambos
area: control
tipo: analisis
---

# Análisis de arquitectura RT de las 3 placas + módulos puros gateados

## Contexto

Tras optimizar el lazo de control del arquero (fast-BNO + amortiguación PD, ya en
`main`), Gustavo pidió llevar el mismo lente de tiempo real a TODO el robot: que
cada placa procese **sin demoras**, lea sensores y puertos serie **en paralelo**
(no bloqueante), y mande datos lo antes posible. Se analizaron las 3 placas con
workflows en paralelo, **alcance: diseño + andamiaje gateado off-by-default**, sin
tocar el firmware vivo (integración post-Incheon en banco).

## Qué se hizo

**5 documentos de diseño** (todos verificados contra el código vivo, `archivo:línea`):

- `docs/firmware/ARQUITECTURA-SENSORIAL-TOP-NO-BLOQUEANTE.md` — pizarra + RX-IRQ +
  emisor por timer; saca las lecturas I²C/`pulseIn` del camino del snapshot.
- `docs/firmware/ESTIMACION-FUSION-TOP.md` — la capa "estimador" (predict@100Hz +
  correct + gate); 24 mejoras priorizadas; mucho ya escrito sin cablear
  (`pose_fusion`, `pose_filter`, `imu_freeze`, `ball_sticky`, `otos_health`).
- `docs/firmware/ARQUITECTURA-LAZO-CENTRAL-RT.md` — 4 capas (RX-ISR → pizarra →
  reflejos → control suave → **FSM prolija**) con timeouts no-bloqueantes.
- `docs/firmware/GUIA-DE-TUNING-CENTRAL.md` — todos los parámetros tuneables del
  firmware VIVO, por "qué querés cambiar"; verificado 100 % (~75 params, 0 disc.).
- `docs/firmware/ARQUITECTURA-LAZO-DOWN-RT.md` — lectura dual-ADC (~717→~50-60 µs),
  detección temprana + vector de escape, tolerancia a fallas (sensor defectuoso /
  robot levantado).

**5 módulos puros host-testeados** (gateados off-by-default, firmware vivo intacto):
`sensor_slot.h` (pizarra seqlock), `snapshot_assembler.h`, `state_timer.h`
(timeout no-bloqueante por estado), `motor_slew.h` (control suave + bypass de
reflejos), `line_early_escape.h` (detección temprana + vector + tolerancia a fallas).

## Qué se midió/observó

- **Patrón común a las 3 placas:** superloop cooperativo donde una lectura
  bloqueante (I²C / `pulseIn` / barrido de muxes) comparte el loop con el envío →
  jitter/latencia. Cura común: I/O no-bloqueante (ISR/DMA) + pizarra doble-buffer +
  consumidor a tasa fija. Sin RTOS (la skill `rtos-scheduling-embebido` lo confirma).
- **Quick-wins reales en el firmware VIVO** (del review adversarial):
  - CENTRAL: el bloque de `Serial.print` de debug NO está gateado (`main_central.cpp:341`)
    → pico de jitter cada 500 ms si el USB se llena. Y el RX del TOP (Serial7) tiene
    solo 64 B de buffer vs 512 del DOWN → snapshot se pierde en silencio.
  - Estimación: cablear lo que ya existe es alto valor / bajo esfuerzo, **pero el
    rumbo es la raíz** (interlock de compilación `imu_freeze` antes de `pose_fusion`).
  - DOWN: el OTOS bloqueante roba ~0.6 ms al barrido de línea (el IntervalTimer lo
    resuelve, pero es P2).
- **Catches finos de concurrencia** (TOP I/O): la barrera del seqlock debe ser
  `__DMB()` con clobber `:::"memory"` (NO el `dsb` pelado del watchdog); el seqlock
  es correcto por single-writer + lector wait-free, NO por "ISR no interrumpe a ISR"
  (falso en el M7 por preempción anidada del NVIC).
- **Gate host:** los 5 módulos puros compilan y pasan (`scripts/run-host-tests.sh`,
  g++ de Webots). Verificado tras el power-cut.

## Conclusión

El robot tiene un análisis de tiempo real completo de sus 3 placas, con un patrón
de arquitectura único (no-bloqueante + pizarra + consumidor) y un backlog priorizado
de mejoras. **Nada toca el firmware de competencia:** todo es diseño + módulos
gateados off-by-default. Los quick-wins reales (debug-print, buffer Serial7) son
chicos y de bajo riesgo. La integración la cierra el equipo en banco post-Incheon.

## Próximos pasos (banco — Claude NO cierra TASKs de hardware)

1. Implementar los 2 quick-wins del lazo CENTRAL gateados (debug-print + buffer Serial7).
2. Medir WCET / ruido de sensores ANTES de tunear cualquier filtro (pre-requisito).
3. Estabilizar el heading (raíz) antes de cablear la fusión de pose.
4. Integrar los módulos puros placa por placa, cada uno detrás de su flag, en banco.
