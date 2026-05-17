---
title: "2026-05-15 — Análisis integral del firmware + FSM testeable (caracterización)"
date: 2026-05-15
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [firmware, testing, fsm, strategy, analisis, deuda-tecnica, caracterizacion]
robot: ambos
area: control
tipo: analisis+decision
related-tasks: [TASK-011]
---

# Análisis integral del firmware + FSM testeable

## Contexto

Gustavo pidió: "análisis detallado del repo, optimizar programas, crear programas
de prueba/testeo, crear estructura funcional, documentar todo, analizar lo creado".
Pedido amplio (6 frentes). Como coach, en vez de tocar todo superficialmente: se
mapeó el estado real con 3 agentes en paralelo, se priorizó, y Gustavo eligió el
foco (FSM testeable, modo "solo tests sin refactorizar strategy.cpp").

## Análisis del estado del firmware (3 frentes)

### Inventario de código

Firmware **estructuralmente sólido**. 3 placas bien separadas (top/down/central)
+ `shared/` para lógica común. `platformio.ini` correcto (6 envs: top, down,
central_robot1/2, teensy41_legacy, test_native).

Stubs conocidos y bien marcados (no son bugs ocultos):
- `down/otos.cpp` — SparkFun OTOS lib comentada (TODO_OTOS_LIB). Odometría = 0
  hasta integrar. → Batch DOWN-B pendiente.
- `top/sensors_tof.cpp` — VL53L5CX/L7CX lib comentada. Solo HC-SR04 da
  proximidad real.
- Hardcodes documentados: `PIN_KICKER_SOL=23` (TASK-011), mapeo arco
  `yellow=opp` (`main_top.cpp:61`), `CAMERA_UNIT_TO_MM=10` (calibración).

### Estado de testing (ANTES de esta sesión)

~95 tests host-native sobre `shared/`: kinematics (11), pids (17),
line_filters (22), proto (13), cameras_fusion (16), behind_ball (16).
Cobertura buena en lógica pura.

**Gap crítico identificado**: `central/strategy.cpp` — la FSM, el cerebro
táctico del robot — tenía **CERO tests**. Su lógica de transición está atada a
`millis()` + world_model (Arduino), no testeable host-native. Igual
`world_model.cpp` (freshness) y el pulso kicker de `motors_zircon.cpp`.

### Estado de documentación

- Specs `docs/firmware/*.md` **desincronizadas** con el código: no reflejan
  CENTRAL Nivel 2 (KICKOFF/POSITION/GK_CLEAR/kicker) ni TOP cameras_fusion.
- Journal: faltaban entradas de los commits `1e46be1` (TOP cameras) y `b10c66d`
  (CENTRAL Nivel 2). Regla 4 del CLAUDE.md incumplida para esos.
- Onboarding 2027: falta diagrama visual de flujo de datos entre placas.

### Correcciones a los agentes (trust but verify)

Dos agentes Explore afirmaron cosas falsas que NO se propagaron al feedback:
- "behind_ball.h no existe / behind-the-ball no implementado" — **falso**.
  Existe desde `b10c66d`, con 16 tests. Confirmado por el agente de inventario.
- "PlatformIO no instalado" — **falso**. Está en
  `~/.platformio/penv/Scripts/pio.exe`. El bloqueo real es de RED para bajar
  la plataforma `native` (persistente toda la sesión).
- "EKF/Kalman faltan = gap caro" — **impreciso**. Son Nivel 3 explícitamente
  diferidos en la spec. Es roadmap planificado, no deuda oculta. P2.

## Lo hecho esta sesión

### Módulo `src/shared/strategy_transitions.{h,cpp}` (nuevo)

Réplica **fiel** del árbol de decisión de `strategy.cpp` (attacker_tick +
goalkeeper_tick, commit `b10c66d`) como funciones puras testeables:

- `atk_decide_transition(current, prev_match_running, world, tuning)` →
  `{next_phase, kicker_fire, start_kickoff_timer}`.
- `gk_decide_transition(current, world, tuning)` → `{next_phase}`.
- Enums espejo `AtkPhase`/`GkPhase` (los `AtkState`/`GkState` de strategy.cpp
  son file-local en el anonymous namespace, no accesibles).
- Tuning en struct con factories `atk_tuning_default()`/`gk_tuning_default()`
  que espejan los `constexpr` de strategy.cpp.

