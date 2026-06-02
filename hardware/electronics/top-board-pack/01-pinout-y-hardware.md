---
title: "Placa TOP — Pinout y hardware (Roboliga2026 TOP + Teensy 4.0 master)"
date: 2026-05-24
status: vigente
parte-de: top-board-pack
fuentes:
  - firmware/top/config_top.h (código vivo)
  - ground-truth/SCH_Roboliga2026_TOP_2026-04-12.json (schematic crudo)
  - ground-truth/PCB_PCB_Roboliga2026_TOP_2026-04-12.json (PCB layout crudo)
  - ground-truth/Schematic_Roboliga2026_TOP_2026-04-12.pdf (schematic legible)
---

# Placa TOP — Pinout y hardware

> Esta placa es el **cerebro sensorial** del robot: 1 Teensy 4.0 master con 2
> cámaras OpenMV, 2 BNO055 (IMU dual), 4 ToF multizona, 1 HC-SR04 ultrasonido,
> dipswitch de rol, y 4 UARTs activos hacia las otras placas. El layout del PCB
> está disponible en `ground-truth/` para correr el extractor automático del
> pack DOWN si se quiere regenerar este doc desde el SCH JSON.

> **🔧 ACTUALIZACIÓN 2026-06-02 — fix mapeo UART TOP (COMM/CENTRAL), confirmado EN BANCO.**
> El **Teensy 4.0 NO expone Serial7 (pines 28/29) en el borde** → son pads SMD
> traseros, no cableables con header. Por eso el mapa UART vigente del TOP es:
> **S1=DOWN · S2=COMM (7/8) · S3=cámara frontal (U8) · S4=CENTRAL
> (`WORLD_SNAPSHOT`, 16/17) · S5=cámara TRASERA**.
> 1. **COMM** pasó a **Serial2 (RX pin 7 / TX pin 8), baud 115200** (los pines 7/8
>    estaban libres tras mover el HC-SR04 a 3/4).
> 2. El link **TOP→CENTRAL** vuelve a **Serial4 (RX pin 16 / TX pin 17), baud
>    230400**, porque Serial7 no es cableable. Cable: **TOP pin 17 (TX4) → CENTRAL
>    pin 28 (RX7)** + GND común. El lado CENTRAL es un **Teensy 4.1** que SÍ tiene
>    28/29 en el borde, así que **sigue recibiendo en su Serial7 (RX7 = pin 28)** —
>    ese lado NO cambia.
> 3. La **cámara trasera** queda en **Serial5 (RX pin 21)** — sin cambios.
> Donde más abajo el doc diga "S4=COMM", "Serial7 → CENTRAL" o "CENTRAL recibe en
> su Serial1", está **superado** por esta nota
> *(fix 2026-06-02: el Teensy 4.0 no expone Serial7 28/29 en el borde; COMM=Serial2 7/8, CENTRAL=Serial4 16/17)*.
>
> **🔧 ACTUALIZACIÓN 2026-05-31 — cámara trasera + HC-SR04 (confirmado EN BANCO).**
> 1. La **cámara trasera** quedó **soldada en Serial5 (RX pin 21)** — confirmado con
>    `diag_top_cameras` (FORMATO OK).
> 2. El **HC-SR04** se cableó en **TRIG=pin 4 / ECHO=pin 3** (no 6/7). El viejo
>    **"conflicto pin 7"** ya **no existe**: los pines 7/8 (Serial2) quedaron libres
>    → hoy son COMM (ver nota 2026-06-02).
> Firmware: `cameras_runtime.cpp` (trasera Serial5) + `pinout_common.h` (HC-SR04 4/3).

## 1. Hardware sobre el que corre

