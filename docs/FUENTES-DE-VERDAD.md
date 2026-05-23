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
| **DOWN — pinout Teensy↔CD4051↔sensores** | **`hardware/electronics/2026-05-19-pinout-down-extraido-schematic.md`** (BORRADOR a validar con Enzo). Extraído automáticamente del schematic JSON, todos los pines con marca de confianza. Reescribe la arquitectura (cada mux tiene 3 SEL propios, NO compartidos). | `src/down/config_down.h` valores actuales (`PIN_MUX_SEL_A=2,B=3,C=4,INH=5..8`) son tentativos/incorrectos — pendiente actualizar tras validación. Sección DOWN de `hardware/electronics/mapa-pines-placas-nuevas.md` líneas 130-134 dice "A/B/C compartidas" — SUPERADO por el doc nuevo |
| **DOWN — contrato de datos** | `docs/firmware/CONTRATO-DATOS-DOWN.md` (LineStatusV2) | — |
| **TOP — contrato de datos** | `docs/firmware/CONTRATO-DATOS-TOP.md` | — |
| **TOP — cámaras (protocolo)** | `docs/firmware/CONTRATO-DATOS-CAMARAS.md` | — |
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
