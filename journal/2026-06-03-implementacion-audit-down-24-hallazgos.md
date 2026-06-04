---
title: "Implementación etapa 2 del audit DOWN: 24 hallazgos corregidos (5 buckets) + 399 tests host verdes"
date: 2026-06-03
author: "Claude (Anthropic - Claude Opus 4.7)"
requested-by: "María Viollaz (en la compu de Gustavo @gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7) — workflow + finalización manual"
status: final
tags: [down, firmware, audit, bugs, mejoras, tests, otos, linea, workflow]
robot: ambos
area: control
tipo: resultado
related-research: [2026-06-03-auditoria-multiagente-firmware-down.md]
---

# Implementación del audit DOWN — 24 hallazgos corregidos + testeados

## Contexto

Etapa 2 del audit multi-agente del firmware DOWN
(`research/in-progress/2026-06-03-auditoria-multiagente-firmware-down.md`, 28
hallazgos). Pedido de María: "avanzá con todo lo que esté a tu alcance sin mi
intervención, testeando TODO, en procesos paralelos con workflow."

## Cómo se hizo (el camino, honesto)

1. **Workflow paralelo con worktrees aislados** (1er intento): los 5 buckets
   devolvían su `diff` por el schema → **falló** (los agentes no emitían el
   `StructuredOutput`, casi seguro el `diff` gigante). Worktree principal quedó
   limpio (los aislados se auto-limpiaron).
2. **Workflow secuencial, commit-directo, schema chico** (2do intento): cada
   bucket implementa+testea+compila y **commitea** en el worktree principal.
   Corrió **docs-config, line-comm, model** (commiteados) y arrancó **otos**,
   pero **se cortó por "servicio ocupado"** con otos a medias (sin commitear) y
   `main-timing` sin arrancar.
3. **Finalización manual** (yo): revisé el bucket otos (estaba completo y de alta
   calidad, solo sin commitear), implementé `main-timing` (#24), corrí toda la
   verificación, commiteé los 2 que faltaban, y verifiqué la suite completa.

## Qué quedó hecho (5 buckets, 24 hallazgos, 5 commits)

| commit | bucket | hallazgos |
|---|---|---|
| `5870308` | docs-config | #8 #12 #17 #22 #27 (comentarios/docs al día) |
| `32f80d0` | line-comm | #2 (sample_age timestamp) #9 (cross_track muerto) #10 (g_dm init) #11 (encode==producción) #18 (doc bloqueo ec_save) |
| `131ec5b` | model | #4 (gate calib por `white[]`) #16 (corner geom real) #21 (penetration robusto) #25 (histéresis line_present) #26 #28 (timing sensor_health) |
| `f560dc4` | otos | #6 #15 (salud I²C real + confidence honesta) #7 (heading vectorial) #20 (slip sobre velocidades) #14 (clamp telemetría int16) #13 (recuperación en caliente) #19 (sacar calib del enlace TOP) #17 |
| `eb0f12b` | main-timing | #24 (mandar línea antes de otos_tick bloqueante) |

**Lo más jugoso (bugs reales que impactaban partido):**
- **#4** — `lc_adapt_carpet` gateaba con `validated[]`: un sensor que lee blanco
  contaminaba el baseline de carpet (arquero parkeado sobre la línea corrompía la
  calib). Ahora gatea con `white[]`.
- **#6/#15** — un OTOS que se desconecta mid-match dejaba la pose congelada con
  `confidence=100` (mentía). Ahora hay máquina de salud (histéresis + latch): la
  confidence cae a 60/0 y CENTRAL deja de creerle.
- **#7** — heading dual-OTOS por `atan2(Δy, sep)` saturaba en ±90°. Ahora promedio
  vectorial correcto en todo [-180,180].
- **#2** — `sample_age_ms` viajaba siempre en 255 (usaba la duración del tick, no
  un timestamp).
- **#14** — `omega` casteaba a int16 sin clamp → a +6 rad/s wrappeaba de signo
  ("gira a la derecha" → reportaba izquierda).

Patrón de diseño aplicado: lógica nueva extraída a **helpers PUROS** host-testeables
(`otos_health.h`, `otos_fusion.h`, `telemetry_sat.h`) en vez de meterla en el glue
Arduino — testeable + reusable 2027.

## Qué se midió (verificación)

- **Suite host completa: 32 suites / 399 tests / 0 fallos / 0 skips** — corrida con
  el runner nuevo `scripts/run-host-tests.sh` (g++ contra Unity vendoreado; `pio
  test` lo bloquea Avast/TASK-025, y el script que ESTADO referenciaba NO existía
  — se creó acá). Incluye **84 tests nuevos** (otos 36, down_tx 4, down_encode 8,
  down_model 20, sensor_health 16) y los tests de **contrato CENTRAL**
  (`test_central_line_ingest`, `test_central_otos_ingest`, `test_central_gk_cross_track`)
  → confirman que los cambios de DOWN **no rompen la ingesta del cerebro**.
- **Compilan las 3 placas**: down (FLASH 35944) + central_robot1 (28220) +
  top_robot1 (39688), SUCCESS.

## Lo que NO se hizo (a propósito) — requiere intervención humana / decisión

- **#1 (TASK-031)** y **#3 (TASK-301)** — validación end-to-end en **banco**
  (UART por protocolo + robustez). Claude no cierra tasks de HW.
- **#5** — unificar las 2 cadenas de línea (`line_ring` vs `DownModel`): **decisión
  de diseño** (cuál queda). Para charlar.
- **#23** — frescura de Velocity2D: es del lado **CENTRAL** (`central/comm_down.cpp`),
  fuera de la placa DOWN → para el agente CENTRAL.

## Próximos pasos

- Banco: validar en HW los cambios que tocan comportamiento (salud OTOS, calib,
  línea) — sobre todo TASK-031/301.
- Gustavo: mergear `agente/down` a `main`.
- Decidir #5 (2 cadenas) y derivar #23 a CENTRAL.
