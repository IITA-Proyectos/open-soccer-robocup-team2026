---
title: "Pinout COMPLETO de la placa DOWN — extraído del schematic EasyEDA (BORRADOR a validar)"
date: 2026-05-19
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: borrador-para-validar
tags: [electronica, down-board, pinout, mux, cd4051, teensy-4.0, fuente-de-verdad, validacion-pendiente]
robot: ambos
area: electronica
tipo: referencia
fuente: SCH_Roboliga_2026_Futbol_2026-04-12.json (schematic EasyEDA)
validacion-pendiente: [Enzo (diseñador PCB), Virginia/Elías (medición con multímetro en banco)]
---

# Pinout COMPLETO de la placa DOWN (Roboliga 2026 Futbol)

> **Status: BORRADOR a validar (2026-05-19).** Este documento es la propuesta
> de fuente de verdad para el pinout de la placa DOWN. Está extraído
> automáticamente del schematic EasyEDA
> (`hardware/electronics/pcb_design/down_board/SCH_Roboliga_2026_Futbol_2026-04-12.json`)
> mediante parsing del JSON + union-find sobre las wires + cruce con el pinout
> oficial PJRC del Teensy 4.0.
>
> Reemplaza el contenido equivalente de
> `hardware/electronics/mapa-pines-placas-nuevas.md` (sección DOWN) cuya
> arquitectura "A/B/C compartidas entre los 4 muxes" resultó **incorrecta**.
>
> ⚠️ **Antes de mergear como verdad oficial**: validar con Enzo (diseñador)
> + Virginia/Elías (medición en banco). Ver §11.

## 1. Resumen ejecutivo

| Dato | Valor |
|---|---|
| Placa | **DOWN** = "Roboliga 2026 Futbol" REV 1.0 (entregada 2026-04-20) |
| MCU | **Teensy 4.0** (U7) — Cortex-M7 @ 600 MHz |
| Sensores de luz | **32 × ALS-PT19** (F1–F32) + 32 LED activos (LED1–LED32) |
| Multiplexación | **4 × CD4051BM** (U1–U4), 8 canales cada uno = 32 sensores |
| Tracking óptico | **2 × SparkFun OTOS** (U5/U6) en buses I²C separados |
| Conectores aux | U10 (UART hacia placa TOP) + U11 (UART secundario) |
| Alimentación | XP1 Dean (LiPo 7.4 V) → 2× MP1584-EN reguladores buck |
| Schematic fuente | `pcb_design/down_board/SCH_Roboliga_2026_Futbol_2026-04-12.json` |

### Hallazgo central que cambia el firmware

**La arquitectura NO es "A/B/C compartidas entre los 4 muxes"** (como decía la
documentación anterior). El schematic muestra que **cada CD4051 tiene sus
propios 3 selectores conectados a 3 pines distintos del Teensy** — total
**12 pines de selección + 4 outputs analógicos = 16 pines** para el anillo.
Ver §5.

## 2. Niveles de confianza

| Marca | Significado |
|---|---|
| ✅ **CONFIRMADO** | Extraído directo del schematic + cruzado con pinout PJRC oficial Teensy 4.0. Alta certeza. |
| ⚠️ **PROBABLE** | Inferido por convención (nombre del pin coincide con pinout estándar). Requiere validación física con multímetro o confirmación de Enzo. |
| ❓ **PENDIENTE** | No extraíble del schematic — requiere confirmación adicional (medición o pregunta a Enzo). |

## 3. Pinout COMPLETO del Teensy 4.0 (U7) — tabla maestra

