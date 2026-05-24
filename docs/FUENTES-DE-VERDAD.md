---
title: "Fuentes de verdad del repo — por tema"
date: 2026-05-19
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: vivo
tipo: indice
---

# Fuentes de verdad — qué doc/módulo leer para cada tema

> **Por qué existe este archivo.** El repo tiene 49 docs creados por sesiones
> Claude descoordinadas (mayo 2026). Muchos temas tienen 2-3 docs rivales sin
> indicación clara de cuál es canónico. Esta tabla resuelve eso: una fila por
> tema, un único doc canónico vigente, los demás marcados como superados.
>
> **Regla de actualización**: cualquier sesión que crea o supera un doc tiene
> que actualizar esta tabla en el mismo commit. Si no, no es una sesión válida.

## Tabla canónica

| Tema | Doc/módulo canónico (vigente) | Superados / no usar como guía |
|---|---|---|
| **Arquitectura general 3 placas** | `docs/ARQUITECTURA-3-PLACAS-2026.md` (la sección "Mapa de flujo de datos" tiene WorldSnapshot v1 24 B — ver tema WorldSnapshot abajo) | — |
| **FSM táctica CENTRAL (la que CORRE)** | `src/central/strategy.cpp` (código vivo, llamado por `main_central.cpp:113`) + `src/shared/strategy_transitions.{h,cpp}` como caracterización pura con 35 tests | `_archive/src/shared/strategy_core.*` (FSM alternativa nunca integrada). `docs/superpowers/specs/2026-05-18-central-strategy-core-design.md` (diseño de la alternativa archivada — útil como referencia post-Incheon, NO refleja lo que corre hoy) |
| **FSM táctica CENTRAL (docs)** | `docs/firmware/FIRMWARE-PLACA-CENTRAL.md §8` (sincronizada con `strategy.cpp` real) | Diferentes sistemas de nombres en otros docs (SEEK/DRIVE vs SEARCH/POSITION) son la **misma máquina con etiquetas distintas**, no FSM rivales |
| **Behind-the-ball** | `src/shared/behind_ball.{h,cpp}` + 16 tests. Llamado por `strategy.cpp:230` (estado POSITION). Versión RELATIVA (sin pose absoluta). | El pseudocódigo de `FIRMWARE-PLACA-CENTRAL.md §8.4/§8.5` con `OPP_GOAL_X/Y` es Nivel 3 futuro (requiere EKF) — ya marcado en el doc |
| **Fusión cámaras (TOP)** | `src/shared/cameras_fusion.{h,cpp}` + `src/top/cameras_runtime.{h,cpp}` + 16 tests. Llamado por `main_top.cpp`. | — (sin doc rival) |
| **WorldSnapshot (struct compartido)** | `docs/firmware/CONTRATO-DATOS-CENTRAL.md` (**v2 = 27 B con ball_vx/vy**, `static_assert` activo en `types.h`) | `docs/ARQUITECTURA-3-PLACAS-2026.md` mapa de flujo dice "24 B" — **superado**, ver banner abajo |
| **DOWN — lectura de línea (la que CORRE)** | **Dos cadenas en paralelo (deuda viva, no archivar antes de Incheon):** (1) `src/down/line_ring.{h,cpp}` llamado por `main_down.cpp` para lectura cruda 1 kHz; (2) `src/down/down_model + line_geometry + line_tracker + line_calib + surface_monitor + down_encode` llamados por `comm_central.cpp` para armar `LineStatusV2` que va al CENTRAL | `docs/firmware/FIRMWARE-PLACA-ABAJO.md` describe solo (1) — desactualizado, banner agregado |
| **DOWN — pinout Teensy↔CD4051↔sensores** | **`hardware/electronics/2026-05-19-pinout-down-extraido-schematic.md`** + copia en el pack `down-board-pack/01-pinout-y-posiciones.md`. **VALIDADO EMPÍRICAMENTE 2026-05-24** en banco (Gustavo + Claude, sesión ejecución directa): pinout aplicado al firmware (config_down.h + line_ring.cpp), verdict del diag_capture = 0 muertos, los 32 sensores responden. La arquitectura "cada mux tiene 3 SEL propios" quedó confirmada. Cierre formal con multímetro (TASK-026) baja de P0 a P2. Ver journal `2026-05-24-hardware-up-down-anillo-linea.md`. | `src/down/config_down.h` ANTERIOR (`PIN_MUX_SEL_A=2,B=3,C=4,INH=5..8`, `PIN_MUX_OUT={A0,A1,A2,A3}`) — reemplazado. Sección DOWN de `mapa-pines-placas-nuevas.md` líneas 130-134 ("A/B/C compartidas") — SUPERADO. |
| **DOWN — pack autocontenido para programar (punto de entrada IA)** | **`hardware/electronics/down-board-pack/`** — todo lo necesario para entender, programar, testear y diagnosticar la placa DOWN en un solo lugar. 47 archivos, 2.4 MB. Contiene: 4 docs canónicos curados (`01-pinout` + `02-funcionalidad` + `03-contrato-datos` + `04-protocolo-comunicaciones`) + snapshot del firmware vivo (`firmware/down/` + módulos `firmware/shared/`) + 8 tests host-native + diag de hardware + scripts + ground-truth (SCH/PCB JSONs + PDF + BOM). El `README.md` tiene un índice "pregunta → doc". **Regla**: si el pack contradice al repo vivo, gana el repo vivo (es snapshot del 2026-05-24). | Otros docs que tocan DOWN tangencialmente (journals, plans superpowers, research in-progress, team-tasks) — quedan en sus carpetas originales como historia/gestión, no se duplican al pack. |
| **CENTRAL — pack autocontenido para programar (punto de entrada IA)** | **`hardware/electronics/central-board-pack/`** — equivalente al pack DOWN pero para la placa CENTRAL (Zircon Rev v15 + Teensy 4.1). 39 archivos. Contiene: 5 docs canónicos curados (`01-pinout-y-hardware` + `02-funcionalidad` 3 capas FSM/PIDs/motores + `03-contrato-datos` WorldSnapshot v2 + `04-protocolo-comunicaciones` + `05-arquitectura-3-placas`) + snapshot del firmware vivo de `src/central/` (14 archivos) + módulos `src/shared/` usados por CENTRAL (strategy_transitions, behind_ball, pids, kinematics, motion_target, proto, types) + 7 tests host-native (test_strategy_transitions 35 + test_behind_ball 16 + test_pids 17 + test_kinematics 11 + central_contract/trajectory/motion). Documenta los 2 robots (ROBOT1 arquero + ROBOT2 delantero) con sus pinouts distintos. **NO incluye ground-truth** (no hay schematic JSON del Zircon disponible, solo `Zircon.pdf` que queda fuera por binario pesado). **Hallazgo crítico documentado**: hay conflicto pines 7/8 entre doc histórico 2026-03-20 (motores) y firmware nuevo (Serial2 hacia DOWN) que hay que resolver antes de probar hardware. | Igual que DOWN: journals, tasks, plans no se duplican al pack. `Zircon.pdf` queda fuera. `software/libraries/zirconLib/` queda fuera (legacy 2025). |
| **TOP — pack autocontenido para programar (punto de entrada IA)** | **`hardware/electronics/top-board-pack/`** — equivalente al pack DOWN/CENTRAL pero para la placa TOP (Teensy 4.0 master + PCB custom "Roboliga2026 TOP"). ~36 archivos. Contiene: 6 docs canónicos curados (`01-pinout-y-hardware` + `02-funcionalidad` cerebro sensorial + `03-contrato-datos-top` WorldSnapshot v2 + `04-contrato-datos-camaras` protocolo OpenMV + `05-protocolo-comunicaciones` + `06-arquitectura-3-placas`) + snapshot del firmware vivo de `src/top/` (16 archivos: main + cameras + cameras_runtime + sensors_imu + sensors_tof + 3 comm_* + config) + módulos `src/shared/` usados por TOP (cameras_fusion + proto + types) + 2 tests host-native (test_cameras_fusion 16 + test_proto 13) + ground-truth (SCH/PCB JSONs + PDF + BOM parcial). El doc 02 separa claramente Nivel 1+2 vivo (cameras + IMU + HC-SR04 + UARTs) vs Nivel 3+ aspiracional (ToF VL53 stub, EKF, Kalman pelota, partner ESP-NOW). **Hallazgos críticos documentados**: (1) conflicto pin 7 (HC-SR04 ECHO vs Serial2 RX2 hacia CENTRAL); (2) Wire1 remap a pines 24/25 pendiente confirmar con multímetro (TASK-003). | Igual que DOWN/CENTRAL: journals, tasks, plans no se duplican al pack. `mapa-pines-placas-nuevas.md` y `2026-05-17-placa-top-analisis-gerbers.md` quedan en sus carpetas como historia (su info útil ya está integrada en el pack). |
| **DOWN — contrato de datos** | `docs/firmware/CONTRATO-DATOS-DOWN.md` (LineStatusV2) | — |
| **TOP — contrato de datos** | `docs/firmware/CONTRATO-DATOS-TOP.md` | — |
| **TOP — cámaras (protocolo)** | `docs/firmware/CONTRATO-DATOS-CAMARAS.md` | — |
| **Cámara FRONTAL (OpenMV) — pack autocontenido** | **`hardware/electronics/cameraFront-pack/`** — pack equivalente a los de placas pero para la cámara OpenMV frontal. 16 archivos, 152 KB. Contiene 4 docs curados (`01-hardware-y-conexion` + `02-funcionalidad` + `03-protocolo-comunicacion` + `04-calibracion-lab-y-homografia`) + snapshot del firmware OpenMV genérico actual + **template objetivo `target-cam-frontal-template.py` con bugs P0 corregidos** + parser del lado Teensy + tests de fusión. Para reemplazar el script genérico actual (`software/vision/enviar coordenadas 2 arcos y pelota`) por uno específico calibrado para esta cámara (TASK-022). | Pack hermano: `cameraBack-pack/`. El skill `openmv-vision-tuning` cubre el workflow general (no se duplica en el pack). |
| **Cámara TRASERA (OpenMV) — pack autocontenido** | **`hardware/electronics/cameraBack-pack/`** — pack equivalente al frontal pero para la cámara trasera. 16 archivos, 156 KB. Misma estructura, con énfasis especial en la **rotación 180°** que el TOP aplica a sus coordenadas (`cameras_fusion.cpp:25-29` con `cam_id=1`). Documenta por qué la cámara trasera NO debe rotar sus coordenadas internamente — la rotación es responsabilidad del TOP. Diferencia clave con la frontal: distinto Serial del Teensy (Serial5 vs Serial3), distinto conector (U9 vs U8), distinto cam_id (1 vs 0), H_MATRIX independiente, HMIRROR/VFLIP probablemente distintos por montaje espejado. | Pack hermano: `cameraFront-pack/`. |
| **Comunicaciones entre placas (diseño)** | `docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md` (declara fuente única, reemplaza al protocolo-comunicaciones-entre-placas.md del mismo día) | `docs/decisions/2026-05-18-protocolo-comunicaciones-entre-placas.md` (base conceptual superada). `docs/decisions/2026-05-18-verificacion-protocolo-comunicaciones.md` (insumo de la verificación, no decisión) |
| **Comunicación inter-robot (SuperTeam)** | `docs/decisions/2026-05-17-comunicacion-inter-robot-superteam.md` (decisión: NO implementar para Incheon) | — |
| **Firmware placa COMM (ESP32-C6) — flash** | `hardware/electronics/comm-board/2026-05-17-procedimiento-flash-firmware-c6.md` + `2026-05-17-placa-comm-componentes-y-circuito.md` (branch `esp32-c6`, pinout 6/7/18/9/20/19, BLE `RCJs-m_<MAC>`) | `journal/2026-05-15-firmware-comm-c6-flash-procedure.md` (branch `master`/C5, pinout 2/3/10/7/9/8 — INCORRECTO, ya tiene banner) |
| **Setup entorno PlatformIO + Avast** | `docs/firmware/SETUP-ENTORNO-BUILD-WINDOWS.md` (con callout Avast del 18-may) + `team-tasks/TASK-025` (excepción Avast por máquina) | — |
| **PIDs (heading, lateral, distancia)** | `src/shared/pids.{h,cpp}` + 17 tests. Llamado desde `strategy.cpp`. | — |
| **Kinemática inversa omni-3** | `src/shared/kinematics.{h,cpp}` + 11 tests. Llamado desde `motors_zircon.cpp`. | — |