| Componente | Cantidad | Nota |
|---|---|---|
| MCU **Teensy 4.0** (U14) | 1 | Cortex-M7 a 600 MHz, 1 MB RAM, 2 MB flash, **7 UARTs hardware** |
| Cámaras **OpenMV N6** (antes H7 Plus) | 2 | UART (Serial3 frontal + **Serial5** trasera), 19200 baud, protocolo viejo 9 bytes/packet |
| **BNO055** IMU | 2 | Ambos en `Wire` (18/19): LEFT=0x28, RIGHT=0x29 (pad ADR a 3V3). Recableado 2026-05-31 → `Wire1` (24/25) libre para DOWN |
| Sensor ToF **VL53L7CX** | 4 fijos (plan: 6) | TODOS en `Wire` (I²C0), LP individual por pin. Enumeran a 0x2A..0x2D. 8×8 SPAD multizona. Plan de escalado: +2 móviles para pelota |
| Ultrasonido **HC-SR04** | 1 | TRIG=pin 4 / ECHO=pin 3 (banco 2026-05-31). Frontal, fallback de ToF |
| Conector a placa **COMM** (ESP32-C6) | 1 | UART (**Serial2, pines 7/8**, 115200). Bridge a árbitros + ESP-NOW partner (fix 2026-06-02) |
| Conector a placa **DOWN** (sensores piso) | 1 | UART (Serial1). Recibe DOWN_OTOS_POSE/VEL + LINE_STATUS |
| Conector a placa **CENTRAL** (cerebro/motores) | 1 | UART (**Serial4, pines 16/17**, 230400). Envía `WORLD_SNAPSHOT`. (fix 2026-06-02: el Teensy 4.0 no expone Serial7 28/29 en el borde; CENTRAL=Serial4 16/17. Cable TX4 pin 17 → CENTRAL RX7 pin 28) |
| Dipswitch selección de rol | 1 | Pin 10 con pull-up. LOW = arquero, HIGH = delantero |
| Conector Dean-T-F batería | 1 | 7.4 V LiPo (compartido con CENTRAL y DOWN) |
| Reguladores MP1584-EN | 2 | 7.4 V → 5 V + 7.4 V → 3.3 V |
| LED de estado | 1 | LED_BUILTIN (pin 13) |

### Datos del PCB extraídos de los gerbers (referencia, 2026-04-15)

| Métrica | Valor |
|---|---|
| Nombre CAD | `Roboliga2026_TOP` |
| Bounding box | 224.0 × 97.5 mm |
| Envelope real (con arcos) | ≈ 229.7 × 103.7 mm |
| Forma | Contorno irregular curvo, deck superior contorneado |
| Capas | 2 (cobre top + bottom) |
| Footprints (clusters) | ~19 multi-pad + 10–12 pads aislados |
| Cluster grande (35 pads) | en (169.5, 79.9) mm = IC/módulo/conector grande |
| Headers 2.54 mm | ~6 clusters de 6 pads (2×3) |
| Drills NPTH mecánicos | **8× M2 + 4× M3 + 1× 5 mm** = 13 puntos de montaje al chasis |

## 2. Pinout completo del Teensy 4.0 (U14)

### 2.1 Buses I²C — distribución crítica

> **🔧 ACTUALIZACIÓN 2026-05-30 (recableado de Enzo, confirmado en banco).**
> Los **4 ToF se movieron al bus `Wire` (I²C0, 18/19)**, con la pata **LP** de
> cada uno cableada por bodge a un pin del Teensy. Esto **liberó `Wire1`
> (24/25)** — ahora reservado para comunicación con la placa **DOWN**, NO para
> ToF. La tabla refleja el estado nuevo.

| Bus | API Arduino | Pin Arduino SDA | Pin Arduino SCL | Periféricos | Dirección I²C |
|---|---|---|---|---|---|
| **I²C #0** | `Wire` | **18** | **19** | BNO055 izq (U10) + BNO055 der (U11) + **los 4 ToF** (U2/U3/U5/U17) | BNO izq=0x28, BNO der=0x29 (ADR a 3V3); ToF=0x2A/0x2B/0x2C/0x2D (vía LP) |
| **I²C #1** | `Wire1` | **25** ⚠️ remap | **24** ⚠️ remap | **(libre para placa DOWN)** — el 2do BNO se movió a `Wire` (0x29) el 2026-05-31 | — |

