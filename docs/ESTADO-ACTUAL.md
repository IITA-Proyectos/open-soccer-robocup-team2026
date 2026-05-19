---
title: "Estado actual del robot — vivo, 1 página"
date: 2026-05-19
last-updated-by: "Claude (sesión 2026-05-19, cleanup quirúrgico)"
status: vivo
tipo: indice-operacional
---

# Estado actual — Snapshot del repo (al 2026-05-19)

> **Toda sesión Claude que toca este repo: ESTA es la primera lectura
> obligatoria** (después de `git pull`). Si lo que estás por hacer contradice
> algo de acá, **parar y consultar al humano**. Si lo que vas a hacer hace
> cambiar algo de acá, **actualizá esta página en el mismo commit.**

## Calendario crítico

- **Incheon 2026** — 30-jun a 6-jul (≈42 días).
- **Estrategia** (CLAUDE.md): inversión en aprendizaje, no en podio. Robot
  honesto, partidos jugados, captura sistemática.
- **Estado realista hoy**: 50/50 que el robot compita "de verdad" (ver
  `journal/2026-05-19-analisis-coach-fabrica.md` cuando se cree).

## Módulos VIVOS (corren en binario hoy)

### CENTRAL (Teensy 4.1, Zircon Rev v15)
- `src/central/main_central.cpp` — entry
- `src/central/strategy.cpp` — FSM ATK + GK Nivel 2 (KICKOFF/SEARCH/POSITION/APPROACH + PATROL/INTERCEPT/CLEAR + LINE_AVOID). **El cerebro.**
- `src/central/motors_zircon.{h,cpp}` — PWM 3 motores omni + kicker (ROBOT2)
- `src/central/imu_zircon.{h,cpp}` — BNO055 respaldo
- `src/central/world_model.{h,cpp}` — espejo del WorldSnapshot
- `src/central/comm_top.{h,cpp}` — recibe WorldSnapshot del TOP (Serial1)
- `src/central/comm_down.{h,cpp}` — recibe LineStatusV2 del DOWN (Serial2)

### TOP (Teensy 4.0)
- `src/top/main_top.cpp` + `cameras_runtime`, `cameras`, `sensors_imu`, `sensors_tof` (HC-SR04 funciona, VL53 stub), `comm_*`

### DOWN (Teensy 4.0) — **deuda: 2 cadenas paralelas**
- `src/down/main_down.cpp` → llama `line_ring.{h,cpp}` (cadena vieja, lectura cruda 1 kHz)
- `src/down/comm_central.cpp` → llama cadena nueva: `down_model + line_geometry + line_tracker + line_calib + surface_monitor + down_encode` para armar `LineStatusV2` que va al CENTRAL
- **NO archivar ni una ni otra antes de Incheon.** Decisión binaria post-Incheon (ver `FUENTES-DE-VERDAD.md` deudas).

### Shared (puro, testeado host-native)
- `pids`, `kinematics`, `behind_ball`, `cameras_fusion`, `line_filters`, `crc16`, `proto`, `types`
- `strategy_transitions` (caracterización pura de `strategy.cpp` con 35 tests — no conectado, mantener como red)

## Tests host-native

| Suite | Tests | Cubre |
|---|---|---|
| `test_kinematics` | 11 | omni-3 |
| `test_pids` | 17 | heading + lateral + distancia |
| `test_proto` | 13 | CRC, frame, marker |
| `test_line_filters` | 22 | temporal + hysteresis + spatial + centroide + lifted |
| `test_cameras_fusion` | 16 | rot 180°, fuse front+back, watchdog |
| `test_behind_ball` | 16 | target detrás, aligned-to-shoot, attack-line, kickoff |
| `test_strategy_transitions` | 35 | árbol decisión ATK + GK (caracterización) |
| `test_central_contract` | ? | contrato CENTRAL |
| `test_central_trajectory` | ? | ball_trajectory |
| `test_down_*` (calib, encode, geometry, model, surface, tracker) | ? | cadena DOWN nueva |
| **Total estimado** | **≥130** | — |

**Estado:** todavía NO se corrieron de punta a punta en esta máquina (TASK-025 Avast destraba). Verificación hoy = lectura cruzada del código.

## TASKs activas (al 2026-05-19) — ver `team-tasks/README.md`

**P0 hardware (asignar HOY a humanos, no a Claude):**
- TASK-001 (Enzo): fix 10 nets DOWN PCB
- TASK-002 (Enzo): DRC+ERC ambas placas
- TASK-006 (Virginia/Elías): **flash firmware COMM ESP32-C6** (procedure del 17-may, NO el del 15-may que tiene banner)
- TASK-011 (Enzo): confirmar PIN_KICKER_SOL en Zircon
- TASK-013 (Enzo): recuperar BOM placa TOP
- TASK-025 (todos): excepción Avast en cada máquina → destraba PlatformIO

**P0 firmware (alguien tiene que hacerlo, pero solo después de tener placa que compile/flashee):**
- TASK-014 (Virginia/Elías): loop TOP no-bloqueante medido con osciloscopio
- TASK-015 (Virginia): CRC + fin de trama enlace cámara
- TASK-016 (Virginia/Elías): fail-safe borde (OR-latch + precedencia)
- TASK-022 (Virginia): cámara operativa (sentinel, exposición fija, recalib Incheon)
- TASK-023 (Virginia/Enzo): build/tooling CI
- TASK-024 (Virginia/Elías): arranque rol/polaridad arco

## Bloqueantes Incheon (los 3 que importan)

1. **COMM no flasheada** → robot no homologa (no recibe START/STOP árbitro). TASK-006.
2. **DOWN PCB con nets faltantes** → sensor de línea no funciona → robot sale de cancha sin advertir. TASK-001.
3. **Cámaras sin recalibrar para iluminación Incheon** → no ve la pelota. TASK-022.

## Regla operativa (CLAUDE.md actualizado 2026-05-19)

- Claude **planifica, documenta y programa firmware host-testeable**.
- Claude **NO cierra TASKs de hardware** — eso solo lo puede hacer el equipo humano que tiene la placa en la mano.
- Moratoria temporal de nuevos docs/specs/plans hasta primera hardware-up (robot encendido + COMM flasheada + DOWN reportando línea por UART real).

## Cómo actualizar esta página

Modificarla en cada commit que cambie:
- Qué módulo es VIVO (entra/sale del binario)
- Qué TASK es bloqueante (cambia prioridad o se cierra)
- Qué deuda apareció o se resolvió

Sin actualización = sesión inválida.
