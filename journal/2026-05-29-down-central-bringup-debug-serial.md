---
title: "DOWN↔CENTRAL bring-up: hallazgos de arquitectura + env down_debug para validar la transmisión sin placa CENTRAL"
date: 2026-05-29
author: "Claude (Anthropic - Claude Opus 4.7)"
requested-by: "María Viollaz (en la compu de Gustavo @gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7, Anthropic)"
status: final
tags: [down, central, bringup, uart, otos, linea, pines-7-8, task-036, debug, firmware]
robot: ambos
area: comunicacion
tipo: resultado
---

# DOWN↔CENTRAL bring-up — hallazgos + tooling para validar DOWN solo

## Contexto

María, en el banco (compu de Gustavo), quiere mover el robot derecho usando los
dos OTOS. Plan inicial de ella: bajar 2 programas (DOWN + CENTRAL), sin la placa
TOP (que todavía no está lista). Antes de pasarle los pasos, se verificó en el
código qué soporta realmente el firmware.

## Qué se encontró (verificado en código, no teoría)

1. **DOWN→CENTRAL (Serial1→Serial2) lleva SOLO la línea** (`LineStatusV2`), NO los
   OTOS. Los OTOS van DOWN→TOP por Serial5 y el TOP los fusiona. Evidencia:
   `main_down.cpp:101` (`comm_top_send_status`) vs `:107` (`comm_central_send_line_urgent`).

2. **El "ir derecho" del sketch de manejo usa heading IMU/TOF, no OTOS.**
   `main_top.cpp::build_snapshot` (líneas 45-55) toma `localization_runtime_get_pose()`
   = trilateración TOF + BNO055. El pose OTOS de DOWN llega al TOP
   (`comm_down_get_pose()`) pero NO entra al snapshot que va a CENTRAL. O sea,
   "manejar con OTOS" **no está cableado** — es el `TODO_DIFFERENTIAL_OTOS`
   (post-Incheon) del propio sketch.

3. **El conflicto de pines 7/8 BLOQUEA DOWN→CENTRAL.** El CENTRAL recibe a DOWN
   por **Serial2 = pines 7/8** (`comm_down.h:3`). Esos mismos pines son de un motor
   (`config_central.h`: motor 2 en ROBOT1 / motor 1 en ROBOT2). `motors_init()`
   (`motors_zircon.cpp:101-105`) los configura como OUTPUT → **pisa el UART**. Como
   TODO firmware del CENTRAL llama `motors_init()`, **ningún firmware existente
   puede recibir a DOWN**. Esto es la **TASK-036** (el doc de la placa CENTRAL ya lo
   lista como bloqueante de "Comunicación DOWN→CENTRAL real").

## Qué se hizo

- Decisión con María: como la mitad CENTRAL está bloqueada por hardware (TASK-036),
  validar **la mitad DOWN** del enlace — confirmar que DOWN lee y **transmite** la
  línea por el UART real — desde la placa DOWN sola.
- **Print de debug gateado** agregado en `comm_central_send_line_urgent()`
  (`src/down/comm_central.cpp`): bajo `#ifdef DOWN_DEBUG_SERIAL`, imprime por el
  Serial USB a ~4 Hz lo MISMO que sale por Serial1 hacia CENTRAL: `data_valid`,
  `line_present`, `sens_on_line`, `angle_deg`, `penet_mm`, `imm_exit`, `flags`,
  más los contadores `tx_ok` (=frames_sent) y `tx_drop` (=frames_dropped, P1.6).
- **Nuevo `[env:down_debug]`** en `platformio.ini`: `extends = env:down` +
  `-DDOWN_DEBUG_SERIAL -DDOWN_NUM_MUXES_CONNECTED=4 -DDOWN_NUM_OTOS_CONNECTED=2`
  (placa fixeada). Un comando: `pio run -e down_debug -t upload`.

## Qué se midió/observó

- `[env:down]` (competencia): **FLASH code:33416 idéntico** al build previo → el
  print está correctamente gateado OFF; el firmware de torneo NO se contamina.
- `[env:down_debug]`: **SUCCESS**, FLASH code:34600 (+1184 B del print), compila
  100% offline (libs vendoreadas, TASK-302).
- **NO validado en hardware todavía** — María lo va a correr ahora. Claude no
  cierra tasks de hardware (regla 1 CLAUDE.md).

## Conclusión

Queda listo el tooling para que María valide, **sin la placa CENTRAL**, que DOWN
emite la línea por UART real (`tx_ok` subiendo ~200/s) y que el measurement
responde al mover el robot sobre el blanco (`line_present`, `angle_deg`). Esa es
la mitad "de abajo" del hito de la moratoria (regla 8: "DOWN reportando línea por
UART real").

La mitad CENTRAL (que el CENTRAL reciba) y el manejo por OTOS quedan **bloqueados
por hardware (TASK-036, pines 7/8)** — no es algo que se arregle en firmware del
DOWN.

## Próximos pasos

- **María (ahora):** correr `down_debug`, confirmar `tx_ok` sube + valores
  responden al blanco. Dejar el resultado en un journal/nota.
- **TASK-036 (Enzo):** resolver pines 7/8 (mover Serial2 a pines libres o
  confirmar el ruteo) → recién ahí el CENTRAL puede recibir a DOWN y manejar.
- **Post-Incheon (agente TOP):** cablear el heading de los OTOS al `build_snapshot`
  del TOP (`TODO_DIFFERENTIAL_OTOS`) si se quiere manejo asistido por OTOS.