> **Remap crítico de `Wire1`** (Q3 confirmado por análisis PCB, 2026-05-10):
> los pines default de `Wire1` en Teensy 4.0 son 16/17, pero esos están
> ocupados por **Serial4 (UART hacia CENTRAL; fix 2026-06-02)**. El PCB **ruteó `Wire1` a los
> pines 24/25**. El firmware debe hacer:
> ```cpp
> Wire1.setSCL(24);    // ANTES de Wire1.begin()
> Wire1.setSDA(25);
> Wire1.begin();
> ```
> Pendiente físicamente confirmar con TASK-003.

Los 2 BNO055 son 0x28 de fábrica, pero el 2do tiene el **pad ADR puenteado a 3V3
→ 0x29**, así que ambos conviven en `Wire` (recableado 2026-05-31; esto liberó
`Wire1` para DOWN). Los **4 ToF se enumeran al boot** en `Wire`: arrancan todos en 0x29
de fábrica, se duermen todos por LP, se despierta uno por uno y a cada uno se
le asigna 0x2A → 0x2B → 0x2C → 0x2D. **Ninguno queda en 0x29.**

> ⚠️ **PROCEDIMIENTO OBLIGATORIO al probar/enumerar ToF.** Las direcciones I²C
> de los VL53L7CX **persisten mientras el módulo tenga 3V3** — un reset del
> Teensy NO las borra. Hay que **QUITAR ENERGÍA Y REPONERLA (power-cycle)** tras
> flashear, o el bus arranca sucio (ToF pegados en direcciones de corridas
> previas) y la enumeración no parte de fábrica. Esto causó un falso negativo
> en el primer diagnóstico. Ver `journal/2026-05-30-top-tof-4-en-bus-unico-enumeracion-ok.md`.

### 2.2 UARTs — las 5 conexiones externas

| Serial | Pin RX | Pin TX | Conector PCB | Conectado a | Baud | Rol |
|---|---|---|---|---|---|---|
| **`Serial1`** | **0** | **1** | U16 "UART_COMM_IN" | placa **DOWN** | 230400 | Recibe ODOM_POSE/VEL del DOWN (100 Hz) |
| **`Serial2`** | **7** | **8** | (cable a pines 7/8) | placa **COMM** (ESP32-C6) | 115200 | Bridge árbitros + ESP-NOW partner. **COMM movido acá (fix 2026-06-02)** — los pines 7/8 quedaron libres al mover el HC-SR04 a 3/4. |
| **`Serial3`** | **15** | **14** | U8 "UART-CAMERA1" | OpenMV **cámara 1** (frontal) | 19200 | Protocolo viejo OpenMV (9 bytes/packet). ✅ FORMATO OK en banco. |
| **`Serial4`** | **16** | **17** | U15 "UART_COMM_OUT" | placa **CENTRAL** | 230400 | **Envía `WORLD_SNAPSHOT` (100 Hz)** por TX4=pin 17. **Link a CENTRAL movido acá (fix 2026-06-02)**: el Teensy 4.0 no expone Serial7 28/29 en el borde. Cable TX4 pin 17 → **CENTRAL RX7 pin 28** (CENTRAL es Teensy 4.1, recibe en su Serial7). |
| **`Serial5`** | **21** | **20** | (pin 21 — confirmar conector) | OpenMV **cámara 2** (trasera) | 19200 | **Cámara trasera soldada acá** (RX pin 21) — ✅ confirmado en banco 2026-05-31 (`diag_top_cameras`, FORMATO OK). |
| Serial6 | ~~25~~ | ~~24~~ | — | **BLOQUEADO** | — | Pines tomados por `Wire1` remap |
| ~~`Serial7`~~ | ~~28~~ | ~~29~~ | — | **NO cableable** | — | El **Teensy 4.0 no expone Serial7 (28/29) en el borde** (pads SMD traseros). Por eso el link a CENTRAL está en **Serial4 (16/17)**, no acá (fix 2026-06-02). |

### 2.3 Sensores ToF — pines LP (bodge, confirmados en banco 2026-05-30)

