---
id: TASK-100
title: "Validar en banco ingest de línea DOWN→CENTRAL + frenado de emergencia (post-fix P0 contrato LineStatusV2)"
date_created: 2026-05-29
assigned: [Virginia, Elías]
priority: P0
status: pending
estimated_hours: 2
blocked_by: [task-036]
tags: [firmware, hardware, central, down, uart, linea]
---

> **🔧 CORRECCIÓN 2026-06-03 (leer antes de cablear).** El cuerpo de abajo dice
> "Serial2 / pines 7-8" y depende del "conflicto 7/8 (TASK-036)". Eso está
> **SUPERADO**: el link DOWN→CENTRAL es **Serial1 (RX1 = pin 0)** y el conflicto
> 7/8 fue **RESUELTO** (2026-05-31; los pines 7/8 quedaron para el motor 2).
> ⚠️ NO conectar el cable de DOWN al pin 7 — ahí va un H-bridge. El receptor de
> banco vigente decodifica `LineStatusV2` por Serial1. Ver ESTADO-ACTUAL.md y
> `DIAG-CENTRAL-COMM-DOWN.md`.

## Resumen

Confirmar en hardware real que CENTRAL ahora **sí recibe** los frames
`LineStatusV2` que manda DOWN por Serial2 y que el frenado de emergencia
de borde dispara cuando corresponde.

## Contexto

Hasta el 2026-05-29 había un **contrato roto (P0)**: `comm_down.cpp` exigía
un payload de `sizeof(LineStatus)` = **5 bytes**, pero DOWN encodea
`LineStatusV2` = **16 bytes** (`down_encode_line`, `main_down.cpp:96`). El
chequeo `payload_len == 5` daba `false` para todo frame de 16 bytes → **CENTRAL
descartaba silenciosamente el 100% de los frames de línea**. Efecto en cancha:
el robot no frenaba en la línea (salidas de cancha), el arquero no podía correr
su lógica de borde, y `LINE_AVOID` nunca veía datos.

El fix (host-verificado, ver journal `2026-05-29-fix-contrato-linea-central.md`):
- CENTRAL ahora decodifica `LineStatusV2` (16 bytes) vía `lsv2_from_frame`
  (`src/shared/line_view.h`).
- `world_model` guarda `LineStatusV2` y mapea los campos con helpers puros
  compartidos (mismo código que el test host-native).
- `world_model_imminent_exit()` ahora honra el contrato documentado en
  `strategy.cpp:17`: si el robot está **lifted**, devuelve `false`.

**Esto es CÓDIGO. No está validado en hardware.** Claude no cierra esta TASK.

> **Pre-requisito (bloqueo):** TASK-036. El conflicto de pines 7/8 (Serial2 vs
> motor del Zircon) tiene que estar resuelto. Si TASK-036 determina que 7/8 son
> pines de motor, hay que **migrar Serial2 a Serial7 (pines 28/29)** ANTES de
> este test, o no va a llegar ni un byte de DOWN. Ver `ESTADO-ACTUAL.md` Avance
> 2026-05-28.

## Pasos concretos

1. Flashear CENTRAL (`central_robot1` o `central_robot2`) y DOWN con sus
   firmwares actuales. Conectar Serial2 de CENTRAL ↔ TX de DOWN (cruzado),
   GND común.
2. Agregar (o usar) un print de debug en CENTRAL que muestre cada ~500 ms:
   `comm_down_get_frames_received()`, `comm_down_get_crc_errors()`,
   `world_model_line_is_fresh()`, `world_model_imminent_exit()`,
   `world_model_get_line_angle_deg()`.
3. Con DOWN **fuera** de la línea (sobre carpeta): confirmar que
   `frames_received` sube sostenido, `crc_errors` ≈ 0, `imminent_exit` = N.
4. Acercar DOWN a la línea blanca hasta cubrir varios sensores: confirmar que
   `imminent_exit` pasa a Y y `line_angle` es coherente con la orientación
   física (mover perpendicular → el ángulo cambia de signo al cruzar).
5. Con motores habilitados y robot avanzando hacia la línea: confirmar que
   `motors_brake()` dispara (el robot frena) y se libera al alejarse.
6. Levantar el robot en el aire (lifted): confirmar que `imminent_exit`
   vuelve a N aunque DOWN siga viendo "línea" (gating por `EV_LIFTED`).

## Criterio de cierre

- [ ] `frames_received` incrementa de forma sostenida (no queda en 0).
- [ ] `crc_errors` se mantiene en ~0 a régimen.
- [ ] `imminent_exit` = Y sólo cuando DOWN está realmente sobre/cerca de la línea.
- [ ] `line_angle` reportado coincide en signo y magnitud aproximada con la
      orientación física de la línea respecto al robot.
- [ ] Con motores: el robot **frena** ante línea inminente y **recupera** al alejarse.
- [ ] Lifted: `imminent_exit` = N aunque haya "línea" (no frena en el aire).
- [ ] Resultados anotados en "Notas" + journal de la sesión de banco.

## Notas / decisiones

(Completar durante el test en banco.)

## Cambios de estado

- 2026-05-29 — creada tras fix del contrato de línea (P0). Author: Claude Opus 4.7 (Anthropic). Requested-by: Viollaz.
