---
title: "TOP — Recableado 4 ToF a bus único: enumeración OK, los 4 LP funcionan (banco)"
date: 2026-05-30
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8, Anthropic)"
status: final
tags: [top-board, tof, vl53l7cx, i2c, xshut, lp, bodge, hardware-test, enzo]
robot: top (placa, R1 montado)
area: electronica
tipo: diagnostico-hardware
---

# TOP — 4 ToF en bus único: enumeración LOGRADA (los 4 LP funcionan)

> **✅ RESULTADO FINAL (2026-05-30, confirmado en banco).** El recableado de
> Enzo **FUNCIONA**: los **4 ToF VL53L7CX** quedaron en el bus `Wire` (18/19)
> y los **4 LP se controlan** desde el Teensy. Pines reales:
> **LP[0]=9, LP[1]=10, LP[2]=11, LP[3]=12**, polaridad **ACTIVO-ALTO**
> (HIGH = ToF despierto). El censo enumeró los 4 a direcciones distintas
> (0x2A/0x2B/0x2C/0x2D). Esto **libera `Wire1` (24/25) para la placa DOWN** y
> **habilita la localización 2D por trilateración** con 4 sensores. 🎉
>
> **⚠️ CORRECCIÓN IMPORTANTE — leer.** La primera versión de este journal
> concluyó (erróneamente) que "el control LP NO llega al Teensy / solo 1 ToF
> usable". **Eso quedó REFUTADO.** La causa del falso negativo: las direcciones
> I2C de los VL53L7CX **persisten entre resets del Teensy** — solo un
> **power-cycle real** (cortar y reponer energía) las vuelve a 0x29 de fábrica.
> Las corridas iniciales se hicieron sin power-cycle, así que el bus arrancaba
> sucio (ToF pegados en 0x60/0x2A de corridas previas) y el algoritmo no podía
> partir del estado de fábrica. **Procedimiento correcto: flashear → quitar y
> reponer energía → abrir monitor.** Crédito a Gustavo, que lo descubrió en
> banco. Además, la hipótesis de pines `{9,11,12,22}` tenía un pin mal: el real
> es **10**, no 22.
>
> El primer commit de esta sesión (`096108a`) quedó en la historia con la
> conclusión equivocada; este journal + el commit de corrección la superan.
> Se documenta el falso negativo a propósito: el aprendizaje del power-cycle es
> reusable para 2027 y para cualquier bring-up de VL53L7CX multi-sensor.

## Contexto: qué cambió en la placa

Hasta el 2026-05-29, el diseño documentado de TOP era **2 ToF por bus** en dos
buses I2C (`Wire` 18/19 y `Wire1` 24/25), cada bus con 2 sensores que se
enumeran con 1 solo XSHUT. Forense previo (journal 2026-05-25) había concluido
que los pads **XSHUT/LP e INT de los 4 ToF estaban como No-Connect en el PCB
rev 1.0** — máximo 2 ToFs sin rework.

El 2026-05-30 Enzo hizo un **bodge físico** para superar eso:
- Los **4 ToF ahora cuelgan del bus principal `Wire` (18/19)**, junto con un BNO055.
- A cada ToF le cableó la pata **LP** a un pin del Teensy, **reusando la
  traza/pad que originalmente iba a INT** (anulando la función INT).
- Objetivo: **liberar `Wire1` (24/25)** para usarlo en comunicación con la
  placa DOWN (inferior).

Ese bodge es **físico y no está en ningún archivo del repo** (el schematic/PCB
siguen mostrando LP e INT como NC). Por eso no se podía saber por documentación
a qué pin del Teensy cayó cada LP — había que **medirlo empíricamente**. El
bodge superó la limitación del forense del 2026-05-25.

## Qué se hizo (5 programas de diagnóstico, banco)

Sketches incrementales (todos compilan limpio, offline). NO son firmware de
competencia.

1. **`diag_top_i2c_scan`** — escáner de los 2 buses I2C, apoyo.
2. **`diag_top_tof_lp_discover`** — duerme candidatos y los despierta de a uno
   mirando 0x29. Autodetecta polaridad.
