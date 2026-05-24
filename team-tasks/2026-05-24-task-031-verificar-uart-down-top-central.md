---
id: TASK-031
title: "Verificar comunicación UART real DOWN→TOP (Serial5) y DOWN→CENTRAL (Serial1)"
date_created: 2026-05-24
assigned: [gviollaz, virginia-viollaz, elias]
priority: P1
status: pending
estimated_hours: 1
blocks: [hardware-up completo del robot (regla 8 CLAUDE.md), integración 3 placas]
blocked_by: [TASK-006 (COMM flash), placa CENTRAL y/o TOP funcionando del otro lado del cable]
tags: [hardware, down-board, comunicaciones, uart, integracion]
---

# TASK-031 — Verificar UART real DOWN→TOP y DOWN→CENTRAL

## Resumen

El subsistema DOWN está operacional a nivel banco (anillo de línea + OTOS
verificados el 2026-05-24, ver journal del mismo día). **Pero todavía
reporta por USB serial vía `diag_down`, NO por UART real**.

Para cerrar el hardware-up completo del robot según la regla 8 de
CLAUDE.md hace falta confirmar que las tramas que DOWN ARMA y MANDA por
sus 2 UARTs llegan correctamente al otro lado.

## Las 2 UARTs a verificar

Ver `hardware/electronics/down-board-pack/01-pinout-y-posiciones.md` §7
para detalles físicos:

### UART hacia TOP (canal principal, Serial5)
- Conector PCB: **U10** ("COMUNICATION")
- Pines Teensy: RX=21, TX=20
- Tráfico: `LINE_STATUS + OTOS_POSE + OTOS_VEL` @ 100 Hz
- Baud: 230400

### UART hacia CENTRAL (bus de emergencia, Serial1)
- Conector PCB: **U11**
- Pines Teensy: RX=0, TX=1
- Tráfico: `LINE_URGENT` @ 100-200 Hz (latencia objetivo < 15 ms)
- Baud: 230400

## Lo que hay que verificar

### Test 1 — DOWN→TOP funcional
1. Placa TOP encendida y con firmware operativo (no diagnóstico).
2. Cable UART U10 (DOWN) ↔ conector correspondiente en TOP.
3. Cargar `pio run -e down -t upload` (firmware de competencia, no `diag_down`).
4. En la placa TOP (o por debug serial del TOP), confirmar que llegan
   tramas `LINE_STATUS`/`OTOS_POSE`/`OTOS_VEL` a 100 Hz con CRC válido.

### Test 2 — DOWN→CENTRAL funcional
1. Placa CENTRAL encendida (firmware `central_robot1` o `central_robot2`).
2. Cable UART U11 (DOWN) ↔ Serial2 del Zircon.
3. Confirmar tramas `LINE_URGENT` llegando a CENTRAL.
4. Medir latencia (osciloscopio o trace en código): objetivo < 15 ms
   desde detección de blanco en sensor hasta lectura en CENTRAL.

### Test 3 — Comandos administrativos (RX desde TOP/CENTRAL hacia DOWN)
1. Desde TOP enviar comando "reset OTOS" o "recalibrar línea" a DOWN.
2. Verificar que DOWN responde (cambio observable en datos posteriores).
3. Idem desde CENTRAL.

## Pre-requisitos antes de empezar este TASK

1. **TASK-006** (COMM flash) cerrada o al menos en estado avanzado — sin
   COMM funcionando el robot no homologa, lo cual prioriza otras cosas.
2. Tener al menos UNA de las otras 2 placas (TOP o CENTRAL) en banco con
   firmware corriendo y conectable a DOWN por cable UART.
3. Cables UART de las 2 placas confeccionados / probados.

## Por qué está postergada (decisión usuario 2026-05-24)

El usuario decidió cerrar la fase "DOWN en banco" como un hito separado.
Verificar comunicación UART requiere coordinar con las otras placas, lo
cual NO se puede hacer hoy sin acceso a las otras placas en condiciones
funcionales. Se retoma cuando esté disponible TOP o CENTRAL listas.

## Criterio de cierre

- [ ] Test 1 (DOWN→TOP): tramas llegan a 100 Hz, CRC OK durante 60s
      continuos sin pérdida.
- [ ] Test 2 (DOWN→CENTRAL): tramas LINE_URGENT llegan, latencia < 15 ms.
- [ ] Test 3 (RX): al menos 1 comando administrativo viaja en cada
      dirección sin error.
- [ ] Journal nuevo con resultados de los 3 tests.
- [ ] Si los 3 tests pasan: **hardware-up del robot completo confirmado**
      → levantar moratoria de fábrica de papel.

## Cambios de estado

- 2026-05-24: creada al cierre de la sesión hardware-up de DOWN.
  Postergada conscientemente por el usuario hasta que las otras placas
  estén disponibles.
