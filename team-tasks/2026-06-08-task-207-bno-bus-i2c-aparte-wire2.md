---
id: TASK-207
title: "BNO del TOP (ROBOT1) a un bus I²C APARTE (Wire2 24/25) — el shared bus no se puede por software"
date_created: 2026-06-08
assigned: [enzo, gustavo]
priority: P1
status: ARQUITECTURA-VALIDADA-EN-ROBOT2-2026-06-09-falta-soldar-el-2do-bno-de-robot1
blocks: [heading del arquero en produccion; orientacion fina]
tags: [hardware, i2c, bno055, top, robot1]
depends_on: []
---

# TASK-207 — BNO de ROBOT1 a bus I²C aparte (Wire2)

> ### ✅ 2026-06-09 — LA ARQUITECTURA QUEDÓ VALIDADA EN BANCO (en ROBOT2)
> Gustavo corrió **`top_robot2` (PRODUCCIÓN)** con el firmware nuevo (commit `0f503f2`,
> `sensors_imu.cpp` gateado `ROBOT2`: BNO PRIMARIO en `Wire2` 24/25 + SECUNDARIO en `Wire`):
> **`imu_L=Y imu_R=Y`** (ambos BNO leyendo) y **el `hdg` TRACKEA el giro EN PRODUCCIÓN**
> (con ToF + 2 cámaras + snapshot 100 Hz activos) y queda estable en reposo. **El freeze del
> heading NO ocurre con el BNO en bus propio** — exactamente lo que esta task predecía.
> **Para ROBOT1 queda SOLO el paso de hardware:** soldar su 2º BNO (o mover el actual) a los
> pads 24/25 (Wire2) como en robot2 → el firmware ya existe y entra con el mismo gate.
> (La cierra el equipo cuando el hardware de ROBOT1 esté hecho y validado.)

## Por qué (P1)
El **heading del BNO055 NO funciona en producción (`main_top`)**: `hdg=0.0` / `flags=0x0`. En
`diag_top_all` SÍ trackea. **Causa: contención del BNO055 en el bus `Wire` (18/19) compartido con
los 4 ToF**, bajo la carga alta de `main_top` (UART DOWN ~300 Hz + snapshot 100 Hz + localización).
El BNO055 es un mal ciudadano I²C (viola el protocolo en buses cargados — foros Bosch/Adafruit).

**El software está AGOTADO** (probados y fallidos: 100 kHz, 20 Hz, deconflict, noInterrupts —
ver `journal/2026-06-08-bno-contencion-bus-debug-y-arquero-sin-bno.md`). El fix es de hardware.

## ✅ Hallazgo confirmado en banco (2026-06-09, i²c scan corregido, commit 9da8e9e)
El Teensy 4.0 tiene **3 buses I²C**: `Wire` (LPI2C1, 18/19) · `Wire1` (LPI2C3, 16/17) ·
**`Wire2` (LPI2C4, 24/25)**. El repo venía llamando MAL "Wire1" al bus de pines 24/25; el
correcto es **`Wire2`**. Scan confirmado en ROBOT2: **BNO1 0x28 en `Wire` (18/19) + 4 ToF**;
**BNO2 0x28 VIVO en `Wire2` (24/25)**; `Wire1` (16/17) vacío.

## Convención PRIMARIO/SECUNDARIO (decisión Gustavo 2026-06-09)
- **BNO PRIMARIO = el que está SOLO en su bus (`Wire2` 24/25, sin ToF)** → sin contención I²C
  con los ToF → es el **MÁS CONFIABLE** y la fuente de heading **preferida**. (El que comparte
  bus con los ToF es el que se CONGELA.)
- **BNO SECUNDARIO = el que comparte `Wire` (18/19) con los 4 ToF** → respaldo.
- **Fix de fondo de ROBOT1:** hoy ROBOT1 tiene 1 BNO en `Wire` (con los ToF) = posición
  "secundaria"; el fix es agregarle un BNO en `Wire2` (solo) como **PRIMARIO** (= lo que ya
  tiene ROBOT2).

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
- 2026-06-09: **i²c scan corregido (commit 9da8e9e)** → el bus de los pines 24/25 es **`Wire2`**
  (LPI2C4), NO `Wire1`. Confirmado en ROBOT2 que el **2º BNO está VIVO en `Wire2` (24/25) en
  0x28**, solo en su bus (sin ToF). Eso valida la convención primario/secundario de arriba:
  el de `Wire2` (solo) es el PRIMARIO/confiable. Donde la task decía "como ROBOT2", ahora
  está confirmado por scan, no asumido.
- Histórico: el BNO de ROBOT1 estuvo en un bus aparte y lo movieron a `Wire` el 2026-05-31 para
  liberar el bus para DOWN — ese movimiento es el origen de la contención. ROBOT2 quedó con el
  bus aparte (`Wire2` 24/25).
- 2026-06-11: **la TOP de R1 fue RECABLEADA a la arquitectura de R2** en la reparación (primario
  `Wire2` 24/25 + secundario `Wire`; scan I²C 0x28 en Wire2 ✓) → donde esta task dice "flashear
  `top_robot1`" hoy corresponde **`top_robot2_pri`** (envs `top_robot1*` = cableado viejo).
  ⚠️ Los BNOs de R1 quedaron DESCONECTADOS (R1 corre sin gyro) → el criterio de cierre queda
  pendiente de reconectar BNO.
