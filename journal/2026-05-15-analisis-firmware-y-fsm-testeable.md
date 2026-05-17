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

### Conectar strategy.cpp → strategy_transitions  (DISEÑO LISTO)

**Categoría:** control · **Robot:** ambos · **Prioridad:** P1

**Por qué no se hizo ya.** Cambiar el cerebro es cambio de código del robot →
regla 1 no-negociable del CLAUDE.md exige test en hardware real. En esta
sesión no hubo red para correr la suite (`platform native` no baja) ni
hardware. Conectar a ciegas viola el frame. Queda el diseño masticado para
ejecutar apenas haya red+hardware.

**Trampa de diseño detectada.** La FSM actual es **Moore**: ejecuta la acción
del estado en el que está (post-transiciones globales) y la transición
per-estado decide el PRÓXIMO tick. Si se conecta ingenuamente "decido todo y
actúo sobre el estado final", se vuelve **Mealy** → la acción del estado nuevo
arranca 1 tick (10 ms a 100 Hz) antes. Parece nimio pero cambia la secuencia
observable y rompería el criterio de regresión. **La conexión DEBE preservar
el orden Moore.**

**Diseño concreto (preserva comportamiento exacto):**

1. En `strategy_transitions.{h,cpp}`: separar `atk_decide_transition` en dos
   funciones componibles (refactor del módulo NUEVO — sin riesgo, no lo usa
   nadie aún): `atk_apply_global(phase, prev_match, w)` y
   `atk_apply_perstate(phase, w, t)`. Mantener `atk_decide_transition` =
   `apply_perstate(apply_global(...))` para que los 35 tests sigan válidos
   sin cambios. Idem GK.
2. En `strategy.cpp::attacker_tick()`, reestructurar a 4 pasos explícitos:
   - (a) construir `AtkWorldView w` desde `world_model_*()` + `millis()`.
   - (b) `g_atk_state = atk_apply_global(g_atk_state, g_match_was_running, w)`
     + setear `g_kickoff_started_ms` si entró a KICKOFF (idéntico a hoy).
   - (c) ejecutar la ACCIÓN del estado actual = el `switch` actual PERO
     borrando solo las llamadas `transition_atk(...)` de adentro (conservando
     los early-return cmd-vacío de POSITION/APPROACH sin pelota).
   - (d) `g_atk_state = atk_apply_perstate(g_atk_state, w, tuning)` con reset
     de PID si cambió; aplicar `d.kicker_fire` a `cmd`.
3. Idem `goalkeeper_tick()` (sin paso KICKOFF).
4. `transition_atk/gk` (reset PID) se invoca solo cuando (b) o (d) cambian de
   fase — misma semántica que hoy.

**Risk-no-fix.** Los 35 tests protegen una réplica, no el binario. Un bug
metido directo en strategy.cpp no lo atrapa nadie hasta la cancha.
**Risk-fix.** Se toca el cerebro. Mitigado por: (1) diseño que preserva Moore,
(2) los ~130 tests como red, (3) test de regresión serial obligatorio abajo.
~3 h código + 1 h test hardware.

**Plan de prueba en hardware real (obligatorio antes de mergear).**
1. Setup: robot delantero (ROBOT2), cancha, árbitro/COMM simulado o botón
   start manual, pelota naranja, arco visible.
2. Instrumentar: loguear `strategy_get_state_name()` + `cmd.kicker_fire` por
   Serial a 50 Hz.
3. Grabar 60 s de secuencia ANTES de la conexión (binario actual):
   WAIT_START→start→KICKOFF→SEARCH→ver pelota→POSITION/APPROACH→kick→...
4. Aplicar la conexión, recompilar, grabar 60 s con el MISMO guion físico.
5. **Criterio de aceptación**: la secuencia de estados y los instantes de
   `kicker_fire` coinciden entre ambas grabaciones (tolerancia ±1 tick por
   jitter de sensores). Cualquier divergencia = la conexión cambió
   comportamiento → no mergear, depurar.
6. Regresión vecinos: confirmar que EMERGENCY_LINE (brake <15 ms en
   `main_central.cpp`) sigue disparando — pisar línea a mano, ver brake.

### Specs desincronizadas + diagrama de arquitectura  ✅ HECHO 2026-05-15

`FIRMWARE-PLACA-CENTRAL.md` §8 sincronizada con la FSM Nivel 2 real (KICKOFF,
POSITION por ángulo, kicker en APPROACH, GK_CLEAR con histéresis, LINE_AVOID
como estado; aclarado qué es Nivel 1/2/3). `ARQUITECTURA-3-PLACAS-2026.md`:
agregado "Mapa de flujo de datos" (tabla de enlaces UART + diagrama + gaps sin
confirmar enlazando TASK-001/008/009) para onboarding del relevo 2027.

### OTOS + ToF stubs  → TASK-012 creada

**Prioridad:** P0 · necesita hardware + decisión modelo ToF (Q4). Bug latente
de velocity en `otos.cpp` corregido en esta sesión (las velocidades se
asignan ahora en la fusión + bloque comentado; sigue en 0 hasta descomentar
→ binario actual idéntico). Procedimiento exacto de activación (lib_deps,
qué descomentar, plan de prueba hardware) en `TASK-012`.

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