## Deudas conocidas (no se resuelven antes de Incheon)

1. **DOWN lectura dual** (`line_ring` vivo + cadena nueva `down_model` viva en
   paralelo, posiblemente leyendo sensores 2 veces). Funciona ambos por
   separado. Decisión binaria pendiente post-Incheon: integrar todo en la
   cadena nueva o documentar formalmente la separación. → revisar en
   journal post-Nacional (noviembre 2026).
2. **Conectar `strategy.cpp` → `strategy_transitions`** (cerrar la red de
   testing de la FSM viva). Diseño en `journal/2026-05-15-analisis-firmware-y-fsm-testeable.md`
   "Conectar strategy.cpp" — pendiente ejecutar en post-Incheon (riesgo:
   tocar el cerebro).
3. **`cameras_runtime` no llena `ball_vx/vy`** (campos v2 nuevos). Quedan en 0;
   CENTRAL no puede usar intercepción por velocidad todavía. P2.

## Mapa de docs superados (con banner)

Estos docs tienen banner explícito que apunta acá:

- `journal/2026-05-15-firmware-comm-c6-flash-procedure.md` → ver procedure del 17-may
- `docs/firmware/FIRMWARE-PLACA-ABAJO.md` → ver `CONTRATO-DATOS-DOWN.md` + this file
- `docs/ARQUITECTURA-3-PLACAS-2026.md` Mapa de flujo → ver `CONTRATO-DATOS-CENTRAL.md` para tamaño real de WorldSnapshot
- `docs/superpowers/specs/2026-05-18-central-strategy-core-design.md` → módulo archivado en `_archive/`, ver este archivo

## Cómo mantener esta tabla viva

- Cualquier PR que cree un doc nuevo de tema técnico → agregar fila acá.
- Cualquier PR que supere un doc existente → marcar superado acá + banner en el doc viejo.
- Sesión Claude que toca el repo: primera lectura obligatoria + `docs/ESTADO-ACTUAL.md`.
