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

## 1. Hardware sobre el que corre

| Componente | Cantidad | Nota |
|---|---|---|
| MCU **Teensy 4.0** (U14) | 1 | Cortex-M7 a 600 MHz, 1 MB RAM, 2 MB flash, **7 UARTs hardware** |
| Cámaras **OpenMV H7 / H7 Plus** | 2 | UART (Serial3 + Serial5), 19200 baud, protocolo viejo 9 bytes/packet |
| **BNO055** IMU | 2 | I²C dual (`Wire` + `Wire1`). Ambos dirección 0x28, por eso buses separados |
| Sensor ToF **VL53L7CX** | 4 fijos (plan: 6) | TODOS en `Wire` (I²C0), LP individual por pin. Enumeran a 0x2A..0x2D. 8×8 SPAD multizona. Plan de escalado: +2 móviles para pelota |
| Ultrasonido **HC-SR04** | 1 | TRIG/ECHO pines 6/7. Frontal, fallback de ToF |
| Conector a placa **COMM** (ESP32-C6) | 1 | UART (Serial4). Bridge a árbitros + ESP-NOW partner |
| Conector a placa **DOWN** (sensores piso) | 1 | UART (Serial1). Recibe DOWN_OTOS_POSE/VEL + LINE_STATUS |
| Conector a placa **CENTRAL** (cerebro/motores) | 1 | UART (Serial2, pines 7/8). Envía `WORLD_SNAPSHOT` |
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
| **I²C #0** | `Wire` | **18** | **19** | BNO055 izq (U10) + **los 4 ToF** (U2/U3/U5/U17) | BNO055=0x28; ToF=0x2A/0x2B/0x2C/0x2D (asignadas vía LP) |
| **I²C #1** | `Wire1` | **25** ⚠️ remap | **24** ⚠️ remap | BNO055 der (U11) + **(libre para placa DOWN)** | BNO055=0x28 |

> **Remap crítico de `Wire1`** (Q3 confirmado por análisis PCB, 2026-05-10):
> los pines default de `Wire1` en Teensy 4.0 son 16/17, pero esos están
> ocupados por **Serial4 (UART hacia COMM)**. El PCB **ruteó `Wire1` a los
> pines 24/25**. El firmware debe hacer:
> ```cpp
> Wire1.setSCL(24);    // ANTES de Wire1.begin()
> Wire1.setSDA(25);
> Wire1.begin();
> ```
> Pendiente físicamente confirmar con TASK-003.

Los 2 BNO055 **comparten dirección 0x28** de fábrica → obligatorio en buses
distintos. Los **4 ToF se enumeran al boot** en `Wire`: arrancan todos en 0x29
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
| **`Serial2`** | **7** | **8** | U1 "OUT1/OUT2/RX_OUT/TX_OUT" | placa **CENTRAL** | 230400 | **Envía `WORLD_SNAPSHOT` (100 Hz)**. ⚠️ TENTATIVO — confirmar con Enzo si U1 RX_OUT/TX_OUT van a pines 7/8 |
| **`Serial3`** | **15** | **14** | U8 "UART-CAMERA1" | OpenMV **cámara 1** (frontal) | 19200 | Protocolo viejo OpenMV (9 bytes/packet) |
| **`Serial4`** | **16** | **17** | U15 "UART_COMM_OUT" | placa **COMM** (ESP32-C6) | 115200 | Bridge árbitros + ESP-NOW partner |
| **`Serial5`** | **21** | **20** | U9 "UART-CAMERA2" | OpenMV **cámara 2** (trasera) | 19200 | Protocolo viejo OpenMV |
| Serial6 | ~~25~~ | ~~24~~ | — | **BLOQUEADO** | — | Pines tomados por `Wire1` remap |
| Serial7 | 28 | 29 | — | **LIBRE** | — | Expansión futura |

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
| **6** | TRIG (output, pulso 10 µs para iniciar medición) |
| **7** | ECHO (input, ancho del pulso proporcional a distancia) ⚠️ |

