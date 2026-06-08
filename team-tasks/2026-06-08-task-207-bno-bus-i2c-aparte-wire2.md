---
id: TASK-207
title: "BNO del TOP (ROBOT1) a un bus I²C APARTE (Wire2 24/25) — el shared bus no se puede por software"
date_created: 2026-06-08
assigned: [enzo, gustavo]
priority: P1
status: pending
blocks: [heading del arquero en produccion; orientacion fina]
tags: [hardware, i2c, bno055, top, robot1]
depends_on: []
---

# TASK-207 — BNO de ROBOT1 a bus I²C aparte (Wire2)

## Por qué (P1)
El **heading del BNO055 NO funciona en producción (`main_top`)**: `hdg=0.0` / `flags=0x0`. En
`diag_top_all` SÍ trackea. **Causa: contención del BNO055 en el bus `Wire` (18/19) compartido con
los 4 ToF**, bajo la carga alta de `main_top` (UART DOWN ~300 Hz + snapshot 100 Hz + localización).
El BNO055 es un mal ciudadano I²C (viola el protocolo en buses cargados — foros Bosch/Adafruit).

**El software está AGOTADO** (probados y fallidos: 100 kHz, 20 Hz, deconflict, noInterrupts —
ver `journal/2026-06-08-bno-contencion-bus-debug-y-arquero-sin-bno.md`). El fix es de hardware.

## El robot ANDA sin esto (no es bloqueante de Incheon)
El arquero degrada con gracia: navega por **línea (DOWN) + cámara (pelota/arcos) + heading del OTOS**
(no del BNO). Pierde solo la orientación fina por giroscopio. Por eso es **P1, no P0**.

## Qué hacer (hardware + 1 línea de firmware)
1. **Mover los 2 cables del BNO de ROBOT1** (SDA/SCL) del bus `Wire` (pines **18/19**) al bus I²C
   **`Wire2`** (pines **24/25** del Teensy 4.0 del TOP), **igual que ROBOT2**.
   - ⚠️ **Confirmar primero** que 24/25 estén LIBRES en el TOP de ROBOT1 (deberían: Serial6 no se usa).
   - Confirmar exactamente cómo está cableado el BNO de ROBOT2 (pines + que su heading ande en producción).
2. **Firmware** (lo hace Claude cuando se confirmen los pines): construir el BNO en `&Wire2` +
   `Wire2.begin()` + `setClock(100000)`, gateado por robot (ROBOT1; ROBOT2 según su cableado). Los
   4 ToF quedan en `Wire` solos → sin contención. Sondeo 0x29/helpers también a `Wire2`.

## Criterio de cierre
- [ ] Con el BNO en Wire2 y `top_robot1` flasheado: girar el robot → el `hdg` del snapshot **trackea**
      y `flags=0x10` (heading_valid=1) **en producción** (no solo en diag).
- [ ] Quieto un rato → el heading NO se cae (estable).
- [ ] Los 4 ToF siguen OK (no se rompió el bus Wire al sacar el BNO).

## Plan de prueba en hardware real
1. Mover los 2 cables, confirmar continuidad a 24/25.
2. Flashear `top_robot1` con el firmware Wire2, abrir `diag_central_rx_all` en la CENTRAL.
3. Girar el robot → `hdg` trackea + `flags=0x10`. Comparar con el OTOS heading (deben coincidir el sentido).
4. Regresión: `diag_top_all` → 4 ToF `ready YYYY` + cámaras OK.

## Notas
- 2026-06-08: creada tras agotar el software (Claude Opus 4.8, pedido de Gustavo).
- Histórico: el BNO de ROBOT1 estuvo en Wire1/aparte y lo movieron a `Wire` el 2026-05-31 para liberar
  el bus para DOWN — ese movimiento es el origen de la contención. ROBOT2 quedó con el bus aparte.
