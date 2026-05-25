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

## 📦 Para programar un subsistema: usar los packs

Hay 5 packs autocontenidos en `hardware/electronics/` (uno por subsistema
programable). Cada uno tiene TODO en un solo lugar: docs curados + snapshot
del firmware vivo + tests + ground-truth.

Punto de entrada: **[`hardware/electronics/PACKS-INDEX.md`](../hardware/electronics/PACKS-INDEX.md)**.

Lista rápida: `down-board-pack/`, `central-board-pack/`, `top-board-pack/`,
`cameraFront-pack/`, `cameraBack-pack/`. Cada uno tiene `README.md` con
"índice pregunta → doc".

> Los packs son snapshot del 2026-05-24. Si contradicen al código vivo del
> repo (`software/teensy/.../src/`), **gana el código vivo**.

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
- `src/top/main_top.cpp` + `cameras_runtime`, `cameras`, `sensors_imu`, `sensors_tof` (HC-SR04 + VL53L7CX frontal U2 vivos, lib `Adafruit_VL53L7CX` — ver journal 2026-05-24), `comm_*`

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

## Bloqueantes Incheon (los 2 que importan)

1. **COMM no flasheada** → robot no homologa (no recibe START/STOP árbitro). TASK-006.
2. **Cámaras sin recalibrar para iluminación Incheon** → no ve la pelota. TASK-022.

### Resuelto 2026-05-24
- ~~**DOWN — pinout Teensy↔mux NO confirmado**~~ → **VALIDADO EMPÍRICAMENTE.**
  Gustavo + Claude (ejecución directa) aplicaron el mapeo del doc canónico
  (`hardware/electronics/down-board-pack/01-pinout-y-posiciones.md`) al
  firmware (config_down.h + line_ring.cpp). Verdict del diag_capture: 0
  muertos, los 32 sensores responden. TASK-026 bajó de P0 a P2. Ver
  `journal/2026-05-24-hardware-up-down-anillo-linea.md`.

- ~~**DOWN OTOS — lib SparkFun en stub**~~ → **LIB ACTIVADA + OTOS RESPONDEN**.
  Misma sesión 2026-05-24. `src/down/otos.cpp` reescrito con API real
  (`getPosition`, no `getPose`; mismo tipo para position y velocity).
  Ambos chips U5 y U6 responden I²C en 0x17, pose se actualiza con
  movimiento. TASK-012 bajó de P0 a P1 (queda parte ToF en stub).
  Validación cuantitativa pendiente: TASK-029. **Regla nueva descubierta**:
  hardware-up requiere power cycle completo (TASK-028). Ver
  `journal/2026-05-24-otos-lib-activada-y-power-cycle-bug.md`.

- ~~**TOP VL53L7CX frontal en stub TODO_TOF_LIB**~~ → **VL53L7CX U2 FRONTAL VIVO**.
  Misma sesión 2026-05-24. Debug de 3 horas: 3 libs ST (L5/L7/L8) fallaron
  todas en init. Bug raiz identificado en `STM32duino_VL53L7CX/src/vl53l7cx_platform.h:49-60`
  (`DEFAULT_I2C_BUFFER_LEN = BUFFER_LENGTH` desborda en 2 bytes el buffer
  de `Wire` en Teensy 4.0 al cargar el firmware blob). Lib Adafruit_VL53L7CX
  funciona out of the box. `src/top/sensors_tof.cpp` migrado del stub a
  Adafruit (solo U2 instalado fisicamente; U3/U5/U17 quedan retornando
  `TOF_NO_READING`). Nuevo `[env:diag_sensors_tof_live]` permite probar el
  modulo migrado en aislamiento en banco. Libs ST marcadas DEPRECATED en
  sus READMEs. Ver `journal/2026-05-24-hardware-up-top-tof-frontal-resuelto.md`.

### 🏁 HITO 2026-05-24 — Subsistema DOWN/BOTTOM operacional en banco + OTOS validado cuantitativamente
La placa DOWN (también llamada "BOTTOM") pasó tests de banco con éxito:
anillo de 32 sensores leyendo + 2 OTOS reportando pose con precisión
cuantitativa validada (280.4 mm sobre 300 mm reales = 6.5% error, pasa
tolerancia 8% de TASK-029).

**Cerradas hoy:**
- ✅ **TASK-030**: lámina protectora sacada (en la misma sesión, sin
  esperar tapa).
- ✅ **TASK-029**: validación cuantitativa OTOS confirmada sobre cartón
  corrugado (300 mm reales → 280 mm reportados).

**Sigue pendiente:**
- **TASK-031**: verificar comunicación UART real DOWN→TOP (Serial5) y
  DOWN→CENTRAL (Serial1). Requiere las otras placas disponibles.

Ver journal `journal/2026-05-24-down-board-passing-tests-cierre.md`
(con sección "Test final post-lámina" agregada al final).

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
