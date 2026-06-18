---
id: TASK-220
title: "Exponer en el monitor en vivo los frames perdidos del enlace TOP->CENTRAL (surfacing del contador)"
date_created: 2026-06-18
assigned: [gustavo-viollaz]
priority: P1
status: pending
estimated_hours: 3
blocks: ["ver la salud del enlace TOP en el pit durante un partido (aprendizaje Incheon)"]
blocked_by: ["coordinar con el worker de la placa base (toca archivos de CENTRAL + contrato de telemetria + test golden)"]
tags: [central-board, telemetria, monitor, enlace, diagnostico, coordinar-placa-base]
related:
  - src/central/comm_top.cpp
  - src/central/central_telemetry_serial.cpp
  - tools/monitor-base/monitor_base/protocol_central.py
---

# TASK-220 — Exponer los frames perdidos del enlace TOP→CENTRAL en el monitor

## Por qué existe

Hoy, si la CENTRAL pierde snapshots del TOP (buffer RX lleno, ruido), se ve **igual que
"sin enlace"** — no hay número de cuántos se perdieron. El TOP numera cada snapshot
(`top/comm_central.cpp`: `f.seq = g_send_seq++`), así que la CENTRAL puede contar los
huecos del SEQ. Eso convierte "pérdida silenciosa" en "pérdida medida" = aprendizaje de
Incheon (el objetivo declarado del mundial).

**Ya hecho (este commit, infraestructura):** el contador + getter `comm_top_get_frames_lost()`
está en `src/central/comm_top.cpp`, **desactivado por defecto** (`-DCENTRAL_TOP_LINK_SEQ`),
binario de competencia **sin cambio**. Verificado que compila con la bandera activada.

**Ya cubierto en banco:** el diag `diag_central_rx_all` rastrea los huecos de SEQ del TOP
por su cuenta (`seq_gaps`) → para diagnóstico de banco YA se puede ver, sin nada nuevo.

## Lo que falta (esta tarea) — y por qué se dejó para después

Verlo EN VIVO en el monitor (durante un partido, no solo en el diag) requiere:
1. Cambiar la gate del contador de `CENTRAL_TOP_LINK_SEQ` a `CENTRAL_USB_MONITOR` (que
   viva donde vive el monitor; competencia pura sigue sin él → byte-idéntico).
2. Agregar un campo al frame de telemetría (`telemetry_central.h` + `central_telemetry_serial.cpp`).
3. Parsearlo en la app (`protocol_central.py`) y mostrarlo en el panel de salud.
4. **Actualizar el test golden byte-exacto** (`test_telemetry_central`) + los tests Python.

Eso toca **varios archivos de la placa base + el contrato firmware↔app + un test golden**.
Se dejó para después a propósito: hay otro worker en la placa base y faltan ~12 días para
Incheon → meter un cambio de contrato cross-lenguaje ahora es riesgo innecesario, sobre todo
cuando el banco YA está cubierto por el diag. **Hacer cuando la placa base se estabilice,
coordinado con ese worker.**

## Criterio de cierre

El panel de salud del monitor muestra los frames perdidos del enlace TOP en vivo; los tests
Python + el golden quedan verdes; competencia pura (`central_robot2`) byte-idéntica.