Fidelidad verificada por **lectura cruzada** (no se pudo correr la suite, sin
red para platform native). Puntos sutiles confirmados fieles:
- Orden de prioridad global: `!match_running` > `imminent_exit&&line_fresh` >
  `kickoff_edge`.
- Timer KICKOFF: en el tick del flanco usa `now` (no el timestamp viejo) →
  evita salto espurio a SEARCH. Test dedicado:
  `test_atk_kickoff_just_started_uses_now_not_stale_timestamp`.
- GK INTERCEPT: `dist<trigger`→CLEAR gana sobre `!visible`→PATROL.
- GK CLEAR: `!visible`→PATROL gana sobre `dist>release`→INTERCEPT.

### `test/test_strategy_transitions/test_main.cpp` (nuevo)

**35 tests** cubriendo: prioridades globales (6), timer KICKOFF (3),
LINE_AVOID (2), SEARCH→POSITION/APPROACH behind-the-ball (4), POSITION (4),
APPROACH + kicker_fire (6), GK completo (10).

Total firmware ahora: **~130 tests** host-native (7 suites).

## Límite honesto (NO ocultar esto)

**`strategy.cpp` NO fue modificado.** Sigue corriendo su lógica inline. El
módulo nuevo es una **caracterización**: los tests fijan la lógica de
transición que la FSM tiene HOY, pero hasta que `strategy.cpp` no llame a
`atk_decide_transition`/`gk_decide_transition`, los tests prueban una réplica,
no el binario que corre en el robot.

Decisión de Gustavo (explícita): "solo agregar tests, no refactorizar, cero
riesgo de romper". Se respetó: el cerebro que ya anda quedó intacto.

**Riesgo de esta decisión**: si alguien cambia el árbol en `strategy.cpp` y se
olvida de reflejarlo acá, la caracterización miente. Mitigación: regla de
sincronización documentada en el header de `strategy_transitions.h`.

## Temas a analizar (próximos pasos priorizados)

### Conectar strategy.cpp → strategy_transitions

**Categoría:** control · **Robot:** ambos · **Prioridad:** P1

**Qué.** Reemplazar el árbol inline de `attacker_tick`/`goalkeeper_tick` por
llamadas a las funciones puras ya testeadas. La FSM pasa a ser: "calcular
acción del estado (velocidades/PID) + llamar a decide_transition".

**Risk-no-fix.** Los 35 tests protegen una réplica, no el código real. Un bug
introducido directo en strategy.cpp no lo atrapa ningún test.
**Risk-fix.** Se toca el cerebro que hoy anda. Mitigable: los tests actuales
son la red — si la conexión cambia comportamiento, los tests de behind_ball +
caracterización lo exponen. ~2-3 h + test de regresión en hardware.
**Plan de prueba en hardware real.** Robot delantero en cancha: secuencia
WAIT_START→(árbitro start)→KICKOFF→SEARCH→ver pelota→POSITION→APPROACH→kick.
Criterio: misma secuencia de estados observada por el debug serial ANTES y
DESPUÉS de la conexión (grabar `strategy_get_state_name()` por 60 s en ambas
versiones, deben coincidir).

### Specs desincronizadas + diagrama de arquitectura

**Prioridad:** P1 (relevo 2027) · ~3 h. Actualizar `FIRMWARE-PLACA-CENTRAL.md`
con Nivel 2 real; diagrama de flujo de datos entre las 3 placas.

### OTOS + ToF stubs

**Prioridad:** P0 · necesita hardware. Batch DOWN-B (OTOS) ya en backlog.

## Pendientes

- Correr `pio test -e test_native` cuando haya red (verificar las 7 suites,
  ~130 tests, incluida la nueva).
- Decidir si/cuándo conectar strategy.cpp (tema-a-analizar arriba).
- Specs + diagrama (P1 relevo 2027).

## Commits de esta sesión

- `1e46be1` — TOP cameras_tick wired + 16 tests (sin journal hasta hoy).
- `b10c66d` — CENTRAL Nivel 2 (sin journal hasta hoy).
- `b4d0286` — COMM flash procedure + TASK-006/010/011.
- (este) — strategy_transitions + 35 tests + este journal.

Nota: los journals de `1e46be1` y `b10c66d` quedaron pendientes — este journal
los cubre retroactivamente en la sección "Lo hecho / análisis". Para el relevo
2027 conviene una entrada dedicada de la arquitectura Nivel 2 (P1).
