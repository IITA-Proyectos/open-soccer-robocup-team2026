---
id: TASK-205
title: "Modo DEBUG/telemetría en TOP + app PC de monitoreo de sensores superiores"
date_created: 2026-06-06
assigned: [mariaviollaz]
priority: P1
status: in-progress
status_note: "firmware DORMIDO en competencia + modo texto humano por ENTER + 17 tests host (2026-06-13); falta banco + app (vista TOP)"
estimated_hours: 16
blocked_by: [TASK-304]
tags: [firmware, top, tooling, pc-app, telemetria]
---

# TASK-205 — Modo DEBUG/telemetría en TOP + app PC de monitoreo superior

## 1. Resumen
Réplica del sistema de la base, para la **parte superior (TOP)**: un **modo debug** en el firmware de TOP que emite telemetría por USB + una **app PC** que la muestra (cámaras, IMU, ToF y el **WorldSnapshot fusionado** que TOP manda a CENTRAL).

## 2. Contexto
Segunda fase del sistema de monitoreo (la base, TASK-304/305, es la prioritaria P0). Reusa el protocolo y el módulo puro de (de)serialización definidos en TASK-304. Diseño: `research/in-progress/2026-06-06-diseno-monitoreo-telemetria-usb-y-apps-pc.md`. Permite verificar en banco qué está percibiendo TOP (pelota, arcos, heading, distancias) y cómo lo fusiona.

## 3. Pasos concretos
1. **Modo debug en `main_top.cpp`** gateado por `-DTOP_DEBUG_TELEMETRY` (DEFAULT OFF → competencia byte-idéntico) + `[env:top_debug_telemetry]`. Emite por USB CDC (`Serial`), no por los UART inter-placa.
2. **Qué emite TOP:** detecciones de cámara (pelota/arcos: posición relativa + velocidad + confianza), heading + validez del IMU, las 4 distancias ToF, y el **WorldSnapshot** real que va a CENTRAL (la fusión).
3. Reusar el módulo puro de (de)serialización de TASK-304 (extender el esquema con los campos de TOP, versionado).
4. **App PC** en `tools/monitor-top/` (mismo stack que la base): vista de **cancha** con pelota/arcos ubicados, panel de heading/ToF, y el WorldSnapshot crudo. Modo replay sin robot.
5. README de uso.

## 4. Criterio de cierre
> **DECISIÓN 2026-06-13 (Gustavo):** mismo patrón que DOWN/TASK-306 — el monitor va DORMIDO en el
> binario de COMPETENCIA (`top_robot2_pri`, flag `-DTOP_USB_MONITOR`), NO en un env aparte. Wake por
> app (JSON) o por ENTER (texto humano). Por eso el criterio "byte-idéntico" ya NO aplica al binario
> de partido (ahora lleva el monitor dormido, match-safe → validar en banco).
- [x] Módulo puro de (de)serialización con test host en verde (`test_telemetry_top`: 17 tests, gate 60/839/0). *(2026-06-13: +5 del formateador humano `tt_format_human`.)*
- [x] **`pio run -e top_robot2_pri` compila SUCCESS** (2026-06-13, tras instalar PlatformIO en la máquina): el fix de link del snapshot cache (ensanchado a `TOP_USB_MONITOR`) quedó **confirmado** — linkea y genera `firmware.hex` (FLASH ~176 KB). Esto es compilación, no hardware → Claude lo cierra.
- [ ] Dormido al boot; ENTER → bloque de texto humano 3 s; app → stream JSON; auto-off 3 s; regresión partido OK. *(test plan en `journal/2026-06-13-top-monitor-usb-dormido-en-competencia.md`)*
- [ ] La app (`monitor_base --top`) muestra cámaras (pelota/arcos + velocidad), IMU, ToF y el WorldSnapshot fusionado, y manda `STREAM ON`+`PING` para despertar el monitor dormido. *(carril del otro agente — coordinar)*
- [ ] `IMU ZERO`/`IMU SAVE` calibran el heading EN VIVO (ack por serial).
- [ ] Modo replay sin robot + README. *(app)*

## 5. Notas / decisiones
- **2026-06-13 — firmware DORMIDO en competencia (code-complete + gate host verde).** Espejo de
  DOWN/TASK-306. Detalle, archivos y test plan: `journal/2026-06-13-top-monitor-usb-dormido-en-competencia.md`.
  Contrato actualizado: `docs/firmware/TELEMETRIA-TOP.md` (§0 modo de operación, §1-bis texto humano).
  Lo NUEVO vs la v1 del 06-07: (a) vive en el binario de partido (no `top_*_debug_telemetry`), (b)
  arranca dormido + auto-off 3 s, (c) modo TEXTO HUMANO por ENTER (pedido de Gustavo), (d) fix de
  link en `comm_central` (snapshot cache gateado solo por `TOP_DEBUG_TELEMETRY`).
- **Pendiente:** banco (el equipo cierra hardware) + la vista TOP de la app (otro agente).

## 6. Cambios de estado
- 2026-06-06 — creada (pending). Fase 2 (P1); reusa el protocolo de TASK-304.
- 2026-06-13 — **in-progress.** Firmware del monitor dormido en competencia + modo texto humano +
  17 tests host (Claude). Falta `pio run` + banco + app. Claude NO cierra hardware.
