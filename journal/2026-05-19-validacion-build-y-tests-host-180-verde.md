---
title: "2026-05-19 — Validación entorno: 4 firmware compilan + 180/180 tests host-native PASSED"
date: 2026-05-19
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [build, testing, validacion, ci, host-native, hardware-up-ready]
robot: ambos
area: build
tipo: validacion
related-tasks: [TASK-025]
---

# Validación entorno: 4 firmware compilan + 180/180 tests host PASSED

## Contexto

Bajo el Plan C Nivel 2+3 del 2026-05-19 (moratoria de fábrica de papel, foco
en preparar entorno para hardware-up), se ejecutó la **validation work** que
el coach pidió: compilar las 4 envs de firmware + correr la suite completa de
tests host-native, sin modificar lógica del robot.

## Lo encontrado y arreglado

**Bug de build detectado y fixeado** (commit `e16e0f5`):
- 3 de 4 firmware (`top`, `central_robot1`, `central_robot2`) NO compilaban
  del repo limpio.
- Causa raíz: el código usaba `Adafruit_BNO055::OPERATION_MODE_IMUPLUS` (con
  qualifier de clase). La lib v1.6.4 define `OPERATION_MODE_IMUPLUS` como
  enum global de `typedef enum { … } adafruit_bno055_opmode_t`, **no** como
  member estático de la clase.
- Fix mínimo en 2 archivos:
  - `src/top/sensors_imu.cpp:35` — `bno.begin(OPERATION_MODE_IMUPLUS)`
  - `src/central/imu_zircon.cpp:49` — `g_bno.begin(OPERATION_MODE_IMUPLUS)`
- Sin este fix: Virginia/Elías iban a chocar exactamente este error el día
  del hardware-up cuando intentaran flashear — pérdida estimada medio día.
- Detectado por validation work, justifica completamente el Plan C.

## Estado del build (post-fix)

```
top             SUCCESS   00:00:09.681
down            SUCCESS   00:00:06.064
central_robot1  SUCCESS   00:00:07.869
central_robot2  SUCCESS   00:00:07.367
```

**Las 4 placas compilan limpio.** Para flashear, el equipo solo necesita:
```bat
cd "C:\Users\violl\iitasoccer\open-soccer-robocup-team2026\software\teensy\Soccer 2026"
pio run -e top -t upload
pio run -e down -t upload
pio run -e central_robot1 -t upload   # arquero
pio run -e central_robot2 -t upload   # delantero
```
(La placa COMM se flashea desde Arduino IDE con el clon local de
`_official_fw/`, ver TASK-006 actualizada.)

## Suite host-native — 180/180 PASSED

Tras destrabarse SSL Avast (Gustavo confirmó "Avast OK" — la excepción de
`*platformio.org*` quedó persistente esta vez), PlatformIO pudo bajar Unity
y compilar/correr cada suite. Resultado:

```
test_native    test_behind_ball           PASSED  16 tests / 11.6s
test_native    test_cameras_fusion        PASSED  16 tests / 3.3s
test_native    test_central_contract      PASSED        / 3.5s
test_native    test_central_motion        PASSED        / 3.4s
test_native    test_central_trajectory    PASSED        / 3.4s
test_native    test_down_calib            PASSED        / 3.1s
test_native    test_down_encode           PASSED        / 2.9s
test_native    test_down_geometry         PASSED        / 2.9s
test_native    test_down_model            PASSED        / 2.9s
test_native    test_down_surface          PASSED        / 2.9s
test_native    test_down_tracker          PASSED        / 2.9s
test_native    test_kinematics            PASSED  11 tests / 3.0s
test_native    test_line_filters          PASSED  22 tests / 3.0s
test_native    test_pids                  PASSED  17 tests / 3.1s
test_native    test_proto                 PASSED  13 tests / 5.2s
test_native    test_strategy_transitions  PASSED  35 tests / 4.2s

================ 180 test cases: 180 succeeded in 00:01:01.451 ================
```

**Primera ejecución real de toda la suite** en esta sesión. Hasta ahora estaba
"verificada por lectura cruzada" porque la excepción Avast no era persistente.

## Implicancias

1. **Toda la lógica pura host-testeable está VERDE** — kinemática, PIDs, filtros
   de línea, fusión cámaras, behind-the-ball, caracterización de la FSM,
   contratos, motion, trajectory, y la cadena DOWN nueva entera (calib,
   encode, geometry, model, surface, tracker). 180 casos.
2. La cadena DOWN nueva (`down_model + line_*`) **tiene sus tests verdes**;
   refuerza que NO se archiva. Sigue siendo deuda viva con `line_ring`, pero
   ambas funcionan por separado.
3. `strategy_transitions` (mi caracterización de la FSM viva, 35 tests) — todos
   PASSED. Mientras nadie cambie `strategy.cpp`, la caracterización sigue fiel.
4. **NO valida nada del comportamiento físico del robot** — la regla 1 del
   CLAUDE.md (testing en hardware real) sigue siendo responsabilidad del
   equipo humano. Estos 180 tests son la red de seguridad PURA, no la
   validación de cancha.

## Estado al cierre

- Firmware compila ✅ (4/4 placas Teensy).
- Tests host-native pasan ✅ (180/180).
- Repo en estado **"ready to flash"** para Virginia/Elías.
- Bloqueantes restantes son **TODOS hardware** (TASK-001/002/006/011/022).
- Moratoria sigue vigente hasta primer hardware-up confirmado.

## Próximo paso

Pelota en cancha del equipo humano. Cuando enchufen la primera placa y
flasheen, registrar en `journal/YYYY-MM-DD-primer-hardware-up.md` y se
levanta la moratoria (regla 8 CLAUDE.md).
