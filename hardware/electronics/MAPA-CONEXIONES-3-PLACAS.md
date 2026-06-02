---
title: "Mapa único de conexiones — las 3 placas (UART + I²C + USB)"
date: 2026-06-02
status: vigente
area: hardware / firmware
tipo: referencia-consolidada
---

# Mapa de conexiones — TOP · CENTRAL · DOWN

> Referencia ÚNICA de qué puerto usa cada placa y para qué. Consolida lo que estaba
> disperso en los pinouts por placa. **La verdad de pines vive en el código**
> (`src/top/pinout_common.h` + `pinout_robotN.h`, `src/central/config_central.h`,
> `src/down/config_down.h`); si algo acá contradice al código vivo, gana el código.

## 0. ⚠️ Lo más importante: "Serial" NO es uno solo

En Teensy hay **dos cosas distintas** que se llaman "serial", y **no chocan entre sí**:

| Nombre | Qué es | Para qué |
|---|---|---|
| **`Serial`** | El **USB** (el conector + cable). Hardware aparte. | **Flashear y monitorear** (`pio device monitor`, `Serial.print` de debug). |
| **`Serial1`…`Serial8`** | Los **UART por pines** del Teensy. | Comunicación **entre placas**. |

👉 **Conclusión clave:** el USB (`Serial`) está **siempre disponible** para programar y
monitorear las **3 placas a la vez**, y **NO consume ningún pin** ni interfiere con los
enlaces entre placas. **No hay ningún UART que "liberar"** para simplificar la
programación — ya es independiente. (Teensy 4.0 tiene 7 UART y la 4.1 tiene 8; sobran.)

---

## 1. UART por placa

### TOP — Teensy 4.0 (master: visión + IMU + ToF + árbitros + fusión)
| Puerto | RX | TX | Conecta con | Baud | Para qué |
|--------|----|----|-------------|------|----------|
| **`Serial`** (USB) | — | — | PC | — | flasheo + monitor (debug) |
| **`Serial1`** | 0 | 1 | ← DOWN | 230400 | recibe odometría OTOS (pose + vel) + línea (`LineStatusV2`, broadcast; cacheada, no consumida aún) |
| **`Serial2`** | 7 | 8 | ↔ COMM (ESP32-C6) | 115200 | árbitros + partner ESP-NOW |
| **`Serial3`** | 15 | 14 | ← cámara **frontal** (U8) | 19200 | blobs pelota/arcos (9 bytes) |
| **`Serial4`** | 16 | 17 | → CENTRAL | 230400 | envía `WORLD_SNAPSHOT` (100 Hz) |
| **`Serial5`** | 21 | 20 | ← cámara **trasera** | 19200 | blobs pelota/arcos (9 bytes) |

> **⚠️ FIX 2026-06-02 (Gustavo, banco):** el Teensy 4.0 del TOP **NO expone `Serial7` (28/29)
> en el borde** — son pads SMD traseros, no cableables con header. TASK-204 había puesto el
> enlace a CENTRAL en `Serial7` por error y el TOP **nunca le llegaba** a la CENTRAL
> (`snap_fresh=N`). Mapeo REAL del TOP: **`Serial2` (7/8) = COMM**, **`Serial4` (16/17) =
> CENTRAL**. El lado CENTRAL (Teensy **4.1**) SÍ tiene 28/29 en el borde → su `Serial7` queda igual.

*Libres:* `Serial6` (24/25) no se usa como UART (esos pines los toma `Wire1`) · `Serial7` (28/29) = pads traseros del 4.0, **no usables**.

### CENTRAL — Teensy 4.1 sobre Zircon Rev v15 (cerebro: FSM + PIDs + motores)
| Puerto | RX | TX | Conecta con | Baud | Para qué |
|--------|----|----|-------------|------|----------|
| **`Serial`** (USB) | — | — | PC | — | flasheo + monitor (debug) |
| **`Serial7`** | 28 | 29 | ← TOP | 230400 | recibe `WORLD_SNAPSHOT` |
| **`Serial1`** | 0 | 1 | ← DOWN | 230400 | recibe `LINE_URGENT` / `LineStatusV2` + odometría OTOS (`Pose2D`/`Velocity2D`, broadcast; OTOS para control de movimiento — Capa 2) |