3. **`diag_top_tof_enumerate`** — estrategia de Gustavo: aparta el "pegado",
   peina los controlables, tolera 1 LP roto. Hipótesis {9,11,12,22}.
4. **`diag_top_tof_census`** — definitivo: prueba **ambas polaridades** + se
   **auto-recupera** de la persistencia de direcciones. **El que confirmó los 4 LP.**
5. **`diag_top_tof_quad_live`** — lee los 4 ToF **simultáneamente** con la lib
   Adafruit (la buena). Para confirmar ranging + mapear dirección→posición.

## Resultado de banco — el camino completo

### Primeras corridas (FALSO NEGATIVO, sin power-cycle)

Las corridas iniciales de `diag_top_tof_census`, hechas sin cortar energía
entre flasheos, daban:

```
Tras recuperar, 0x29: PRESENTE (hay >=1 ToF vivo)
--- Intento polaridad ACTIVO-ALTO ---
  pin 2/9/10/11/12/13/22/23 : sin efecto   (los 8)
--- Intento polaridad ACTIVO-BAJO ---
  pin 2/9/10/11/12/13/22/23 : sin efecto   (los 8)
Polaridad que funciono: (ninguna)
ToF aislados por LP: 0
```

Esto NOS HIZO CONCLUIR (mal) que ningún LP funcionaba. **El error fue de
procedimiento, no de hardware**: el bus arrancaba con ToF pegados en
direcciones de corridas previas (0x60, 0x2A...) porque las direcciones I2C
**persisten entre resets**. Sin partir de 0x29 de fábrica, el algoritmo de
enumeración no podía aislar nada.

### Corrida correcta (tras flashear + POWER-CYCLE)

Gustavo flasheó y **cortó/repuso energía** antes de abrir el monitor. Salida:

```
BNO055 (0x28): PRESENTE (bus OK)
Recuperando ToF dispersos -> 0x29 ...
Tras recuperar, 0x29: AUSENTE (ningun ToF responde!)   <- todos dormidos (LP=LOW al boot)

--- Intento polaridad ACTIVO-ALTO (wake=HIGH) ---
  todos dormidos -> 0x29 ausente (buena señal: LP apaga)
  pin  2 : sin efecto
  pin  9 : LP OK -> ToF movido a 0x2A
  pin 10 : LP OK -> ToF movido a 0x2B
  pin 11 : LP OK -> ToF movido a 0x2C
  pin 12 : LP OK -> ToF movido a 0x2D

Polaridad que funciono: ACTIVO-ALTO
ToF aislados por LP: 4
   LP pin 9  ->  0x2A
   LP pin 10 ->  0x2B
   LP pin 11 ->  0x2C
   LP pin 12 ->  0x2D
VEREDICTO: LP control FUNCIONA en >=3 ToF. Enumeracion viable!
  -> PIN_TOF_XSHUT = { 9 10 11 12 }
```

Lecturas duras (las correctas):
- **Bus I2C sano** (BNO en 0x28).
- **Los 4 LP se controlan** desde pines 9/10/11/12, **activo-alto**.
- **Los 4 ToF enumerados** a 0x2A/0x2B/0x2C/0x2D.
- (El "ToF leibles totales: 0" del final es un bug cosmético del census: hace
  `set_all(sleep)` antes del conteo final, así que cuenta con todos dormidos.
  El dato válido es "ToF aislados por LP: 4".)

### Confirmación de ranging (pendiente de correr)

`diag_top_tof_quad_live` enumera con los pines confirmados y lee los 4 ToF
**simultáneamente** con la lib Adafruit, imprimiendo distancia media de cada
uno. Sirve para (a) confirmar que los 4 MIDEN de verdad (no solo que responden
I2C) y (b) mapear dirección → posición física tapando cada sensor.

## El procedimiento que importa (reusable 2027)

**Las direcciones I2C de los VL53L7CX persisten mientras el módulo tenga 3V3.**
Un reset del Teensy NO las borra. Para cualquier bring-up / re-enumeración:

