---
title: "Etapa 2 — 2da tanda autónoma: OTOS vel freshness + honestidad de contrato (CENTRAL)"
date: 2026-06-03
author: "Claude Opus 4.8 (Anthropic)"
ai-assisted: true
status: completado
tags: [central, etapa2, otos, freshness, contrato, referee, motors-brake, telemetria, auditoria]
robot: ambos (CENTRAL Teensy 4.1 / Zircon Rev v15)
related:
  - research/in-progress/2026-06-03-auditoria-firmware-central.md
  - journal/2026-06-03-etapa2-tier1-fixes-central-host-tested.md
  - team-tasks/2026-06-03-task-102-validar-banco-tier1-fixes-central.md
---

# Etapa 2 — 2da tanda (robustez + honestidad de contrato)

Continuación de la Etapa 2: los ítems autónomos que quedaban (sin banco, sin romper el
link con TOP). Estos NO son host-testeables con el harness actual (viven en `src/central`,
que usa Arduino/`millis()` y no se linkea en `scripts/run-host-tests.sh`) → **compile-verified**.

| # | Qué | Dónde | Tipo |
|---|---|---|---|
| **#21** | OTOS vel con **freshness propia** (timestamp aparte de la pose) + gateo en strategy: si la vel no está fresca, NO cancelar deriva con dato viejo (degradación conservadora) | `world_model.{h,cpp}` + `strategy.cpp` (KICKOFF + APPROACH) | fix robustez |
| **#16** | Nota en `world_model.h`: accessors **expuestos pero NO consumidos** por la FSM (min_obstacle, partner_*, get_otos_x/y, etc.) → "que exista accessor ≠ el robot lo usa" | `world_model.h` | doc anti-trampa |
| **#19** | `referee_cmd` marcado **RESERVADO/dead** (0 callers): el contrato real del árbitro es **binario por GPIO** (match_running), halftime/reset no se transmiten | `world_model.h` | doc/cleanup |
| **#41** | Comentario de `motors_brake` **honesto**: es freno activo SOLO si el chip hace short-brake; en VNH-style PWM=0 = coast. Pendiente identificar el driver (#11) | `motors_zircon.h` | doc honestidad |
| **#23** | Semántica de `frames_lost`: **se solapa con `crc_errors`** (un frame CRC-malo aparece como hueco) y mezcla los 3 tipos → `huecos_reales ≈ frames_lost - crc_errors` | `comm_down.cpp` | doc telemetría |

## Verificación

- `central_robot1` SUCCESS + `central_robot2` SUCCESS.
- Suite host: sin cambios esperados (no toqué lógica de `src/shared`; el único toque a
  shared fue un comentario en `types.h`… en realidad la nota de #19 fue a `world_model.h`,
  shared intacto) → **322 tests / 0 fallos** (confirmado tras la corrida).

## Pendiente de banco (regla 1)

- **#21**: comportamiento conservador (si se pierde la vel del OTOS, el drive-straight no
  cancela deriva con el dato viejo). Bajo riesgo. Validar junto con TASK-102: confirmar que
  el kickoff/approach con OTOS sigue andando y que al cortar selectivamente la vel el robot
  no corrige con basura. (El escenario es de baja probabilidad; no es bloqueante.)
- El resto de esta tanda (#16/#19/#41/#23) son **doc/telemetría, no cambian conducta** → no
  requieren banco.

## Lo que sigue pendiente (no autónomo)

- **#1** (schema_version): cross-board, coordinar con agente TOP.
- **Tier-2** (#8 signo omega, #10 deadzone, #6 fuente heading, #7 watchdog DOWN, #11
  brake/coast, #2 derivative kick, #4 debounce, #35 reset PATROL, #26/#27 control): requieren
  tu banco/decisión.
- **#33** (unificar FSM con strategy_transitions): refactor grande del cerebro, post-Incheon.
- **Harness host para `world_model`** (stub de millis): habilitaría host-test de #21 y futuros.

## Atribución

Fixes + docs: Claude Opus 4.8 (Anthropic), 2026-06-03 (requested-by Gustavo Viollaz).
Validación en banco de #21: equipo humano (regla 1).