> ⚠️ **Posible conflicto pin 7**: el config dice que pin 7 es ECHO del HC-SR04
> y ADEMÁS RX2 de Serial2 (UART hacia CENTRAL). **No pueden ser ambas cosas a
> la vez.** Hay que confirmar con Enzo cuál es el cableado físico real y
> decidir: o el HC-SR04 va a otros pines, o el Serial2 hacia CENTRAL va a otro
> UART (Serial7 está libre, pines 28/29).

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
| 3 | libre (ya NO es XSHUT ToF) | ✅ |
| 4 | libre (ya NO es XSHUT ToF) | ✅ |
| 5 | libre (ya NO es XSHUT ToF) | ✅ |
| 6 | HC-SR04 TRIG | ✅ |
| 7 | HC-SR04 ECHO **+** RX2 (Serial2) ← CENTRAL | ⚠️ **POSIBLE CONFLICTO** |
| 8 | TX2 (Serial2) → CENTRAL | ⚠️ tentativo (conector U1 sin doc clara) |
| 9 | **LP ToF[0]** (bodge) | ✅ banco 2026-05-30 |
| 10 | **LP ToF[1]** (bodge) ⚠️ colisiona con dipswitch rol | ⚠️ conflicto |
| 11 | **LP ToF[2]** (bodge) | ✅ banco 2026-05-30 |
| 12 | **LP ToF[3]** (bodge) | ✅ banco 2026-05-30 |
| 13 | LED_BUILTIN | ✅ |
| 14 | TX3 (Serial3) → Cámara 1 | ✅ |
| 15 | RX3 (Serial3) ← Cámara 1 | ✅ |
| 16 | RX4 (Serial4) ← COMM (= SCL1 default, NO usado para I²C) | ✅ |
| 17 | TX4 (Serial4) → COMM (= SDA1 default, NO usado para I²C) | ✅ |
| 18 | SDA0 (Wire) — BNO055 izq + **los 4 ToF** | ✅ |
| 19 | SCL0 (Wire) — BNO055 izq + **los 4 ToF** | ✅ |
| 20 | TX5 (Serial5) → Cámara 2 | ✅ |
| 21 | RX5 (Serial5) ← Cámara 2 | ✅ |
| 22, 23 | libres (candidatos para reubicar el dipswitch de rol) | ✅ |
| **24** | **SCL1 (Wire1 REMAP)** — BNO055 der + **libre para placa DOWN** (ya NO ToF) | ⚠️ confirmar |
| **25** | **SDA1 (Wire1 REMAP)** — BNO055 der + **libre para placa DOWN** (ya NO ToF) | ⚠️ confirmar |
| 26–33 | libres | ✅ |

## 4. Pendientes humanos (NO bloquean uso del pack, pero hay que resolver)

| # | Pendiente | Asignado | Bloqueante para |
|---|---|---|---|
| 1 | Confirmar `Wire1` remap a pines 24/25 con multímetro (TASK-003) | Enzo | I²C bus 1 funcionando → 1 BNO055 + 2 ToF |
| 2 | **Confirmar pines 7/8 del conector U1** (Serial2 hacia CENTRAL) | Enzo | UART TOP→CENTRAL → robot no recibe WORLD_SNAPSHOT |
| 3 | **Resolver conflicto pin 7** (HC-SR04 ECHO **vs** Serial2 RX2) | Enzo + firmware | Una de las dos funciones no puede coexistir |
| 4 | ~~Confirmar pines XSHUT/LP de los 4 ToF~~ → **RESUELTO en banco 2026-05-30**: LP = {9,10,11,12}, activo-alto, enumeran 0x2A..0x2D | ✅ | — |
| 5 | **Mapear dirección → posición física** (tapar cada sensor con `diag_top_tof_quad_live`) + reubicar dipswitch de rol (pin 10 colisiona con LP ToF) | Enzo + firmware | Localización por trilateración + lectura de rol confiable |
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
- Pinout del CENTRAL (la otra punta de Serial2): [`../central-board-pack/01-pinout-y-hardware.md`](../central-board-pack/01-pinout-y-hardware.md).
- Pinout del DOWN (la otra punta de Serial1): [`../down-board-pack/01-pinout-y-posiciones.md`](../down-board-pack/01-pinout-y-posiciones.md).
- Pinout del COMM (la otra punta de Serial4): `../comm-board/2026-05-17-placa-comm-componentes-y-circuito.md`.