1. `pio run -e <env> -t upload`
2. **Quitar energía de la placa y reponerla** (no alcanza el botón de reset).
3. `pio device monitor -b 115200`

El census/quad_live igual hacen un "recover" por software, pero el power-cycle
es lo más confiable para partir del estado de fábrica (todos en 0x29).

## El bodge FUNCIONÓ — qué habilita

Poner los **4 ToF en un solo bus** requería **3 LP controlables** para
enumerar (con 4 en un bus, hay que dormir 3 para aislar). **El bodge entrega
los 4**, así que:
- ✅ **`Wire1` (24/25) queda libre** para comunicación con la placa DOWN, que
  era el objetivo de Enzo.
- ✅ **Localización 2D por trilateración habilitada** — hay 4 ToF en las 4
  orientaciones (frontal/trasero/izq/der), suficiente para X e Y.

La hipótesis previa (que sin el bodge el máximo era 2 ToF, journal 2026-05-25)
queda superada por el bodge físico de Enzo + el procedimiento de power-cycle.

## Pendiente humano

**TASK-201** (Enzo) — **DEGRADADA**: el diagnóstico de banco ya confirmó que
los 4 LP funcionan, así que el multímetro **ya no es necesario** para
desbloquear. Queda solo como verificación opcional.

Lo que SÍ sigue pendiente para cerrar el subsistema ToF:
1. **Correr `diag_top_tof_quad_live`** (flashear → power-cycle → monitor) y
   confirmar que los 4 ToF MIDEN distancia real.
2. **Mapear dirección → posición física**: tapar cada sensor y anotar qué
   dirección (0x2A..0x2D) corresponde a frontal / trasero / izq / der.
3. Con ese mapeo, actualizar `pinout_robot1.h`:
   `PIN_TOF_XSHUT = {9,10,11,12}` (reordenado según posición) +
   `TOF_I2C_ADDR_ASSIGNED` + `NUM_TOF_ACTIVE = 4` + flags `ROBOT_HAS_TOF_*`.
4. **Extender `sensors_tof.cpp` (HAL Sprint B)** para enumerar los 4 al boot
   con esta secuencia (dormir todos → despertar de a uno → setAddress) y
   leerlos en runtime. Hoy el módulo vivo solo lee el frontal en 0x29.

## Estado para Incheon (honesto, actualizado)

- **4 ToF enumerables y controlables** ✅ — gran avance respecto a "1 frontal".
- **Localización 2D por trilateración: DESBLOQUEADA a nivel hardware.** Falta
  el trabajo de firmware (Sprint B) + validación de ranging/mapeo (puntos 1-4).
- **Decisión TASK-033/034** (cuántos ToF, arquitectura de localización) ahora
  se puede tomar con 4 ToF reales sobre la mesa, no como hipótesis.

## Archivos

- `src/diag/diag_top_i2c_scan.cpp` — scanner de buses I2C (apoyo).
- `src/diag/diag_top_tof_lp_discover.cpp` — descubridor de LP.
- `src/diag/diag_top_tof_enumerate.cpp` — enumerador tolerante a 1 LP roto.
- `src/diag/diag_top_tof_census.cpp` — censo auto-polaridad (el que confirmó los 4 LP).
- `src/diag/diag_top_tof_quad_live.cpp` — lectura simultánea de los 4 ToF (nuevo).
- `platformio.ini` — 5 envs `[env:diag_top_*]` nuevos.
- `team-tasks/2026-05-30-task-201-*.md` — multímetro LP (degradada: ya confirmado por banco).

## Lección capturada (para 2027)

**VL53L7CX en bus compartido:** las direcciones I2C persisten mientras haya
3V3. Cualquier diagnóstico de enumeración tiene que partir de un power-cycle
real, no de un reset. Un falso negativo "ningún LP funciona" casi siempre es
"el bus arrancó sucio". Procedimiento: flashear → cortar/reponer energía →
monitor. Este aprendizaje le ahorró al equipo una sesión de multímetro
innecesaria.
