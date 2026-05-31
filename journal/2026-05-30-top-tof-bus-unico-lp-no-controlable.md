---
title: "TOP — Recableado 4 ToF a bus único: el control LP/XSHUT no llega al Teensy (diagnóstico de banco)"
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

# TOP — 4 ToF a bus único: el control LP no llega al Teensy

> **TL;DR.** El 2026-05-30 Enzo recableó la placa TOP: movió los **4 ToF
> VL53L7CX al I2C principal (`Wire`, 18/19)** junto a un BNO, y cableó por
> bodge la pata **LP** de cada ToF (reusando la traza que iba a INT, anulando
> INT) para liberar `Wire1` (24/25) hacia la placa DOWN. Hicimos diagnóstico
> de banco con 3 programas nuevos. **Veredicto definitivo: el Teensy NO
> controla el LP de ningún ToF** — ni en polaridad activo-alto ni activo-bajo,
> probando los 8 pines libres candidatos. **Solo 1 ToF es usable** (responde
> fijo en 0x29, casi seguro el frontal U2 que ya andaba). Sin control LP
> individual es **físicamente imposible** enumerar N sensores idénticos en un
> bus (todos nacen en 0x29 y se mueven juntos). **Es problema de hardware, no
> de firmware.** Acción: Enzo mide continuidad con multímetro (TASK-201).

## Contexto: qué cambió en la placa

Hasta el 2026-05-29, el diseño documentado de TOP era **2 ToF por bus** en dos
buses I2C (`Wire` 18/19 y `Wire1` 24/25), cada bus con 2 sensores que se
enumeran con 1 solo XSHUT. Forense previo (journal 2026-05-25) había
confirmado que los pads **XSHUT/LP e INT de los 4 ToF estaban como No-Connect
en el PCB rev 1.0** — máximo 2 ToFs sin rework.

El 2026-05-30 Enzo hizo un **bodge físico** para superar eso:
- Los **4 ToF ahora cuelgan del bus principal `Wire` (18/19)**, junto con un BNO055.
- A cada ToF le cableó la pata **LP** a un pin del Teensy, **reusando la
  traza/pad que originalmente iba a INT** (anulando la función INT).
- Objetivo: **liberar `Wire1` (24/25)** para usarlo en comunicación con la
  placa DOWN (inferior).

Ese bodge es **físico y no está en ningún archivo del repo** (el schematic/PCB
siguen mostrando LP e INT como NC). Por eso no se podía saber por documentación
a qué pin del Teensy cayó cada LP — había que **medirlo empíricamente**.

## Qué se hizo (3 programas de diagnóstico, banco)

Se crearon 3 sketches incrementales (todos compilan limpio, 0 warnings,
offline). NO son firmware de competencia.

1. **`diag_top_tof_lp_discover`** — duerme todos los pines candidatos y los
   despierta de a uno mirando cuándo aparece 0x29. Autodetecta polaridad.
2. **`diag_top_tof_enumerate`** — implementa la estrategia de Gustavo: apartar
   el ToF "pegado" a una dir de park, peinar los controlables a 0x2A/0x2B/0x2C,
   tolerar 1 LP roto. Hipótesis de pines {9,11,12,22}.
3. **`diag_top_tof_census`** — versión **definitiva**, que cierra los 2 puntos
   ciegos de los anteriores:
   - **Persistencia:** las direcciones I2C de los VL53L7CX **sobreviven al
     reset del Teensy** (solo un power-cycle del módulo las borra). Las
     corridas previas dejaban un ToF pegado en 0x60 y ensuciaban el arranque.
     El census **se auto-recupera** juntando los ToF dispersos de vuelta a 0x29.
   - **Polaridad:** los anteriores asumían LP activo-alto. Si el LP fuera
     activo-bajo, producían el mismo output "0 LP" aunque todo anduviera. El
     census **prueba ambas polaridades** automáticamente.

## Resultado de banco (concluyente)

Salida del `diag_top_tof_census` (COM11, R1 montado, repetido estable):