Los 4 ToF cuelgan del **mismo bus `Wire`** (I²C0, 18/19). Cada uno tiene su
pata **LP** cableada por bodge a un pin del Teensy. Pines **confirmados** con
`diag_top_tof_census` (R1, tras power-cycle), polaridad **ACTIVO-ALTO**
(HIGH = ToF despierto):

| ToF (índice firmware) | I²C bus | Pin LP | Dir | Posición física | Ángulo montaje |
|---|---|---|---|---|---|
| **ToF[0]** | Wire (I²C0) | **9**  | 0x2A | **FRENTE**    | 0°   |
| **ToF[1]** | Wire (I²C0) | **10** ⚠️ | 0x2B | **ATRÁS**     | 180° |
| **ToF[2]** | Wire (I²C0) | **11** | 0x2C | **DERECHA**   | 270° |
| **ToF[3]** | Wire (I²C0) | **12** | 0x2D | **IZQUIERDA** | 90°  |

> ✅ **Pines y posiciones CONFIRMADOS en banco** (2026-05-30/31, Gustavo): SET
> de pines {9,10,11,12} (`diag_top_tof_census`) + mapeo pin→posición. La
> hipótesis vieja `{2,3,4,5}` / `0x52..0x58` quedó descartada.
>
> ⚠️ **Ángulos corregidos**: `TOF_MOUNT_ANGLE_DEG` era `{0,180,90,270}` (cruzaba
> der/izq en localization) → ahora `{0,180,270,90}`. Ver journal 2026-05-31.
>
> ⚠️ **Orientación interna de zonas PENDIENTE**: cada ToF entrega una grilla
> 8×8; el orden de zonas depende del montaje. El **izquierdo (TOF3) es de otro
> fabricante, montado mirando abajo** → puede tener arriba/abajo o izq/der
> invertidos. Verificar con `diag_top_tof_zonemap`. Importa para el barrido
> tipo lidar-360 (no para el promedio de zonas actual).
>
> ⚠️ **Conflicto pin 10:** era dipswitch de rol y ahora es LP de ToF. No pueden
> coexistir → reubicar la lectura de rol a un pin libre (22/23). → Enzo.

Hardware: **VL53L7CX**. La enumeración I²C por LP es estándar: dormir todos los
LP, despertar el que queremos (HIGH), cambiar su dirección, repetir.

**Plan de escalado a 6 ToF (diseño objetivo, no cableado todavía):** además de
los 4 fijos, se planean **2 ToF móviles** sobre un mecanismo que los orienta
para **ubicar la pelota** (complemento de la cámara). Colgarían del mismo bus
`Wire` con su LP propio, enumerados a 0x2E/0x2F. Ver `pinout_common.h`
(`NUM_TOF_MAX = 6`).

### 2.4 HC-SR04 (ultrasonido frontal)

| Pin Arduino | Función |
|---|---|
| **4** | TRIG (output, pulso 10 µs para iniciar medición) |
| **3** | ECHO (input, ancho del pulso proporcional a distancia) ⚠️ 5 V → divisor |

> ✅ **Cableado 2026-05-31:** el HC-SR04 quedó en **TRIG=pin 4 / ECHO=pin 3** (pines
> ex-XSHUT ToF, hoy libres; no son UART). El viejo "conflicto pin 7" ya no aplica —
> el HC-SR04 no usa el pin 7. ⚠️ **Nivel:** el ECHO del HC-SR04 sale a **5 V** y el
> Teensy 4.0 **NO tolera 5 V** → usar divisor (1k+2k → 3.3 V) o alimentar el sensor
> a 3.3 V antes de conectar ECHO al pin 3.

Lectura bloqueante ~25 ms — usar fuera del loop crítico de fusión.

### 2.5 Selección de rol del robot

| Pin Arduino | Función | Valor LOW | Valor HIGH |
|---|---|---|---|
| **10** | Dipswitch de rol (pull-up interno) | Arquero (GOALKEEPER) | Delantero (ATTACKER) |

