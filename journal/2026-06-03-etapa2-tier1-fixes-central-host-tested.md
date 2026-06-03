---
title: "Etapa 2 — Tier-1 de la auditoría CENTRAL: 7 fixes host-testeables implementados (TDD, suite verde)"
date: 2026-06-03
author: "Claude Opus 4.8 (Anthropic) — workflow TDD 2 lanes + integración central"
ai-assisted: true
status: completado
tags: [central, etapa2, tdd, pids, line_view, anti-windup, omega-overflow, data_valid, resync, workflow]
robot: ambos (CENTRAL Teensy 4.1 / Zircon Rev v15)
related:
  - research/in-progress/2026-06-03-auditoria-firmware-central.md
  - journal/2026-06-03-banco-resultados-arbitro-strafe-y-bno-freeze.md
---

# Etapa 2 — Tier-1 de la auditoría CENTRAL (host-testeable, sin intervención humana)

Pedido de Gustavo: ejecutar **en workflow paralelo** todos los ítems de la auditoría
que se puedan hacer **sin su intervención**, probarlos y documentarlos.

## Qué se hizo

Workflow TDD de 2 lanes sobre `src/shared` (lo único host-testeable: todos los
`src/central/*.cpp` incluyen Arduino.h y el harness solo linkea `src/shared`) +
integración central serial hecha por mí (compile-verified).

| # | Fix | Dónde | Test | Estado |
|---|---|---|---|---|
| **#9** | Overflow int16 de `omega_centideg_s` → sign-flip (gira al revés a casi máx vel con error de rumbo >109°) | helper puro `omega_degps_to_centideg()` en `pids.{h,cpp}` + cableado en los **5** sitios de `strategy.cpp` | test_pids +5 | ✅ host-tested + compila |
| **#29** | Anti-windup real (conditional integration) en heading + lateral PID | `pids.cpp` | test_pids +4 (incl. 2 regresiones no-saturadas bit-idénticas) | ✅ host-tested |
| **#5** | `lsv2_penetration_u8` no honraba `data_valid` (fallback GK strafea con dato inválido) | `line_view.h` | test_central_line_ingest | ✅ host-tested |
| **#13** | `lsv2_line_angle_deg` no honraba `data_valid` (LINE_AVOID retrocede a dirección basura) | `line_view.h` | test_central_line_ingest | ✅ host-tested |
| **#25** | `resync_events` del decoder no se exponía en CENTRAL | getters en `comm_top`/`comm_down` + columna `rsy=` en el debug print de `main_central` | n/a (getter trivial; el contador ya existía en `proto.h`) | ✅ compila |
| **#15** | `COMMAND_TIMEOUT_MS=200` muerto + comentario equivocado | `config_central.h` | n/a | ✅ removido |
| **#17** | Constantes de pin UART parecían reasignables (no lo son) | `config_central.h` | n/a | ✅ anotadas informativas |

**Patrón TDD respetado:** cada lane escribió el test que **falla** (RED confirmado por
la razón esperada: #9 compile-error por helper inexistente; #29 integral llegaba al
clamp; #5/#13 devolvían 50/45° con `data_valid=0`), luego el fix mínimo, luego GREEN.

## Verificación

- **Suite host completa:** `bash scripts/run-host-tests.sh` → **25 envs / 322 tests / 0 fallos / 0 build-errors**. (`test_pids` 18→27, `test_central_line_ingest` 8→11.)
- **Compile firmware:** `central_robot1` SUCCESS + `central_robot2` SUCCESS.

## Lo que NO se hizo (y por qué)

- **#1 (`schema_version` en WorldSnapshot):** aunque el esfuerzo es bajo, **cambia el
  formato de wire** y rompería el link TOP→CENTRAL hasta que la placa TOP emita el
  nuevo layout. Es **cross-board** → necesita coordinación con el agente TOP, no es
  "sin intervención". Queda para coordinar.
- **#21 (freshness propia de la vel OTOS), #23 (conteo de frames_lost):** viven en
  `src/central` (Arduino/Serial) → **no host-testeables** con el harness actual, y #21
  además cambia conducta del drive-straight. Se difieren a una ronda con banco.
- **Toda la lista Tier-2** (#8 signo de omega, #10 deadzone, #6 fuente de heading, #7
  watchdog DOWN, #11 brake/coast, #2 derivative kick, #4 debounce): requieren tu banco.

## PENDIENTE DE BANCO (regla 1 — Claude NO cierra TASKs de hardware)

Estos fixes **compilan y pasan host**, pero cambian conducta del robot → hay que
validarlos en banco antes de darlos por buenos en cancha → **TASK-102**:
1. **#9:** con un setpoint de heading que sature el PID (error >120°), confirmar que el
   robot gira hacia el lado **corto** y NO se invierte a alta velocidad.
2. **#29:** tras una excursión grande, confirmar que vuelve al setpoint **sin sobrepasar**.
3. **#5/#13:** forzar `data_valid=0` (calib suspect / todo-blanco) y confirmar que el
   arquero NO strafea y que LINE_AVOID no retrocede a dirección basura.
4. **#9 keystone:** sigue dependiendo de validar el **signo de omega** (#8) en banco —
   el clamp evita el overflow, pero el sentido de giro se confirma con el test de #8.

## Commits

- `f545810` — los 7 fixes + tests (12 archivos).
- (este journal + estado de auditoría + TASK-102 en el commit de docs).

## Atribución

Workflow TDD + integración + docs: Claude Opus 4.8 (Anthropic), 2026-06-03.
Pedido + scope: Gustavo Viollaz (@gviollaz). La validación en banco la hace el equipo.
