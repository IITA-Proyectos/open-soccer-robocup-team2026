---
title: "Firmware COMM RCJ — el branch correcto es esp32-c6 (no master)"
date: 2026-05-17
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [firmware, comm-board, esp32c6, rcj-official, completed]
robot: comm
area: comunicacion
tipo: resultado
related-tasks: [TASK-006, TASK-010]
---

# Firmware COMM RCJ — branch correcto = `esp32-c6`

## Resumen ejecutivo (30 s)

Pregunta: ¿qué firmware oficial cargar a la placa COMM (ESP32-C6-MINI-1-N4)?
Respuesta: el repo oficial `robocup-junior/soccer-communication-module` **no
tiene tags/releases**; el `master` compila para **ESP32-C5**. El firmware
correcto para nuestra placa C6 está en el branch dedicado **`esp32-c6`**,
commit `ffb4e3c1a1ddac2b3d3ed7bd8a24aacc19ea0081` (v0.91). La documentación
previa (2026-05-15) estaba equivocada por mirar solo `master`.

## Conclusión

- Branch: **`esp32-c6`** · commit `ffb4e3c` · firmware v0.91.
- Core Arduino-ESP32 **3.2.2** exacto · board `ESP32C6 Dev Module`.
- Pin map C6: SDA=6, SCL=7, BTN(CONNECT)=18, BTN2(PROG)=9, OUT1=20, OUT2=19.
- BLE: `RCJs-m_<MAC>`. Start/stop = nivel en OUT1/OUT2; árbitro por BLE.
- Flash oficial: cablear USB D+/D−/VIN/GND a los pines del header (sin USB-C).
- v0.91 no usa acelerómetro ni hace puente ESP-NOW (UART del header = pasivo).

## Recomendaciones accionables

1. Ejecutar TASK-006 con el procedimiento correcto:
   `hardware/electronics/comm-board/2026-05-17-procedimiento-flash-firmware-c6.md`.
2. Verificar OUT1/OUT2 físicamente con multímetro antes de cancha (issue
   oficial #5: rotulación históricamente inconsistente).

## Decisión tomada

Plan A (cargar firmware oficial sin modificar) — **se mantiene** desde
TASK-010, pero con el branch y pin map corregidos. No se requiere portar.

## Links a journal / docs

- Análisis: `journal/2026-05-17-analisis-3-placas-y-correccion-firmware-c6.md`
- Procedimiento: `hardware/electronics/comm-board/2026-05-17-procedimiento-flash-firmware-c6.md`
- Circuito/pinout: `hardware/electronics/comm-board/2026-05-17-placa-comm-componentes-y-circuito.md`
- Entrada superada: `journal/2026-05-15-firmware-comm-c6-flash-procedure.md`
- Repo oficial: https://github.com/robocup-junior/soccer-communication-module (branch `esp32-c6`)