Se lee 1 vez en `setup()` y se mantiene fijo. A futuro podrá cambiar
dinámicamente vía mensaje del coach/COMM.

### 2.6 LED de estado

Pin Arduino **13** (LED_BUILTIN).

## 3. Resumen — uso por pin del Teensy 4.0

| Pin Arduino | Función | Confianza |
|---|---|---|
| 0 | RX1 (Serial1) ← DOWN | ✅ |
| 1 | TX1 (Serial1) → DOWN | ✅ |
| 2 | libre (ya NO es XSHUT ToF) | ✅ |
| 3 | **HC-SR04 ECHO** (cableado 2026-05-31) ⚠️ 5 V → divisor | ✅ banco |
| 4 | **HC-SR04 TRIG** (cableado 2026-05-31) | ✅ banco |
| 5 | libre (ya NO es XSHUT ToF) | ✅ |
| 6 | libre (HC-SR04 movido a pin 4) | ✅ |
| 7 | **RX2 (Serial2) ← COMM** (fix 2026-06-02) | ✅ |
| 8 | **TX2 (Serial2) → COMM** (fix 2026-06-02) | ✅ |
| 9 | **LP ToF[0] FRENTE** (bodge → 0x2A) | ✅ banco 2026-05-30 |
| 10 | **LP ToF[1] ATRÁS** (bodge → 0x2B) ⚠️ colisiona con dipswitch rol | ⚠️ reubicar rol |
| 11 | **LP ToF[2] DERECHA** (bodge → 0x2C) | ✅ banco 2026-05-30 |
| 12 | **LP ToF[3] IZQUIERDA** (bodge → 0x2D) | ✅ banco 2026-05-30 |
| 13 | LED_BUILTIN | ✅ |
| 14 | TX3 (Serial3) → Cámara 1 | ✅ |
| 15 | RX3 (Serial3) ← Cámara 1 | ✅ |
| 16 | RX4 (Serial4) ← CENTRAL (= SCL1 default, NO usado para I²C) (fix 2026-06-02) | ✅ |
| 17 | **TX4 (Serial4) → CENTRAL** (`WORLD_SNAPSHOT`; = SDA1 default, NO usado para I²C). Cable a CENTRAL RX7 pin 28 (fix 2026-06-02) | ✅ |
| 18 | SDA0 (Wire) — BNO055 + **los 4 ToF** (bus único, bodge 2026-05-30) | ✅ |
| 19 | SCL0 (Wire) — BNO055 + **los 4 ToF** (bus único) | ✅ |
| 20 | TX5 (Serial5) → cámara trasera (sin uso; la cam sólo transmite) | ✅ 2026-05-31 |
| 21 | RX5 (Serial5) ← **cámara trasera** (datos, soldada 2026-05-31) | ✅ banco |
| 22, 23 | libres (candidatos para reubicar el dipswitch de rol) | ✅ |
| **24** | **SCL1 (Wire1 REMAP)** — BNO055 der + **libre para placa DOWN** (ya NO ToF, bodge 2026-05-30) | ⚠️ confirmar |
| **25** | **SDA1 (Wire1 REMAP)** — BNO055 der + **libre para placa DOWN** (ya NO ToF) | ⚠️ confirmar |
| 28, 29 | **NO cableables** — el Teensy 4.0 no expone Serial7 (28/29) en el borde (pads SMD traseros). El link a CENTRAL está en Serial4 (16/17), no acá (fix 2026-06-02) | ✅ |
| 26–33 | libres | ✅ |

## 4. Pendientes humanos (NO bloquean uso del pack, pero hay que resolver)

