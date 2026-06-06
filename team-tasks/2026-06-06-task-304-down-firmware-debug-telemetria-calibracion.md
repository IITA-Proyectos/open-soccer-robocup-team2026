---
id: TASK-304
title: "Modo DEBUG/telemetría + calibración automática en el firmware de DOWN (stream USB)"
date_created: 2026-06-06
assigned: [mariaviollaz, gviollaz]
priority: P0
status: pending
estimated_hours: 10
blocks: [TASK-305]
tags: [firmware, down, tooling, telemetria, calibracion, prioritaria]
---

# TASK-304 — Modo DEBUG/telemetría + calibración en el firmware de DOWN

## 1. Resumen
Agregar al **firmware de competencia de DOWN** un **modo DEBUG** (gateado, byte-idéntico OFF) que **emita telemetría estructurada por USB** y acepte **comandos de calibración**, para que una app de PC (TASK-305) muestre en vivo los 32 sensores de luz, lo que ve de las líneas y **la interpretación real que DOWN manda a CENTRAL** (LineStatusV2).

## 2. Contexto
Es el **próximo desarrollo PRIORITARIO** del equipo (pedido 2026-06-06). Hoy el diagnóstico de línea es por texto (`diag_down`, `diag_down_calibracion`). Queremos ver gráficamente, en banco, moviendo el robot sobre las líneas: (a) que los 32 sensores funcionan, (b) qué líneas ve, (c) **qué LineStatusV2 está enviando** a CENTRAL/TOP. Debe ser un modo del **firmware real** (no un sketch aparte) para ver exactamente lo que el robot "piensa". Diseño completo: `research/in-progress/2026-06-06-diseno-monitoreo-telemetria-usb-y-apps-pc.md`. Conecta con el roadmap de telemetría F1 (CANbus, MEJORAS-PENDIENTES (e) E4) como su versión USB de hoy.

## 3. Pasos concretos
1. Definir el **esquema de telemetría v1** (recomendado: líneas de texto CSV/JSON por frame, versionado). Campos: `raw[32]`, `white[32]`, calib (`carpet[32]`, `white_cal[32]`, `threshold[32]`) y la **LineStatusV2** que arma `dm_update` (line_angle, escape_angle, penetration, cross_track, line_present, sensors, quality, sample_age).
2. Extraer la **serialización del frame de telemetría** y el **parser de comandos** a un **módulo PURO host-testeable** en `src/shared/` (+ su test) — el gate lo verifica. El glue de `Serial` queda en `src/down/`.
3. Agregar el **modo debug** en `main_down.cpp`, gateado por `-DDOWN_DEBUG_TELEMETRY` (DEFAULT OFF → competencia byte-idéntico) y/o activable por comando USB. Cuando activo: emitir el frame a 20–50 Hz por `Serial` (USB CDC, NO los UART inter-placa).
4. Implementar **comandos por USB** (host→DOWN): start/stop stream, calibrar carpet, calibrar blanco, **auto-calib** (capturar min/máx por sensor mientras se pasa el robot por las líneas y computar umbral), **guardar calib a EEPROM**. Reusar `line_ring_calibrate_carpet/white/set_calibration` + `calib_storage`/`ec_*`.
5. Agregar `[env:down_debug_telemetry]` (extends `env:down` + `-DDOWN_DEBUG_TELEMETRY`), aditivo.
6. Documentar el protocolo (esquema versionado) junto al diseño.

## 4. Criterio de cierre
- [ ] `pio run -e down` (competencia) compila y el binario es byte-idéntico al actual (flag OFF).
- [ ] `pio run -e down_debug_telemetry -t upload` emite el stream por USB a tasa estable.
- [ ] El stream incluye raw[32], white[32], calib y la LineStatusV2 real (la misma que va a CENTRAL).
- [ ] Comandos de calib (carpet/blanco/auto/guardar-EEPROM) funcionan vía USB.
- [ ] Módulo puro de (de)serialización del frame con test host en verde (gate).
- [ ] Protocolo documentado y versionado.

## 5. Notas / decisiones
- (vacío — completar al ejecutar)

## 6. Cambios de estado
- 2026-06-06 — creada (pending). Pedido del coach como desarrollo prioritario.
