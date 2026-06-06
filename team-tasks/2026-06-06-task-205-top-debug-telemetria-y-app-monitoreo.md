---
id: TASK-205
title: "Modo DEBUG/telemetría en TOP + app PC de monitoreo de sensores superiores"
date_created: 2026-06-06
assigned: [mariaviollaz]
priority: P1
status: pending
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
- [ ] `pio run -e top` (competencia) byte-idéntico con el flag OFF.
- [ ] `top_debug_telemetry` emite el stream por USB a tasa estable.
- [ ] La app muestra cámaras (pelota/arcos + velocidad), IMU, ToF y el WorldSnapshot fusionado.
- [ ] Módulo puro de (de)serialización con test host en verde (reusa el de TASK-304).
- [ ] Modo replay sin robot + README.

## 5. Notas / decisiones
- (vacío — completar al ejecutar)

## 6. Cambios de estado
- 2026-06-06 — creada (pending). Fase 2 (P1); reusa el protocolo de TASK-304.