Numeración del header físico del Teensy 4.0 (cara superior, 34 pines en U
shape). El "Pin Arduino" es el número usado en código `digitalWrite(N)` /
`analogRead(AN)` / `pinMode(N, ...)` — viene del pinout oficial PJRC
(<https://www.pjrc.com/teensy/pinout.html>).

| Pin header | Label schematic | Pin Arduino | Net del schematic | Función en este PCB | Confianza |
|---|---|---|---|---|---|
| 1 | VIN | — | +5V | Entrada alimentación 5 V | ✅ |
| 2 | GND | — | GND | Masa | ✅ |
| 3 | 3.3V | — | (sin net) | 3V3 salida del regulador interno (no usado) | ✅ |
| 4 | A9 | **23** | **O4** | ADC entrada — output del mux U4 (sensores S25–S32) | ✅ |
| 5 | A8 | **22** | **O3** | ADC entrada — output del mux U3 (sensores S17–S24) | ✅ |
| 6 | A7 | **21** | **RX5** | UART RX hacia placa TOP (U10) | ✅ |
| 7 | A6 | **20** | **TX5** | UART TX hacia placa TOP (U10) | ✅ |
| 8 | A5 | **19** | **SCL1** | I²C SCL del OTOS U5 (= `Wire` en Arduino) | ✅ |
| 9 | A4 | **18** | **SDA1** | I²C SDA del OTOS U5 (= `Wire` en Arduino) | ✅ |
| 10 | A3 | **17** | **SDA2** | I²C SDA del OTOS U6 (= `Wire1` en Arduino) | ✅ |
| 11 | A2 | **16** | **SCL2** | I²C SCL del OTOS U6 (= `Wire1` en Arduino) | ✅ |
| 12 | A1 | **15** | **O2** | ADC entrada — output del mux U2 (sensores S9–S16) | ✅ |
| 13 | A0 | **14** | **O1** | ADC entrada — output del mux U1 (sensores S1–S8) | ✅ |
| 14 | SCK | **13** | **E1** | Selector A del mux U1 | ✅ |
| 15 | on/Off | — | (sin net) | Control on/off (no usado por firmware) | ✅ |
| 16 | Program | — | (sin net) | Pin de programación del Teensy | ✅ |
| 17 | GND | — | (sin net) | Masa | ✅ |
| 18 | 3.3V | — | (sin net) | 3V3 (no usado) | ✅ |
| 19 | VBat | — | (sin net) | RTC battery (no usado) | ✅ |
| 20 | MISO | **12** | **E12** | Selector C del mux U4 | ✅ |
| 21 | MOSI | **11** | **E11** | Selector B del mux U4 | ✅ |
| 22 | CS | **10** | **E10** | Selector A del mux U4 | ✅ |
| 23 | OUT1C | **9** | **E9** | Selector C del mux U3 | ✅ |
| 24 | TX2 | **8** | **E8** | Selector B del mux U3 | ✅ |
| 25 | RX2 | **7** | **E7** | Selector A del mux U3 | ✅ |
| 26 | OUT1D | **6** | **E6** | Selector C del mux U2 | ✅ |
| 27 | IN2 | **5** | **E5** | Selector B del mux U2 | ✅ |
| 28 | BCLK2 | **4** | **E4** | Selector A del mux U2 | ✅ |
| 29 | LRCLK2 | **3** | **E3** | Selector C del mux U1 | ✅ |
| 30 | OUT2 | **2** | **E2** | Selector B del mux U1 | ✅ |
| 31 | TX1 | **1** | **TX1** | UART TX al conector U11 + señal E1 (uso a confirmar) | ⚠️ |
| 32 | RX1 | **0** | **RX1** | UART RX al conector U11 | ⚠️ |
| 33 | GND | — | GND | Masa | ✅ |
| 34 | NC | — | (sin conexión) | No conectado | ✅ |

### Resumen de pines Arduino usados (los que importan al firmware)

| Función | Pines Arduino |
|---|---|
| Selectores de muxes (12) | 13, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 |
| Outputs analógicos de muxes (4) | A0 (14), A1 (15), A8 (22), A9 (23) |
| I²C OTOS #1 (Wire) | SDA=18, SCL=19 |
| I²C OTOS #2 (Wire1) | SDA=17, SCL=16 |
| UART hacia TOP (Serial5) | RX=21, TX=20 |
| UART aux U11 (Serial1) | RX=0, TX=1 |

**Pines Arduino LIBRES en la placa DOWN (no usados):** del 1 al 13 ya están todos usados (selectores + UART aux + A0/A1 vía pin 13 *espera, A0=14, A1=15 son otros pines*). Reviso: pines 0–12 todos asignados (12 selectores + Serial1). Pines 13 (SCK) + 14 (A0) + 15 (A1) + 16/17 (Wire1) + 18/19 (Wire) + 20/21 (Serial5) + 22 (A8) + 23 (A9) = 24 pines en uso. Quedan **libres**: pin 24, 25, 26, 27, 28, 29, 30, 31, 32, 33 (10 pines digitales para expansión futura).

## 4. Los 4 CD4051BM (U1, U2, U3, U4) — pinout y conexión

Pinout del CD4051BM (datasheet TI):

| Pin chip | Función | Notas |
|---|---|---|
| 1 | CH4 (X4) | Canal 4 |
| 2 | CH6 (X6) | Canal 6 |
| 3 | COM | Salida común (al ADC del Teensy) |
| 4 | CH7 (X7) | Canal 7 |
| 5 | CH5 (X5) | Canal 5 |
| 6 | INH | Enable activo bajo (LOW = mux habilitado) |
| 7 | VEE | -5 V (acá a GND) |
| 8 | VSS | GND |
| 9 | C | Selector bit C (MSB) |
| 10 | B | Selector bit B |
| 11 | A | Selector bit A (LSB) |
| 12 | CH3 (X3) | Canal 3 |
| 13 | CH0 (X0) | Canal 0 |
| 14 | CH1 (X1) | Canal 1 |
| 15 | CH2 (X2) | Canal 2 |
| 16 | VDD | +3.3 V |

### Conexión de los 4 muxes al Teensy

| Mux | Selector A → Teensy | Selector B → Teensy | Selector C → Teensy | COM → Teensy ADC | INH | VDD | Sensores cubiertos |
|---|---|---|---|---|---|---|---|
| **U1** | pin 13 (E1, SCK) | pin 2 (E2, OUT2) | pin 3 (E3, LRCLK2) | A0 / pin 14 (O1) | GND (siempre habilitado) | +3.3 V | **S1 – S8** |
| **U2** | pin 4 (E4, BCLK2) | pin 5 (E5, IN2) | pin 6 (E6, OUT1D) | A1 / pin 15 (O2) | GND | +3.3 V | **S9 – S16** |
| **U3** | pin 7 (E7, RX2) | pin 8 (E8, TX2) | pin 9 (E9, OUT1C) | A8 / pin 22 (O3) | GND | +3.3 V | **S17 – S24** |
| **U4** | pin 10 (E10, CS) | pin 11 (E11, MOSI) | pin 12 (E12, MISO) | A9 / pin 23 (O4) | GND | +3.3 V | **S25 – S32** |

✅ Todo confirmado por el schematic (extracción automática + verificación PJRC).

> **Implicancia importante para el firmware**: como **INH de cada mux está atado
> a GND directo**, el mux está siempre habilitado. El firmware **NO necesita
> controlar los INH** (eliminar `PIN_MUX_INH[]` de `config_down.h`).

## 5. Mapeo de los 32 sensores físicos (S1–S32) a (mux, canal)

Para leer el sensor Sn desde el firmware, hay que seleccionar el canal correcto
en el mux correspondiente. **El orden de canal NO es secuencial** — Enzo aplicó
un "scrambling" consistente entre los 4 muxes (probablemente por routing del PCB).

### LUT para el firmware

```cpp
// Para cada sensor S(i+1) (donde i = 0..31), qué mux y qué canal seleccionar:
//   mux_idx = i / 8;                  // 0..3
//   channel = CH_LUT[i % 8];          // 0..7
static const uint8_t CH_LUT[8] = { 3, 0, 1, 2, 5, 7, 6, 4 };
// Lectura: digitalWrite(PIN_MUX_A[mux_idx], (channel & 1) ? HIGH : LOW); …
//          delayMicroseconds(5);
//          raw = analogRead(PIN_MUX_OUT[mux_idx]);
```

### Tabla completa (los 32)

| Sensor | Mux | Canal CD4051 | Pin chip CD4051 |
|---|---|---|---|
| **S1** | U1 | 3 | 12 (CH3) |
| **S2** | U1 | 0 | 13 (CH0) |
| **S3** | U1 | 1 | 14 (CH1) |
| **S4** | U1 | 2 | 15 (CH2) |
| **S5** | U1 | 5 | 5 (CH5) |
| **S6** | U1 | 7 | 4 (CH7) |
| **S7** | U1 | 6 | 2 (CH6) |
| **S8** | U1 | 4 | 1 (CH4) |
| **S9** | U2 | 3 | 12 |
| **S10** | U2 | 0 | 13 |
| **S11** | U2 | 1 | 14 |
| **S12** | U2 | 2 | 15 |
| **S13** | U2 | 5 | 5 |
| **S14** | U2 | 7 | 4 |
| **S15** | U2 | 6 | 2 |
| **S16** | U2 | 4 | 1 |
| **S17** | U3 | 3 | 12 |
| **S18** | U3 | 0 | 13 |
| **S19** | U3 | 1 | 14 |
| **S20** | U3 | 2 | 15 |
| **S21** | U3 | 5 | 5 |
| **S22** | U3 | 7 | 4 |
| **S23** | U3 | 6 | 2 |
| **S24** | U3 | 4 | 1 |
| **S25** | U4 | 3 | 12 |
| **S26** | U4 | 0 | 13 |
| **S27** | U4 | 1 | 14 |
| **S28** | U4 | 2 | 15 |
| **S29** | U4 | 5 | 5 |
| **S30** | U4 | 7 | 4 |
| **S31** | U4 | 6 | 2 |
| **S32** | U4 | 4 | 1 |

✅ Mapeo extraído del schematic. El **patrón de scrambling es idéntico para los
4 muxes** — buena señal de consistencia interna del PCB.

> 📌 **Nota terminológica importante**: en el SCH los nets entre fototransistor
> y mux se llaman `S1..S32`, pero los **designators de los fototransistores**
> (lo que está silkscreeneado en el PCB) son `F1..F32`. La correspondencia es
> **1:1 perfecta**: `F1↔S1`, `F2↔S2`, ..., `F32↔S32` (verificado parsando los
> pins de cada ALSPT19 contra el net que les llega). En el código y este doc
> usamos `S` como número lógico del sensor y `F` cuando hay que referenciar el
> componente físico en el PCB.

## 5b. Posiciones FÍSICAS de los 32 sensores en el PCB

Extraídas del PCB layout JSON (componente `LIB~` con designator `F1..F32`).
Convertidas a milímetros con la escala EasyEDA (1 unidad = 10 mil = 0.254 mm)
y trasladadas al **centro del bounding box del PCB** como origen.

### Sistema de referencia

- **(0, 0) = centro del PCB DOWN** (bbox center, x=929.6mm y=751.21mm en coords
  EasyEDA crudas — irrelevante para uso firmware).
- **+X = derecha del PCB** según viewport de EasyEDA (silkscreen TOP visto desde
  el componente side).
- **+Y = arriba del PCB** según viewport de EasyEDA (Y flipeada respecto a la
  convención EasyEDA donde Y crece hacia abajo en pantalla).
- **Asunción de montaje** (a validar con Enzo): el PCB DOWN se monta centrado
  bajo el robot ⇒ centro del PCB = centro del robot. Si el montaje no es
  centrado, hay que aplicar un offset (típicamente pocos mm).
- **Asunción de orientación** (consistente con simetría observada, a validar):
  **+Y del PCB = adelante del robot**, **+X = derecha**. Razón: F1–F8
  (anillo externo frontal, 8 sensores en arco simétrico) están todos en
  Y≈+75…+82 mm con X de –36 a +36 mm — patrón típico de "anillo frontal denso"
  que mira hacia adelante. Si Enzo confirma que el "logo IITA" del silkscreen
  TOP queda mirando hacia adelante del robot, esta asunción se valida.

### Tabla completa: S1–S32 con (mux, canal, pin Arduino) + posición XY

| Sensor | F# | Mux | Canal | COM_Arduino | X (mm) | Y (mm) | R (mm) | θ (deg) |
|--------|----|-----|-------|-------------|--------|--------|--------|---------|
| S1  | F1  | U1 | 3 | A0 | −36.28 | +82.04 | 89.70 | +113.9 |
| S2  | F2  | U1 | 0 | A0 | −30.57 | +75.06 | 81.05 | +112.2 |
| S3  | F3  | U1 | 1 | A0 | −20.28 | +74.17 | 76.89 | +105.3 |
| S4  | F4  | U1 | 2 | A0 | −10.12 | +74.17 | 74.86 |  +97.8 |
| S5  | F5  | U1 | 5 | A0 | +10.20 | +74.17 | 74.87 |  +82.2 |
| S6  | F6  | U1 | 7 | A0 | +20.36 | +74.17 | 76.91 |  +74.7 |
| S7  | F7  | U1 | 6 | A0 | +30.52 | +75.18 | 81.14 |  +67.9 |
| S8  | F8  | U1 | 4 | A0 | +36.49 | +82.04 | 89.79 |  +66.0 |
| S9  | F9  | U2 | 3 | A1 | −86.32 | +12.19 | 87.18 | +172.0 |
| S10 | F10 | U2 | 0 | A1 | −86.32 |  −0.51 | 86.32 | −179.7 |
| S11 | F11 | U2 | 1 | A1 | −84.92 | −13.21 | 85.94 | −171.2 |
| S12 | F12 | U2 | 2 | A1 | −81.24 | −26.04 | 85.31 | −162.2 |
| S13 | F13 | U2 | 5 | A1 | −67.65 | −50.04 | 84.15 | −143.5 |
| S14 | F14 | U2 | 7 | A1 | −60.03 | −60.20 | 85.02 | −134.9 |
| S15 | F15 | U2 | 6 | A1 | −48.98 | −69.09 | 84.69 | −125.3 |
| S16 | F16 | U2 | 4 | A1 | −36.28 | −76.71 | 84.86 | −115.3 |
| S17 | F17 | U3 | 3 | A8 | +36.36 | −76.71 | 84.89 |  −64.6 |
| S18 | F18 | U3 | 0 | A8 | +49.31 | −69.09 | 84.88 |  −54.5 |
| S19 | F19 | U3 | 1 | A8 | +60.87 | −60.20 | 85.61 |  −44.7 |
| S20 | F20 | U3 | 2 | A8 | +69.63 | −50.04 | 85.75 |  −35.7 |
| S21 | F21 | U3 | 5 | A8 | +82.21 | −25.91 | 86.20 |  −17.5 |
| S22 | F22 | U3 | 7 | A8 | +85.64 | −13.21 | 86.65 |   −8.8 |
| S23 | F23 | U3 | 6 | A8 | +87.29 |  −0.51 | 87.29 |   −0.3 |
| S24 | F24 | U3 | 4 | A8 | +86.40 | +12.06 | 87.24 |   +7.9 |
| S25 | F25 | U4 | 3 | A9 | −22.82 | +50.93 | 55.81 | +114.1 |
| S26 | F26 | U4 | 0 | A9 | −12.66 | +50.93 | 52.48 | +104.0 |
| S27 | F27 | U4 | 1 | A9 | +12.74 | +51.05 | 52.62 |  +76.0 |
| S28 | F28 | U4 | 2 | A9 | +22.90 | +50.93 | 55.84 |  +65.8 |
| S29 | F29 | U4 | 5 | A9 | +34.55 |  −9.51 | 35.83 |  −15.4 |
| S30 | F30 | U4 | 7 | A9 | +34.55 | −19.67 | 39.76 |  −29.7 |
| S31 | F31 | U4 | 6 | A9 | −33.25 |  −9.51 | 34.58 | −164.0 |
| S32 | F32 | U4 | 4 | A9 | −33.25 | −19.80 | 38.70 | −149.2 |

Convención angular: θ = atan2(Y, X) en grados. 0° = +X (derecha), 90° = +Y
(adelante), 180°/−180° = −X (izquierda), −90° = −Y (atrás).

### Diagrama ASCII del layout (vista desde arriba del PCB)

```
                         FRENTE del robot (+Y)
                                  ↑
                 F1 F2 F3 F4   F5 F6 F7 F8        ← anillo externo frontal (R≈80mm)
                          F25 F26 F27 F28          ← anillo interno frontal  (R≈55mm)
        F9                                                F24
        F10                                               F23
        F11        F31           F29                      F22         ← muy externos
        F12          F32         F30                      F21              (R≈85mm)
        F13                                               F20
        F14                                               F19
        F15                                               F18
   ← F16                                                  F17 →
                                  ↓
                          ATRÁS del robot (−Y)

   IZQ del robot (−X) ←                              → DER del robot (+X)
```

(Las posiciones son aproximadas — la tabla numérica de arriba es la fuente.)

### Agrupación por mux y por anillo

| Anillo | Sensores | Mux | Radio promedio | Posición |
|---|---|---|---|---|
| **Externo frontal** (8) | S1–S8 | U1 | ≈80 mm | Y ≈ +75…+82, X ∈ [−36, +36] |
| **Externo izquierdo + tras-izq** (8) | S9–S16 | U2 | ≈85 mm | X ∈ [−86, −36], Y ∈ [+12, −77] |
| **Externo derecho + tras-der** (8) | S17–S24 | U3 | ≈86 mm | X ∈ [+36, +87], Y ∈ [−77, +12] |
| **Interno frontal** (4) | S25–S28 | U4 ch 3,0,1,2 | ≈54 mm | Y ≈ +51, X ∈ [−23, +23] |
| **Interno trasero** (4) | S29–S32 | U4 ch 5,7,6,4 | ≈37 mm | Y ∈ [−10, −20], X ≈ ±34 |

Observaciones útiles para el algoritmo de detección de línea:
- El anillo externo (S1–S24, 24 sensores) cubre TODO el perímetro a R ≈ 80–87 mm.
- Pero hay un **gap atrás central**: ningún sensor a 270° (atrás). Los más
  traseros son F16 (X=−36, Y=−77) y F17 (X=+36, Y=−77), separados ≈72 mm en X.
- El anillo interno (8 sensores, S25–S32) está sólo en frente (Y > +50) y centro
  (Y ≈ −15). **No hay anillo interno lateral** — el lateral solo está cubierto
  por R≈85 mm.

### LUT inversa para firmware (sensor → posición)

```cpp
// Para algoritmos de geometria de linea (centroide, dirección): convertir
// el índice del sensor (0..31) a posición (x, y) en mm desde el centro del robot.
struct SensorPos { float x_mm; float y_mm; };

static const SensorPos SENSOR_POS[32] = {
    {-36.28f, +82.04f}, {-30.57f, +75.06f}, {-20.28f, +74.17f}, {-10.12f, +74.17f},  // S1-S4
    {+10.20f, +74.17f}, {+20.36f, +74.17f}, {+30.52f, +75.18f}, {+36.49f, +82.04f},  // S5-S8
    {-86.32f, +12.19f}, {-86.32f,  -0.51f}, {-84.92f, -13.21f}, {-81.24f, -26.04f},  // S9-S12
    {-67.65f, -50.04f}, {-60.03f, -60.20f}, {-48.98f, -69.09f}, {-36.28f, -76.71f},  // S13-S16
    {+36.36f, -76.71f}, {+49.31f, -69.09f}, {+60.87f, -60.20f}, {+69.63f, -50.04f},  // S17-S20
    {+82.21f, -25.91f}, {+85.64f, -13.21f}, {+87.29f,  -0.51f}, {+86.40f, +12.06f},  // S21-S24
    {-22.82f, +50.93f}, {-12.66f, +50.93f}, {+12.74f, +51.05f}, {+22.90f, +50.93f},  // S25-S28
    {+34.55f,  -9.51f}, {+34.55f, -19.67f}, {-33.25f,  -9.51f}, {-33.25f, -19.80f},  // S29-S32
};
// Indexar como SENSOR_POS[sensor_idx_0_based] donde 0 = S1, 31 = S32.
```

## 6. SparkFun OTOS (U5, U6) — tracking óptico

| OTOS | Bus I²C | SDA Teensy | SCL Teensy | Arduino API | Dirección I²C |
|---|---|---|---|---|---|
| **U5** | I²C #1 | pin 18 (A4) | pin 19 (A5) | `Wire` | `0x17` (default SparkFun) ⚠️ |
| **U6** | I²C #2 | pin 17 (A3) | pin 16 (A2) | `Wire1` | `0x17` ⚠️ |

**Pines del header del OTOS** (4 pines del módulo SparkFun: GND, +3V3, SDA, SCL — los pines IO9/IO10 del OTOS están en el header pero **no se usan** según el schematic).

> **Nota Arduino**: el schematic los llama "SDA1/SCL1" y "SDA2/SCL2", pero en
> Arduino Teensy 4.0 esos pines corresponden a `Wire` (I²C0) y `Wire1` (I²C1)
> respectivamente. Atención con la confusión de nombres (NO es Wire1/Wire2).

❓ **Pendiente confirmar**: ¿están **ambos OTOS poblados** físicamente, o U6 es
spare? Si solo U5 está soldado → `DOWN_NUM_OTOS_CONNECTED=1`. Si los 2 →
`=2`. Verificable visualmente sobre la placa.

## 7. UARTs

| Serial | Pines Teensy | Conector | Propósito |
|---|---|---|---|
| **Serial5** | RX5=21, TX5=20 | U10 ("COMUNICATION") | UART hacia placa TOP (envía LineStatusV2 + odometría OTOS) ✅ |
| **Serial1** | RX1=0, TX1=1 | U11 (4 pines) + señal E1 | ❓ propósito a confirmar — ¿debug? ¿otra placa? |

Baud rate típico para placas Teensy: **230400** (config actual). ✅

## 8. Alimentación

| Componente | Función | Notas |
|---|---|---|
| **XP1** (Dean-T-F) | Entrada batería LiPo | 7.4 V nominal (2S) |
| **D1, D2** (B5819W Schottky) | Protección de polaridad / OR-ing | 1 A / 40 V |
| **U8, U9** (MP1584-EN buck) | 2 reguladores switching | ❓ Set-points a medir — probablemente uno entrega 5 V para alimentación digital, el otro 3.3 V para sensores. **Trimpot físico** — verificar tensión con multímetro antes del primer power-on completo. |
| **C1–C6** (100 nF) | Desacople | 6 capacitores cerámicos |
| **+3.3V net** | Alimenta CD4051 (VDD pin 16), OTOS (+3V3) | Confirmado por schematic ✅ |
| **+5V net** | Alimenta Teensy VIN (pin 1 del header) | Confirmado ✅ |

## 9. Conector de control aux U11

El schematic muestra U11 (header 4 pines) conectado a Serial1 (RX1/TX1) +
una señal extra "E1". ❓ Propósito no documentado. Posibles usos:
- Debug serial.
- UART hacia otra placa (¿COMM ESP32-C6? ¿Display externo?).
- Provisión para módulo futuro.

Pendiente preguntar a Enzo.

## 10. Cambios necesarios en el firmware

Comparación con `src/down/config_down.h` y `src/down/line_ring.cpp` actuales:

### `config_down.h` — reescribir

```cpp
// === Pinout Teensy 4.0 ↔ CD4051 (CONFIRMADO desde schematic 2026-05-19) ===

// Selector A de cada mux (4 pines, NO compartidos)
constexpr int PIN_MUX_A[4] = { 13, 4, 7, 10 };
// Selector B de cada mux
constexpr int PIN_MUX_B[4] = { 2, 5, 8, 11 };
// Selector C de cada mux
constexpr int PIN_MUX_C[4] = { 3, 6, 9, 12 };
// Output COM de cada mux (ADC del Teensy)
constexpr int PIN_MUX_OUT[4] = { A0, A1, A8, A9 };

// INH: NO se controla por firmware — todos los INH están atados a GND
// (siempre habilitado). Eliminar PIN_MUX_INH[] del config viejo.

// Mapeo channel del mux → sensor lógico (orden de scrambling de Enzo)
constexpr uint8_t MUX_CH_FOR_SENSOR[8] = { 3, 0, 1, 2, 5, 7, 6, 4 };
```

### `line_ring.cpp` — reescribir `sample_all_sensors_hardware()`

```cpp
void sample_all_sensors_hardware() {
    for (uint8_t i = 0; i < 8; ++i) {              // sensor i dentro de cada mux
        const uint8_t ch = MUX_CH_FOR_SENSOR[i];   // canal real del mux
        for (int m = 0; m < 4; ++m) {              // setear los 12 selectores
            digitalWrite(PIN_MUX_A[m], (ch & 0x01) ? HIGH : LOW);
            digitalWrite(PIN_MUX_B[m], (ch & 0x02) ? HIGH : LOW);
            digitalWrite(PIN_MUX_C[m], (ch & 0x04) ? HIGH : LOW);
        }
        delayMicroseconds(5);                       // settle time CD4051
        for (int m = 0; m < 4; ++m) {
            g_raw[m * 8 + i] = analogRead(PIN_MUX_OUT[m]);  // S(m*8 + i + 1)
        }
    }
}
```

### `line_ring_init()` — actualizar `pinMode`

```cpp
for (int m = 0; m < 4; ++m) {
    pinMode(PIN_MUX_A[m], OUTPUT);
    pinMode(PIN_MUX_B[m], OUTPUT);
    pinMode(PIN_MUX_C[m], OUTPUT);
}
// pinMode de los INH: eliminado (no se controlan).
analogReadResolution(10);
```

## 11. Pendiente validar (con Enzo, Virginia, Elías)

**P0 — bloquea actualizar el firmware con confianza:**

1. **Enzo**: confirmar que la extracción del schematic JSON refleja el
   PCB realmente fabricado (no hubo cambios post-Apr-12).
2. **Enzo o Virginia/Elías con multímetro**: verificar continuidad de 2–3
   nets representativas (ej. pin 13 del Teensy ↔ pin 11 del CD4051 U1; pin
   A0 del Teensy ↔ pin 3 del CD4051 U1).
3. **Virginia/Elías**: confirmar que **ambos OTOS están poblados** (U5 y U6).
4. **Enzo o quien monte el PCB en el chasis**: confirmar la **orientación
   física** del PCB DOWN respecto al robot. La asunción del doc es **+Y del
   PCB (lado del logo IITA del silkscreen) = adelante del robot**, **+X =
   derecha**. La asunción se desprende de la simetría de F1–F8 + F25–F28
   (todos en Y+ alto, X simétrico → "anillo frontal denso"), pero el montaje
   final puede haber rotado la placa. Si rotada 90° o 180°, hay que rotar la
   LUT `SENSOR_POS[]` en consecuencia (multiplicar por una matriz de rotación
   trivial).

**P1 — útiles para interpretar datos:**

5. **Enzo**: confirmar para qué se usa el conector U11 (Serial1 + E1).
6. **Enzo**: confirmar que el centro del PCB ≈ centro del robot (offset de
   montaje < ~5 mm). Si no, anotar el offset (dx, dy) en mm para sumarlo a
   `SENSOR_POS[]` en firmware.

**P2 — útil pero no bloqueante:**

7. **Virginia/Elías con multímetro**: medir voltajes de salida de U8/U9
   (los 2 reguladores buck) — confirmar 5 V y 3.3 V según expectativa.

## 12. Plan de validación rápida en banco (15 minutos)

Una vez actualizado el firmware con el pinout de este doc:

1. **Recompilar** `pio run -e diag_down` (4 muxes ya activos por flag del env).
2. **Reflashear** `pio run -e diag_down -t upload`.
3. **Correr el script de captura masiva** (`scripts/diag_capture.py`) con las
   3 lecturas (mesa / blanco / negro) tapando los 32 sensores.
4. **Veredicto esperado**: `32 OK, 0 sospechosos, 0 muertos` (Enzo dijo que
   los 32 andan físicamente).
5. Si el veredicto da problemas → revisar específicamente las nets/pines del
   sensor problemático contra esta tabla.

## 13. Reproducibilidad — cómo se generó este doc

- **Fuentes** (dos JSONs de EasyEDA, ambos del proyecto de Enzo de Apr-12):
  - Schematic: `hardware/electronics/pcb_design/down_board/SCH_Roboliga_2026_Futbol_2026-04-12.json` (290 KB) — usado para todo §3–§10 (pinout, mux wiring, nets).
  - PCB Layout: `hardware/electronics/pcb_design/down_board/PCB_PCB_Roboliga_2026_Futbol_2026-04-12.json` (1.6 MB) — usado para §5b (posiciones físicas de F1–F32, BBox, conversión EasyEDA→mm).
- **Método (SCH)**: parser Python + union-find sobre los wires (`W~`) y
  junctions (`J~`) para reconstruir conectividad eléctrica entre pines de
  componentes (`LIB~...P~`) y netlabels (`F~`).
- **Método (PCB)**: parser Python que itera `LIB~x~y~...` y extrae el designator
  desde el sub-shape `TEXT~P~...` (campo P = package designator visible). Las
  coords se convierten con la escala EasyEDA `1 unidad = 10 mil = 0.254 mm` y se
  trasladan al centro del BBox.
- **Cruce SCH↔PCB**: las nets `S1..S32` del SCH se mappean 1:1 a los designators
  `F1..F32` del PCB (verificado pin por pin con union-find sobre el SCH:
  `F1.pin2 → S1`, ..., `F32.pin2 → S32`).
- **Cruce con pinout Teensy**: pinout oficial PJRC Teensy 4.0
  (<https://www.pjrc.com/teensy/pinout.html>).
- **Script reusable**: `software/teensy/Soccer 2026/scripts/extract_pinout_from_schematic.py`
  (acepta `--json` y `--pcb-json`, default a los paths de arriba). Re-ejecutarlo
  regenera la tabla unificada si el PCB cambia.
- **Sesión**: Claude Code Opus 4.7 (1M context), 2026-05-19, requested by Gustavo Viollaz.

---

> **Próximo paso**: validar puntos §11 con Enzo. Una vez confirmado:
> 1. Actualizar `src/down/config_down.h` con el código de §10.
> 2. Reescribir `src/down/line_ring.cpp` (`sample_all_sensors_hardware`).
> 3. Marcar este documento como `status: confirmado`.
> 4. Promover este doc a **fuente de verdad canónica** en
>    `docs/FUENTES-DE-VERDAD.md` para el tema "Pinout DOWN".
> 5. Marcar `hardware/electronics/mapa-pines-placas-nuevas.md` sección DOWN
>    como superada (banner apuntando acá).
> 6. Cerrar **TASK-026**.
