---
id: TASK-019
title: "Robustez de arranque: guard last_ms>0 (3 sitios TOP) + gate debug + handlers bloqueantes"
date_created: 2026-05-18
assigned: [mariaviollaz]
priority: P1
status: pending
estimated_hours: 4
blocks: [arranque seguro del robot]
tags: [firmware, comunicacion, top-board, boot, seguridad]
depends_on: []
---

# TASK-019 — Robustez de arranque y handlers seguros

## Por qué importa (P1)

- **V-A5 / 4.5:** `top/comm_down.cpp:25-27` `fresh()` carece del guard
  `last_ms>0`; los primeros 500 ms post-boot reporta datos "frescos" sin haber
  recibido nada. Son **3 sitios** (`comm_down_is_line/pose/vel_fresh`,
  líneas 110/113/116), no 1. El patrón correcto ya existe en CENTRAL
  (`world_model.cpp:40,44`) y en cámaras (`cameras_runtime.cpp:46-48`).
- **M1:** el debug `Serial.print` (~10 campos cada 500 ms) no está gated por
  flag de competición → riesgo de stall si el USB CDC se bloquea.
- **V-A3:** `CENTRAL_CALIB_LINE`/`RESET_OTOS` recibidos en runtime bloquean el
  loop de DOWN ~320 ms (`line_ring.cpp:147-165`) → cascada LOST autoinfligida.

## Pasos concretos

1. Agregar guard `last_ms > 0` en los **3** sitios de `top/comm_down.cpp`
   (usar el patrón ya existente en CENTRAL/cámaras como referencia).
2. Estado inicial seguro: cada enlace arranca en **LOST** hasta el primer frame
   válido (replicar el guard de CENTRAL en TOP).
3. Gate del debug `Serial.print` por una flag de compilación
   (`#ifdef DEBUG_TELEMETRY`) — apagado en build de competición.
4. Handlers bloqueantes-largos (`CALIB_LINE`, `RESET_OTOS`): **rechazar o
   diferir** si `match_running` (no recalibrar 320 ms en pleno partido); solo
   permitir en estado admin/pre-partido.

## Criterio de cierre

- [ ] Los 3 `is_fresh()` de TOP devuelven false hasta el primer frame real.
- [ ] Build de competición sin debug prints (flag verificada).
- [ ] `CALIB_LINE`/`RESET_OTOS` no se ejecutan con `match_running` activo.

## Plan de prueba en hardware real

1. **Arranque:** bootear TOP sin DOWN conectado → `is_fresh()` = false (no actúa
   sobre datos inexistentes); motores quietos hasta primer mundo válido.
2. **Runtime:** enviar `CALIB_LINE` con match corriendo → el comando se rechaza/
   difiere, DOWN NO se stallea 320 ms, no hay cascada LOST.
3. **Regresión:** `CALIB_LINE` en pre-partido sí calibra normal.

## Notas / decisiones

_(completar al ejecutar)_

## Cambios de estado

- 2026-05-18: creada por Claude tras la verificación independiente, a pedido de
  Gustavo Viollaz.
