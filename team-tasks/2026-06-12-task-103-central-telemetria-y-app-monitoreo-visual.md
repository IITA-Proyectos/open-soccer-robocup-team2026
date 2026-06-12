---
id: TASK-103
title: "Modo DEBUG/telemetría en CENTRAL + app PC visual del estado de la FSM/decisiones"
date_created: 2026-06-12
assigned: []
priority: P2
status: pending
estimated_hours: 12
blocked_by: [TASK-304]
tags: [firmware, central, tooling, pc-app, telemetria]
---

# TASK-103 — Modo DEBUG/telemetría en CENTRAL + app PC visual

## 1. Resumen
Tercera (y última) fase del sistema de monitoreo: una **app PC visual** para ver
en vivo lo que la **placa CENTRAL** decide, igual que ya existe para DOWN
(TASK-304) y TOP (TASK-205). Hoy la CENTRAL solo se observa por el **panel de
texto** del monitor serie (`[CENTRAL] loop=... role=ATK state=ATK_SEARCH ...
otos=... valid=...`), que es difícil de leer en vivo mientras el robot se mueve.

## 2. Contexto
Pedido por **Elías** durante la práctica del delantero R1 del 2026-06-12: para
seguir el robot (estado de la FSM, rumbo OTOS, línea válida, transiciones,
empuje) la única herramienta en vivo es el panel de texto. La app visual
`monitor-base` solo tiene vistas de DOWN (anillo de sensores), arquero
(línea+OTOS) y TOP (radar de cancha) — **no hay vista de la CENTRAL** porque la
CENTRAL no emite la telemetría JSON que esas vistas dibujan. El panel de texto
funciona y alcanzó para la práctica (por eso esto es P2, no bloquea Incheon),
pero una vista visual aceleraría mucho el debug de banco.

Diseño base del sistema: `research/in-progress/2026-06-06-diseno-monitoreo-telemetria-usb-y-apps-pc.md`.

## 3. Pasos concretos
1. **Modo debug en `main_central.cpp`** gateado por `-DCENTRAL_DEBUG_TELEMETRY`
   (DEFAULT OFF → competencia byte-idéntica) + `[env:central_debug_telemetry]`.
   Emite por USB CDC (`Serial`), no por los UART inter-placa.
2. **Qué emite la CENTRAL:** estado de la FSM (rol + `state` actual + última
   transición y su causa), `match_running`, rumbo OTOS y/o heading + validez,
   estado de la línea (`data_valid`, ángulo, profundidad, `event_flags`), pelota
   (del WorldSnapshot ya recibido), y los comandos de motor que sale generando
   (vx/vy/w). Reusar el módulo puro de (de)serialización de TASK-304 (extender el
   esquema con los campos de la CENTRAL, versionado).
3. **App PC** en `tools/monitor-central/` (mismo stack que las otras): un panel
   con el **diagrama de estados** resaltando el `state` activo, un **dial de
   rumbo OTOS**, indicador de línea válida/borde, la pelota relativa, y los
   comandos de motor. Modo replay sin robot.
4. Idealmente reusar/leer también el **CSV de la caja negra** (mismas columnas
   que `tools/blackbox/analizar_corrida.py`) para que la vista en vivo y el
   análisis offline compartan formato.
5. README de uso.

## 4. Criterio de cierre
- [ ] `pio run -e central_robot1` / `central_robot2` (competencia) byte-idénticos con el flag OFF.
- [ ] `central_debug_telemetry` emite el stream por USB a tasa estable.
- [ ] La app muestra el estado de la FSM resaltado, rumbo OTOS, línea y comandos de motor.
- [ ] Módulo puro de (de)serialización con test host en verde (reusa el de TASK-304).
- [ ] Modo replay sin robot + README.

## 5. Notas / decisiones
- Asignado a confirmar por el equipo (Elías lo propuso; encaja en el rango
  CENTRAL TASK-100–199).
- Alternativa más barata si falta tiempo: enriquecer `analizar_corrida.py` para
  graficar la FSM/transiciones desde el CSV de la caja negra (no es en vivo,
  pero ya tenemos el dato sin tocar firmware).

## 6. Cambios de estado
- 2026-06-12 — creada (pending). Fase 3 (P2); pedida por Elías en la práctica del delantero R1. Reusa el protocolo de TASK-304.