```
BNO055 (0x28): PRESENTE (bus OK)
Tras recuperar, 0x29: PRESENTE (hay >=1 ToF vivo)

--- Intento polaridad ACTIVO-ALTO (wake=HIGH) ---
  todos dormidos pero 0x29 presente -> aparto pegado a 0x60 : OK
  pin 2/9/10/11/12/13/22/23 : sin efecto   (los 8)

--- Intento polaridad ACTIVO-BAJO (wake=LOW) ---
  todos dormidos pero 0x29 presente -> aparto pegado a 0x60 : OK
  pin 2/9/10/11/12/13/22/23 : sin efecto   (los 8)

Polaridad que funciono: (ninguna)
ToF aislados por LP: 0
ToF leibles totales en el bus: 1
VEREDICTO: CERO ToF controlables por LP en ninguna polaridad.
```

Lecturas duras:
- **Bus I2C sano**: el BNO contesta en 0x28.
- **Hay al menos 1 ToF vivo**: contesta en 0x29 (no se puede apagar).
- **Ningún pin del Teensy controla un LP de ToF**: los 8 candidatos libres
  (2,9,10,11,12,13,22,23), en ambas polaridades, dan "sin efecto".
- **Solo 1 ToF es leíble.** Casi con certeza es el **frontal U2**, que ya
  funcionaba en 0x29 desde el 2026-05-24 sin necesitar XSHUT.

## Por qué esto NO se arregla por software

Los 4 VL53L7CX salen de fábrica en la **misma** dirección 0x29. Para usar más
de uno en un bus compartido hay que **reasignarles la dirección de a uno**, y
eso **solo es posible si se puede apagar a los demás por su pin LP/XSHUT**. Si
dos ToF están en 0x29 simultáneamente, el comando de cambio-de-dirección lo
reciben **ambos** y se mueven juntos → no se pueden separar. **No existe**
mecanismo I2C para enumerar N sensores idénticos sin control LP individual. Es
una limitación del chip, no del firmware. El software llegó a su límite.

## Tensión de diseño (para registrar)

Poner los **4 ToF en un solo bus** OBLIGA a tener **3 XSHUT/LP funcionando**
(con 4 en un bus, 3 hay que dormirlos para enumerar). El diseño original
**2+2 en dos buses** solo necesitaba **1 XSHUT por bus**. El recableado a bus
único (para liberar `Wire1` hacia DOWN) **depende** de que el bodge LP
funcione. Si el bodge no se puede hacer andar, las opciones son:
- (A) Arreglar el bodge para tener ≥3 LP controlables (TASK-201).
- (B) Volver a 2+2 en dos buses y resolver de otra forma el bus para DOWN
  (p.ej. usar Serial/UART hacia DOWN en vez de I2C, que es lo que ya
  contempla el firmware: Serial1).

Esta decisión se cruza con TASK-033 (cuántos ToF para Incheon) y TASK-034
(arquitectura de localización).

## Pendiente humano

**TASK-201** (Enzo, P1): con multímetro, para cada ToF (U2/U3/U5/U17):
1. Continuidad del pad **LP (pin 1 del módulo)** → ¿qué pin del Teensy?
2. Confirmar que el bodge esté en el pad **LP (pin 1)**, no en **GPIO/INT
   (pin 2)** — son adyacentes y el INT es el que el PCB tenía como NC.
3. 3V3 en Vin + GND de cada ToF.
4. Continuidad SDA→18 y SCL→19 de cada ToF.

Con los pines reales, se re-corre `diag_top_tof_census` apuntando a ellos.

## Estado para Incheon (honesto)

- **1 ToF frontal funcional** (U2 en 0x29) → distancia frontal OK, como ya
  estaba. Suficiente para evasión frontal básica.
- **Localización 2D por trilateración: bloqueada** — necesita ≥3 ToF en ejes
  distintos. No depende de firmware sino del bodge (TASK-201) o de cambiar la
  arquitectura (TASK-033/034).

## Archivos

- `src/diag/diag_top_tof_lp_discover.cpp` — descubridor de LP (nuevo).
- `src/diag/diag_top_tof_enumerate.cpp` — enumerador tolerante a 1 LP roto (nuevo).
- `src/diag/diag_top_tof_census.cpp` — censo definitivo auto-polaridad (nuevo).
- `src/diag/diag_top_i2c_scan.cpp` — scanner de buses I2C (nuevo, de apoyo).
- `platformio.ini` — 4 envs `[env:diag_top_*]` nuevos.
- `team-tasks/2026-05-30-task-201-*.md` — multímetro de continuidad LP (nuevo).