*Motores:* GPIO directo (INA/INB/PWM), **no UART**. *Kicker* (R2): GPIO pin 23 (placeholder, confirmar Enzo).
*✅ Reasignado 2026-05-31 (decisión Gustavo, cableado en banco):* el link a **TOP usa `Serial7` (28/29)** y el de **DOWN usa `Serial1` (0/1)**. Así `Serial2` (pines 7/8) queda **LIBRE para el driver del motor 2 (U17)** → el viejo conflicto **F8/TASK-036 está RESUELTO** (ya no hay UART en 7/8).

### DOWN — Teensy 4.0 (sensores de piso: 32 línea + 2 OTOS)
| Puerto | RX | TX | Conecta con | Baud | Para qué |
|--------|----|----|-------------|------|----------|
| **`Serial`** (USB) | — | — | PC | — | flasheo + monitor (debug) |
| **`Serial1`** | 0 | 1 | → CENTRAL | 230400 | difunde línea (`LineStatusV2`) + odometría OTOS (broadcast) |
| **`Serial5`** | 21 | 20 | → TOP | 230400 | difunde línea (`LineStatusV2`) + odometría OTOS (broadcast) |

---

## 2. Enlaces entre placas (las dos puntas)

> Cada enlace usa un **nombre de Serial distinto en cada punta** (son Teensy distintos
> en placas separadas; lo único que importa es el **pin físico + GND común**). Hay dos
> "Serial5" y dos "RX en pin 0" en el sistema — **no colisionan** porque están en placas
> distintas.

| Enlace | TX (placa · puerto · pin) | RX (placa · puerto · pin) | Baud | Estado |
|--------|---------------------------|---------------------------|------|--------|
| **TOP → CENTRAL** (snapshot) | TOP · `Serial4` · **pin 17** (TX4) | CENTRAL · `Serial7` · **pin 28** (RX7) | 230400 | 🔧 fix 2026-06-02 (era `Serial7` en el TOP, inexistente en el borde del 4.0) |
| **DOWN → TOP** (línea + odometría OTOS) | DOWN · `Serial5` · **pin 20** | TOP · `Serial1` · **pin 0** | 230400 | ⚠️ sin cablear |
| **DOWN → CENTRAL** (línea + odometría OTOS) | DOWN · `Serial1` · **pin 1** | CENTRAL · `Serial1` · **pin 0** | 230400 | ✅ cable validado (DOWN→CENTRAL OK en banco 2026-06-02) |
| **TOP ↔ COMM** (árbitros) | TOP · `Serial2` · **7/8** | COMM (ESP32-C6) | 115200 | 🔧 fix 2026-06-02 (era `Serial4`) |
| **cámara frontal → TOP** | cam · UART3 | TOP · `Serial3` · pin 15 | 19200 | ✅ FORMATO OK |
| **cámara trasera → TOP** | cam · UART3 | TOP · `Serial5` · pin 21 | 19200 | ✅ FORMATO OK |

**Siempre:** cada enlace necesita **GND común** entre las dos placas.

---

## 3. I²C (ToF, BNO, OTOS) — NO son UART

I²C es un **bus**: muchos chips comparten **los mismos 2 pines** (SDA + SCL) y se
distinguen por **dirección**, no por pin. "Comparten pines" a propósito. Es un
periférico distinto de los UART y del USB.

### TOP — todo en un solo bus `Wire`
| Bus | SDA | SCL | Chips (dirección) |
|-----|-----|-----|-------------------|
| **`Wire`** (I²C0) | **18** | **19** | BNO055 **LEFT** (0x28) + BNO055 **RIGHT** (0x29, pad ADR a 3V3) + **4 ToF** VL53L7CX (0x2A · 0x2B · 0x2C · 0x2D) |
| `Wire1` (I²C1) | 25 | 24 (remap) | **libre** (quedó libre al mover el 2º BNO a `Wire`) |