| # | Pendiente | Asignado | Bloqueante para |
|---|---|---|---|
| 1 | Confirmar `Wire1` remap a pines 24/25 con multímetro (TASK-003) | Enzo | I²C bus 1 funcionando → 1 BNO055 + 2 ToF |
| 2 | ✅ Pines 20/21 (Serial5) = **cámara trasera** (confirmado en banco 2026-05-31). El link a CENTRAL está en **Serial4 (16/17)** (fix 2026-06-02: el Teensy 4.0 no expone Serial7 28/29 en el borde). | ✅ | — |
| 3 | ✅ HC-SR04 cableado en **pines 3/4** (banco 2026-05-31) → el "conflicto pin 7" ya no existe (pin 7 libre). | ✅ | — |
| 3b | ✅ parcial: cámara trasera confirmada en **Serial5 (pin 21)** con FORMATO OK (2026-05-31). Falta validar el link a CENTRAL en **Serial4** (cable TX4 pin 17 → CENTRAL RX7 pin 28) (fix 2026-06-02). | Gustavo | snapshot a CENTRAL |
| 4 | ✅ RESUELTO en banco 2026-05-30: LP de los 4 ToF = **{9,10,11,12}**, activo-alto, enumeran a 0x2A..0x2D (bus `Wire` único) | ✅ | — |
| 5 | **Mapear dirección → posición física** (`diag_top_tof_quad_live`) + **reubicar dipswitch de rol** (pin 10 colisiona con LP ToF[1]) | Enzo + firmware | Localización por trilateración + lectura de rol confiable |
| 6 | **Recuperar BOM y Pick&Place del proyecto EasyEDA TOP** (TASK-013) | Enzo | Trazabilidad de componentes para repuestos en Incheon |
| 7 | Cruzar BOM con `mapa-pines-placas-nuevas.md` para verificar componentes | Enzo + Claude | Confiar 100% del schematic-vs-fabricación |

## 5. Regeneración automática del pinout (opcional)

El pack contiene `ground-truth/SCH_Roboliga2026_TOP_2026-04-12.json` y
`PCB_PCB_Roboliga2026_TOP_2026-04-12.json`. Estos son los mismos formatos
EasyEDA que tiene la placa DOWN, así que el script
`software/teensy/Soccer 2026/scripts/extract_pinout_from_schematic.py` puede
ser apuntado a estos JSONs para regenerar automáticamente el pinout con
todas las nets y conexiones:

```bash
python extract_pinout_from_schematic.py \
  --json hardware/electronics/down-board-pack/ground-truth/SCH_Roboliga2026_TOP_2026-04-12.json \
  --pcb-json hardware/electronics/down-board-pack/ground-truth/PCB_PCB_Roboliga2026_TOP_2026-04-12.json
```

⚠️ El script fue diseñado pensando en DOWN (busca `Uxx` con `comment~U7~`,
`comment~U1~..U4~`, `comment~U5~U6~`). Para usarlo con TOP hay que ajustar
los designators que busca: el Teensy 4.0 es U14, los BNO055 son U10 y U11,
los ToF son U2/U3/U5/U17, etc. Trabajo de 1-2 h adaptarlo. Cuando se haga,
este doc pasa a tener confianza ✅ en todos los pines (como `01-pinout-y-posiciones.md`
del pack DOWN).

## 6. Referencias

- Código vivo del firmware: [`firmware/top/`](firmware/top/) (especialmente `config_top.h`).
- Schematic legible: [`ground-truth/Schematic_Roboliga2026_TOP_2026-04-12.pdf`](ground-truth/Schematic_Roboliga2026_TOP_2026-04-12.pdf).
- BOM CSV (parcial — falta Pick&Place): [`ground-truth/BOM_Roboliga2026_TOP_2026-04-12.csv`](ground-truth/BOM_Roboliga2026_TOP_2026-04-12.csv).
- Pinout del CENTRAL (la otra punta del enlace, su Serial7 = RX7 pin 28; CENTRAL es Teensy 4.1): [`../central-board-pack/01-pinout-y-hardware.md`](../central-board-pack/01-pinout-y-hardware.md).
- Pinout del DOWN (la otra punta de Serial1): [`../down-board-pack/01-pinout-y-posiciones.md`](../down-board-pack/01-pinout-y-posiciones.md).
- Pinout del COMM (la otra punta de Serial2, fix 2026-06-02): `../comm-board/2026-05-17-placa-comm-componentes-y-circuito.md`.
