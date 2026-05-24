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
| Sensor ToF **VL53L5/L7CX** | 4 | I²C en 2 buses, XSHUT individual. 8×8 SPAD multizona |
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

| Bus | API Arduino | Pin Arduino SDA | Pin Arduino SCL | Periféricos | Dirección I²C |
|---|---|---|---|---|---|
| **I²C #0** | `Wire` | **18** | **19** | BNO055 izq (U10) + ToF U2 + ToF U3 | BNO055=0x28; ToF=0x52 y 0x54 (asignadas vía XSHUT) |
| **I²C #1** | `Wire1` | **25** ⚠️ remap | **24** ⚠️ remap | BNO055 der (U11) + ToF U5 + ToF U17 | BNO055=0x28; ToF=0x56 y 0x58 |

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
distintos. Los 4 ToF se enumeran al boot cambiando XSHUT secuencialmente:
default 0x52 → cambian a 0x54 → 0x56 → 0x58.

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

### 2.3 Sensores ToF (4×) — pines XSHUT

| ToF | I²C bus | Pin XSHUT (Arduino) | Dirección asignada al boot |
|---|---|---|---|
| **ToF 1** (U2) | Wire (I²C0) | **2** | 0x52 ⚠️ tentativa |
| **ToF 2** (U3) | Wire (I²C0) | **3** | 0x54 ⚠️ tentativa |
| **ToF 3** (U5) | Wire1 (I²C1) | **4** | 0x56 ⚠️ tentativa |
| **ToF 4** (U17) | Wire1 (I²C1) | **5** | 0x58 ⚠️ tentativa |

Hardware comprado (según coach Q4): **VL53L7CX disponibles**, VL53L5CX en
pedido. Pines XSHUT tentativos — confirmar con TASK-003 extendida. La
enumeración I²C por XSHUT es estándar: pre-condicion, XSHUT del que queremos
enumerar HIGH, otros LOW; se cambia la dirección con `setI2CAddress()`; se
suben los siguientes XSHUT.

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
| 2 | XSHUT ToF 1 | ⚠️ tentativo |
| 3 | XSHUT ToF 2 | ⚠️ tentativo |
| 4 | XSHUT ToF 3 | ⚠️ tentativo |
| 5 | XSHUT ToF 4 | ⚠️ tentativo |
| 6 | HC-SR04 TRIG | ✅ |
| 7 | HC-SR04 ECHO **+** RX2 (Serial2) ← CENTRAL | ⚠️ **POSIBLE CONFLICTO** |
| 8 | TX2 (Serial2) → CENTRAL | ⚠️ tentativo (conector U1 sin doc clara) |
| 9 | libre | ✅ |
| 10 | Dipswitch rol (pull-up) | ✅ |
| 11, 12 | libres | ✅ |
| 13 | LED_BUILTIN | ✅ |
| 14 | TX3 (Serial3) → Cámara 1 | ✅ |
| 15 | RX3 (Serial3) ← Cámara 1 | ✅ |
| 16 | RX4 (Serial4) ← COMM (= SCL1 default, NO usado para I²C) | ✅ |
| 17 | TX4 (Serial4) → COMM (= SDA1 default, NO usado para I²C) | ✅ |
| 18 | SDA0 (Wire) — BNO055 izq + ToF 1/2 | ✅ |
| 19 | SCL0 (Wire) — BNO055 izq + ToF 1/2 | ✅ |
| 20 | TX5 (Serial5) → Cámara 2 | ✅ |
| 21 | RX5 (Serial5) ← Cámara 2 | ✅ |
| 22, 23 | libres | ✅ |
| **24** | **SCL1 (Wire1 REMAP)** — BNO055 der + ToF 3/4 | ⚠️ Q3 — confirmar con TASK-003 |
| **25** | **SDA1 (Wire1 REMAP)** — BNO055 der + ToF 3/4 | ⚠️ Q3 — confirmar con TASK-003 |
| 26–33 | libres | ✅ |

## 4. Pendientes humanos (NO bloquean uso del pack, pero hay que resolver)

| # | Pendiente | Asignado | Bloqueante para |
|---|---|---|---|
| 1 | Confirmar `Wire1` remap a pines 24/25 con multímetro (TASK-003) | Enzo | I²C bus 1 funcionando → 1 BNO055 + 2 ToF |
| 2 | **Confirmar pines 7/8 del conector U1** (Serial2 hacia CENTRAL) | Enzo | UART TOP→CENTRAL → robot no recibe WORLD_SNAPSHOT |
| 3 | **Resolver conflicto pin 7** (HC-SR04 ECHO **vs** Serial2 RX2) | Enzo + firmware | Una de las dos funciones no puede coexistir |
| 4 | Confirmar pines XSHUT de los 4 ToF (TASK-003 ext) | Enzo | Enumeración I²C de los 4 ToF al boot |
| 5 | Confirmar direcciones I²C asignadas (0x52/54/56/58) | firmware | Coincidencia con código `sensors_tof.cpp` |
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