- **Los 6 sensores (2 BNO + 4 ToF) cuelgan de los MISMOS 2 pines (18/19).**
- Los 4 ToF arrancan todos en 0x29 (chocarían) → cada uno tiene una pata **LP** a un
  GPIO (**pines {9, 10, 11, 12}**, eso **NO es I²C**, es GPIO) que al boot los despierta
  de a uno y les reasigna 0x2A..0x2D. ⚠️ Las direcciones **persisten con 3V3** →
  **power-cycle obligatorio** al re-enumerar.
- ⚠️ **pin 10** (LP del ToF[1]) choca con el dipswitch de rol del TOP → resolver (F7).

### CENTRAL — sin I²C
**No hay BNO ni ningún chip I²C en la CENTRAL** (los 2 BNO están en el TOP). El heading
absoluto del robot llega desde el TOP por `WORLD_SNAPSHOT`. *(El módulo `imu_zircon` del
firmware queda como compat: si no hay sensor, `imu_init()` cae por timeout y no se usa.)*

### DOWN — 2 OTOS en 2 buses separados
| Bus | SDA | SCL | Chip (dirección) |
|-----|-----|-----|------------------|
| **`Wire`** (I²C0) | **18** | **19** | OTOS **U5** (0x17) |
| **`Wire1`** (I²C1) | **17** | **16** | OTOS **U6** (0x17) |

- Los 2 OTOS tienen la **misma dirección fija 0x17** (no se puede cambiar) → **no pueden
  ir en el mismo bus** → van en **dos buses distintos** (`Wire` + `Wire1`). (Es lo
  opuesto a los ToF, que sí reasignan dirección y por eso comparten un bus.)
- Los 32 sensores de línea NO son I²C: son 4 muxes CD4051 con 12 pines SEL (pines 2–13) + 4 entradas ADC.

---

## 4. ¿Hay algún pin compartido problemático?

- **I²C vs UART:** en ninguna placa se pisan. TOP: I²C en 18/19 (+24/25 `Wire1`), UART en 0/1·7/8·14/15·16/17·20/21. DOWN: I²C en 18/19 + 16/17 (`Wire1`), UART en 0/1·20/21. CENTRAL: sin I²C.
- **USB vs todo:** el USB (`Serial`) es independiente — nunca compite con UART ni I²C.
- **✅ Conflicto 7/8 RESUELTO (2026-05-31):** en la **CENTRAL**, el link DOWN→CENTRAL se movió a `Serial1` (0/1), así que los pines 7/8 quedan para el motor 2 (U17). ⚠️ Ojo: el "7/8" del **TOP** es otra cosa — ahí `Serial2` (7/8) = COMM; son placas distintas, no colisionan.
- **🔧 FIX UART TOP (2026-06-02):** el Teensy 4.0 del TOP no expone `Serial7` (28/29 = back-pads). Mapeo corregido: **COMM → `Serial2` (7/8)**, **CENTRAL → `Serial4` (16/17)**. **No quedan conflictos de UART abiertos.**

---

## 5. Dónde está el detalle por placa (docs canónicos)
- **TOP:** [`top-board-pack/01-pinout-y-hardware.md`](top-board-pack/01-pinout-y-hardware.md) §2.
- **DOWN:** [`down-board-pack/01-pinout-y-posiciones.md`](down-board-pack/01-pinout-y-posiciones.md).
- **CENTRAL:** [`central-board-pack/01-pinout-y-hardware.md`](central-board-pack/01-pinout-y-hardware.md) + `src/central/config_central.h`.
- **Flujo de datos / arquitectura:** [`../../docs/ARQUITECTURA-3-PLACAS-2026.md`](../../docs/ARQUITECTURA-3-PLACAS-2026.md).
- **Protocolo de los enlaces:** `src/shared/proto.h` (START / CRC16 / SEQ / END) + `docs/firmware/CONTRATO-DATOS-*.md`.
