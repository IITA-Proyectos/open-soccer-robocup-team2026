# Bill of Materials (BOM) — Major Components
## IITA Low Battery Messi · RoboCupJunior Soccer **Open** · Incheon 2026

> ⚠️ **VERSIÓN DE TRABAJO EN ESPAÑOL** — esta BOM alimenta el **Poster** y el **TDP**, que **FINALES deben entregarse en INGLÉS** (requisito de rúbrica RCJ). **Traducir antes de enviar.** El equipo lee/mejora esta versión; GitHub traduce automáticamente para los jueces.

> 📋 **Plantilla oficial:** si RoboCupJunior publica una **plantilla oficial de BOM** para el TDP/poster Soccer 2026, **transcribir esta tabla a esa plantilla** antes de entregar (verificar en `robocup-junior.github.io/soccer-rules/master/` y en el formulario online del TDP). Esta BOM ya trae todas las columnas que la rúbrica pide para *componentes mayores*: **Componente · Part number/modelo · Cantidad · Fuente/proveedor · Nuevo vs reusado · Kit vs custom · Costo unitario · Costo total**.

---

**Por qué este documento existe (mapeo a rúbrica):** la rúbrica RCJ Soccer 2026 pide explícitamente, tanto en el **Poster → Method/Design** como en el **TDP → Electrical/Mechanical**, un **BOM de componentes mayores con selección de componentes justificada, costo y tiempo de desarrollo**. Este documento es la fuente única de esos datos; el poster y el TDP referencian a esta tabla en lugar de duplicarla. Cada sección abajo lleva un encabezado que mapea **1:1** a un criterio de rúbrica para que el juez lo encuentre de un vistazo.

---

## 0. Identificación (Poster → Title/Identification · nivel apuntado: Excellent)

| Dato | Valor |
|---|---|
| Equipo | **IITA Low Battery Messi** (org. interna: **IITA** — Instituto de Innovación y Tecnología Aplicada (Fundación Innovar)) |
| Región | **Salta, Argentina** — campeones de la final nacional de la Roboliga Argentina 2025 (organizada por la UAI) |
| Liga / Sub-liga | RoboCupJunior **Soccer — Open League** |
| Evento | RoboCup 2026, **Incheon, Corea del Sur** (30-jun a 6-jul 2026) |
| Clasificación | Campeones nacionales Roboliga Argentina (diciembre 2025 (UAI)) |
| Robots | **2** — ROBOT1 = arquero, ROBOT2 = delantero |
| Licencia / open-source | **MIT** (Copyright 2026 IITA / Fundación Innovar) — repo público: https://github.com/IITA-Proyectos/open-soccer-robocup-team2026 |

---

## 1. BOM de componentes mayores por robot (TDP → Electrical · Poster → Method/Design · nivel apuntado: Excellent)

> **Cómo leer la tabla.** Las cantidades son **por robot** salvo nota. La columna **Nuevo/Reusado** distingue lo que el equipo diseñó/compró nuevo para 2026 de lo que **reutiliza** del robot campeón nacional 2025 (ángulo de **sustentabilidad** premiado por la rúbrica). **Kit/Custom** distingue módulo comprado (COTS) de placa/pieza fabricada por el equipo. Los **costos unitarios** de los pasivos del anillo provienen de las BOM de fabricación EasyEDA/LCSC del repo (`hardware/electronics/.../BOM_*.csv`, fecha 2026-04-12/20, **citados verbatim**); el resto son **referencia internacional, un solo valor = el más alto por ítem** (cota superior conservadora, verificado por web). La columna **Costo total = costo unit. × cantidad**; cada sección cierra con su **Subtotal** y el gran total está en §3.1.

### 1.1 Cómputo / control (procesadores)

| Componente | Part number / modelo | Cant. | Fuente / proveedor | Nuevo / Reusado | Kit / Custom | Costo unit. | Costo total |
|---|---|---|---|---|---|---|---|
| MCU placa TOP | **Teensy 4.0** (LCSC `C99001332551`) — Cortex-M7 @600 MHz | 1 | PJRC / LCSC | Nuevo | Módulo (kit) sobre PCB custom | **USD 23.80** (SparkFun DEV-15583) | **USD 23.80** |
| MCU placa DOWN | **Teensy 4.0** (LCSC `C99001332551`) | 1 | PJRC / LCSC | Nuevo | Módulo (kit) sobre PCB custom | **USD 23.80** (SparkFun DEV-15583) | **USD 23.80** |
| MCU placa CENTRAL | **Teensy 4.1** (Cortex-M7 @600 MHz) | 1 | PJRC | **Reusado** (del robot Nacional 2025) | Módulo (kit) sobre PCB Zircon | **USD 31.50** (SparkFun DEV-16771) | **USD 31.50** |
| MCU placa COMM | **ESP32-C6-MINI-1-N4** (Espressif; RISC-V, WiFi 6 / BLE 5 / 802.15.4, USB nativo, flash 4 MB) | 1 | Espressif (en PCB COMM) | Nuevo | SMD en PCB custom (fork RCJ) | **USD 4.53** (DigiKey) | **USD 4.53** |
| **Subtotal §1.1** | | | | | | | **USD 83.63** |

### 1.2 Sensores de percepción (cámaras + IMU + distancia)

| Componente | Part number / modelo | Cant. | Fuente / proveedor | Nuevo / Reusado | Kit / Custom | Costo unit. | Costo total |
|---|---|---|---|---|---|---|---|
| Cámara de visión | **OpenMV Cam N6** (STM32N6 Cortex-M55 + NPU Neural-ART, sensor PAG7936, QVGA RGB565 ~30 Hz) | 2 | OpenMV | Nuevo | Módulo (kit) | **USD 165/u** (openmv.io) | **USD 330.00** |
| IMU (heading/yaw) | **Bosch BNO055** (designadores U10/U11) | 2 | Bosch (módulo) | Nuevo | Módulo (kit) en header 4P | **USD 34.95/u** (Adafruit #2472) | **USD 69.90** ⚠️ ver nota |
| Sensor ToF multizona | **ST VL53L7CX** (8×8 zonas, FoV 60°, módulo Pololu) | 4 | STMicro / Pololu | Nuevo | Módulo (kit) | **USD 19.95/u** (Pololu #3418) | **USD 79.80** |
| Ultrasonido | **HC-SR04** (designador U6 en TOP) | 1 | genérico | Nuevo | Módulo (kit) | **USD 5.25** (SparkFun) | **USD 5.25** |
| **Subtotal §1.2** | | | | | | | **USD 484.95** |

> ⚠️ **Nota BNO055:** el repo monta **2 BNO055** pero **1 unidad (RIGHT, 0x29) está FALLADA** → hoy es **2 montados, 1 sano**; el robot compite con **1 BNO sano + 4 ToF**. Para replicabilidad/repuestos: prever **2–4 unidades** (Incheon). Precio real de referencia: **USD 34.95/u** (Adafruit #2472) o 29.95 (Qwiic #4646); el viejo ~USD 15 era una nota cualitativa del repo.

### 1.3 Odometría y sensores de piso (placa DOWN)

| Componente | Part number / modelo | Cant. | Fuente / proveedor | Nuevo / Reusado | Kit / Custom | Costo unit. | Costo total |
|---|---|---|---|---|---|---|---|
| Odometría óptica | **SparkFun OTOS** (Optical Tracking Odometry Sensor; designadores U5/U6) | 2 | SparkFun | Nuevo | Módulo (kit), header | **USD 84.95/u** (SparkFun SEN-24904) | **USD 169.90** |
| Fototransistor de línea | **Everlight ALS-PT19-315C/L177/TR8** (LCSC `C146233`) | 32 | Everlight / LCSC | Nuevo | SMD en PCB custom | **USD 0.116** | **USD 3.71** |
| LED emisor 0402 (anillo) | **YLED0402O** (LCSC `C28310436`) | 32 | Yongyutai / LCSC | Nuevo | SMD en PCB custom | **USD 0.016** | **USD 0.51** |
| Multiplexor analógico 8:1 | **TI CD4051BM** (LCSC `C353976`) — 4 muxes × 8 canales = 32 | 4 | TI / LCSC | Nuevo | SMD en PCB custom | **USD 0.96** | **USD 3.84** |
| Resistencia limit. LED | 330 Ω 0603 | 32 | LCSC | Nuevo | SMD | **USD 0.01** (ref. máx.) | **USD 0.32** |
| Resistencia bias fototr. | 10 kΩ 0603 | 32 | LCSC | Nuevo | SMD | **USD 0.01** (ref. máx.) | **USD 0.32** |
| **Subtotal §1.3** | | | | | | | **USD 178.60** |

### 1.4 Actuadores (tracción omni KIWI)

| Componente | Part number / modelo | Cant. | Fuente / proveedor | Nuevo / Reusado | Kit / Custom | Costo unit. | Costo total |
|---|---|---|---|---|---|---|---|
| Motor de tracción | **Motor DC brushed tipo "TT"** (5 V común, base KIWI 3 ruedas a 120°; modelo/torque/RPM exactos **[a confirmar]**) | 3 | genérico / Pololu | **[Nuevo/Reusado?]** — 2025 usó "motores TT" | Módulo (kit) | **USD 23.95/u** (ref. máx.: Pololu 30:1 HP #1093; el TT genérico es ~USD 3) | **USD 71.85** |
| Driver de motor (H-bridge) | **3 H-bridges integrados en la PCB Zircon** (drivers U5/U7/U17; INA+INB+PWM 8-bit) | 3 | Robomov (en Zircon) | **Reusado** (parte del Zircon) | Custom (en shield Zircon) | incluido en Zircon | incluido |
| Rueda omnidireccional | **Rueda omni 38 mm** (Nexus 14184 + cubo eje 3 mm; Ø/material/rodillos exactos **[a confirmar]**) | 3 | Nexus / genérico | **[Nuevo/Reusado?]** | Módulo (kit) | **USD 6.50/u** (omni ~4.00 + cubo ~2.50) | **USD 19.50** |
| Kicker / solenoide | **NINGUNO** — el robot **no tiene kicker físico** (empuja la pelota por inercia) | 0 | — | — | — | — | **USD 0** |
| **Subtotal §1.4** | | | | | | | **USD 91.35** |

> ✅ **Decisión de diseño (sustentabilidad + simplicidad, premiable):** **sin kicker** → menos componentes, menos energía, menos puntos de falla. El delantero empuja por inercia cuando se alinea al arco rival (lógica en `src/shared/behind_ball.{h,cpp}`, 16 tests).

### 1.5 Alimentación (común a las 3 placas)

| Componente | Part number / modelo | Cant. (por robot) | Fuente / proveedor | Nuevo / Reusado | Kit / Custom | Costo unit. | Costo total |
|---|---|---|---|---|---|---|---|
| Batería | **LiPo 2S · 7.4 V nominal · 6800 mAh** (8.4 V plena / ~6.0 V corte; energía **≈ 50 Wh** = 6.8 Ah × 7.4 V; conector Deans; C-rating/marca/peso **[a confirmar]**, ref. 50C) | 1 | genérico (RC) | Nuevo | Módulo (kit) | **USD 42.99** (ref. máx.: Gens Ace 7.4 V 6800 mAh 50C 2S) | **USD 42.99** |
| Conector de batería | **Deans-T-F (XP1)** | 3 (1/placa) | genérico | Nuevo | Componente | **USD 1.00** (ref. máx.) | **USD 3.00** |
| Diodo Schottky (protec.) | **B5819W SL** (LCSC `C8598`, 1 A/40 V; OR-ing/polaridad) | 6 (2/placa) | CJ / LCSC | Nuevo | SMD en PCB custom | **USD 0.024** | **USD 0.14** |
| Regulador buck | **MP1584-EN** (módulo SIP 4-pin; rails 5 V lógica + 3.3 V sensores) | 6 (2/placa) | genérico | Nuevo | Módulo (kit) | **USD 0.90/u** (pack 6 = 5.39) | **USD 5.40** |
| LDO 3.3 V (solo COMM) | **TI UA78M33CDCYR** (SOT-223) | 1 | TI | Nuevo | SMD en PCB COMM | **USD 0.30** (ref. máx.) | **USD 0.30** |
| **Subtotal §1.5** | | | | | | | **USD 51.83** |

### 1.6 PCBs custom (fabricadas por el equipo) y placa reusada

| Componente | Part number / modelo | Cant. | Fuente / proveedor | Nuevo / Reusado | Kit / Custom | Costo unit. | Costo total |
|---|---|---|---|---|---|---|---|
| PCB **TOP** | `Roboliga2026_TOP` (224.0 × 97.5 mm, 2 capas; EasyEDA → JLCPCB) | 1 | JLCPCB (diseño IITA) | **Nuevo** | **Custom** | **USD 10.00** (ref. máx., prorrateado lote de 5) | **USD 10.00** |
| PCB **DOWN** (= plato base) | `Roboliga 2026 Futbol` REV 1.0 (~175.1 × 165.7 mm, contorno tipo plato, **ES el chasis estructural**) | 1 | JLCPCB (diseño IITA) | **Nuevo** | **Custom** | **USD 10.00** (ref. máx., prorrateado) | **USD 10.00** |
| PCB **COMM** | `PCB1` (25.40 × 31.20 mm; fork IITA del módulo oficial RCJ `soccer-communication-module`) | 1 | JLCPCB (diseño IITA) | **Nuevo** | **Custom** (fork open-source RCJ) | **USD 5.00** (ref. máx., prorrateado) | **USD 5.00** |
| PCB/Shield **CENTRAL** | **Zircon Rev v15** (PCB comercial de **Robomov**, robomov.net; esquemático `Zircon.pdf` público) | 1 | Robomov | **Reusado** (placa que ganó el Nacional 2025) | **Comprado (COTS comercial)** | **USD 250.00** (ref. máx.; Robomov no lo vende suelto, kit completo USD 529) | **USD 250.00** |
| **Subtotal §1.6** | | | | | | | **USD 275.00** |

> 🔁 **Sustentabilidad / reuso (premiable):** el **Zircon Rev v15 + Teensy 4.1** es el cerebro que **ganó el Nacional 2025**; las placas nuevas (TOP/DOWN) se **montan alrededor**, no lo reemplazan. Si una placa nueva falla en Incheon, CENTRAL **degrada a modo monolítico**. El diseño es **capitalizable post-Incheon** (reemplazar una placa sin tocar las otras; mejor cámara = solo cambia el firmware del TOP).

### 1.7 ICs específicos de la placa COMM (árbitro RCJ)

> Componentes SMD poblados en la PCB COMM (`PCB1`), verbatim de `BOM_Board1_PCB1_2026-04-20.xlsx` y del netlist (`FlyingProbeTesting.json`) reconstruido en `hardware/electronics/comm-board/2026-05-17-placa-comm-componentes-y-circuito.md`. Cantidades **por placa COMM** (1 placa COMM por robot).

| Componente | Part number / modelo | Cant. | Fuente / proveedor | Nuevo / Reusado | Kit / Custom | Costo unit. | Costo total |
|---|---|---|---|---|---|---|---|
| Level shifter UART | **TI TXS0102DCUR** (VSON8; 2 bits bidireccional, UART robot ↔ ESP 3.3 V; designador U6) | 1 | TI | Nuevo | SMD en PCB COMM | **USD 0.50** (ref. máx.) | **USD 0.50** |
| Acelerómetro 3 ejes | **ST LIS3DHTR** (LGA-16; I²C, "shake-to-start" del módulo RCJ; designador U7) | 1 | STMicro | Nuevo | SMD en PCB COMM | **USD 2.00** (ref. máx.) | **USD 2.00** |
| Pulsadores táctiles | **TS-1088-AR02016** (SW-SMD; CONNECT/PROG = botón usuario + strap boot GPIO9) | 2 | genérico | Nuevo | SMD en PCB COMM | **USD 0.10** (ref. máx.) | **USD 0.20** |
| **Subtotal §1.7** | | | | | | | **USD 2.70** |

> ⚠️ **Nota COMM:** el árbitro arranca/para por **NIVEL de tensión en OUT_1/OUT_2** (3.3 V=GO, 0 V=STOP), **NO por UART** — el firmware C6 recibe el comando por BLE de la app del árbitro y lo traduce a nivel. El UART `RX_OUT/TX_OUT` (vía TXS0102) es hardware pasivo que el firmware oficial v0.91 todavía no usa. El display OLED del módulo RCJ es **externo** (no poblado en esta placa, va por I²C en U4).

---

## 2. Justificación de selección de componentes (TDP → Electrical: "razonamiento basado en DATOS" · nivel apuntado: Excellent)

La rúbrica premia **decisiones de diseño basadas en datos y trade-offs**, no solo la lista. Resumen de las decisiones con respaldo cuantitativo (detalle completo en los docs citados):

| Decisión | Alternativas evaluadas | Dato / criterio | Elección |
|---|---|---|---|
| **Localización 2D: 4× VL53L7CX (ToF) vs LiDAR vs EKF/MCL** | LiDAR (~USD 100), array ToF (~USD 80 = 4× VL53L7CX), EKF (±0.5–1 cm/3–5 días), Particle Filter (~500 µs, "overkill para cancha 1.83×2.43 m con 4 paredes ortogonales") | ToF: **±2–3 cm**, CPU despreciable, **1 día** de dev, **~USD 80** vs **USD 100** del LiDAR | **Trilateración geométrica con 4 ToF** (`docs/lidar-tof-slam-analysis.md`, `research/.../2026-05-25-localizacion-tof-imu-analisis.md`) |
| **2× BNO055** (redundancia de heading) | 1 IMU | "dos chips de ~USD 35 c/u; confiabilidad muy superior" | 2 IMU (**2 montados, 1 sano**; ver gap) |
| **2× OTOS** (odometría óptica) | encoders en rueda | mide **slip lateral y rotación** directo del piso; banco: 300 mm reales → 280.4 mm (6.5 % error, pasa tolerancia 8 %) — ver gráfico de error por superficie en `docs/competencia/assets/fig9_otos_error.png` | 2 OTOS dual-bus |
| **MCU: Teensy 4.x (Cortex-M7 @600 MHz)** | ESP32, STM32 menores | cada MCU corre a **<30 % de CPU** *(objetivo de diseño — no medido con osciloscopio)* (margen para Kalman/EKF/coordinación) | Teensy 4.0/4.1 |
| **Sin kicker** | solenoide (2025 lo tenía) | menos componentes/energía/fallas; empuje por inercia | eliminado del firmware 2026-06-03 |
| **OpenMV N6** (vs H7 Plus) | OpenMV H7 Plus | NPU Neural-ART para visión por color; restricción HW (usar `sensor`+`pyb.UART`) documentada | 2× N6 |

> ✅ **Precio ToF RESUELTO (2026-06-05):** el VL53L7CX (8×8 zonas) cuesta **USD 19.95/u** (carrier Pololu #3418, con regulador 3.3V + level-shifters; baja a 17.96 desde 5 u). El viejo ~USD 26 era del VL53L1X de 1 zona y NO aplica. Subtotal ToF/robot = 4 × 19.95 = **USD 79.80**. La decisión de diseño (ToF vs LiDAR) sigue válida.

---

## 3. Costo y tiempo de desarrollo (Poster → Method/Design: "tiempo y costo de desarrollo" · nivel apuntado: Excellent)

### 3.1 Costo total estimado

> 🇦🇷 **Nota de costos — contexto de importación (Argentina).** Los precios de esta sección son **valores internacionales de referencia (USD)** tomados de retailers internacionales (DigiKey, Mouser, SparkFun, Pololu, openmv.io, etc.). El **costo real de adquisición en Argentina es sensiblemente mayor**: la importación está **restringida** —se puede traer un número acotado de unidades por operación (del orden de **3 por ítem por vez**)—, lo que obliga a **fraccionar las compras entre varios proveedores** y a pagar **múltiples impuestos y costos** (aranceles, IVA, percepciones, courier). Por eso publicamos el **precio internacional de referencia** como base reproducible para que otro equipo pueda estimar el suyo, y aclaramos que el costo *landed* local es superior.

> **Criterio de costeo:** un **solo valor por ítem = el más alto** de las referencias internacionales (cota superior conservadora). Suma de los subtotales de §1.1–§1.7.

| Concepto | Costo (USD, ref. internacional · valor más alto por ítem) |
|---|---|
| §1.1 Cómputo / control (Teensy 4.0 ×2 + Teensy 4.1 + ESP32-C6) | **USD 83.63** |
| §1.2 Percepción (2× N6 + 2× BNO055 + 4× VL53L7CX + HC-SR04) | **USD 484.95** |
| §1.3 Odometría / piso (2× OTOS + anillo 32 sensores + 4 muxes + diodos) | **USD 178.60** |
| §1.4 Tracción (3 motores + 3 ruedas omni; H-bridges incluidos en Zircon) | **USD 91.35** |
| §1.5 Alimentación (batería 6800 mAh + bucks + protección) | **USD 51.83** |
| §1.6 PCBs (TOP + DOWN + COMM + Zircon) | **USD 275.00** |
| §1.7 ICs específicos placa COMM | **USD 2.70** |
| **COSTO TOTAL por robot — TODO NUEVO (ref. más alta)** | **≈ USD 1.168** |
| **COSTO TOTAL por robot — reusando el CENTRAL** (Zircon $250 + Teensy 4.1 $31.50 del 2025) | **≈ USD 887** |
| **COSTO TOTAL 2 robots** — 1 reusa el CENTRAL campeón + 1 todo nuevo | **≈ USD 2.055** |
| **COSTO TOTAL 2 robots** — ambos todo nuevo | **≈ USD 2.336** |
| Conversión a moneda local (ARS) | **TC 1480 ARS = 1 USD** (2026-06-13) → equivalentes de referencia **MÍNIMA**: ≈ **ARS 1.728.640/robot** (USD 1.168 × 1480), ≈ **ARS 1.312.760** reusando CENTRAL (USD 887), ≈ **ARS 3.041.400** los 2 robots (USD 2.055) · ⚠️ el costo *landed* local real es **MAYOR** por impuestos y restricciones de importación argentinas (ver nota arriba) — el equivalente en pesos es **piso, no total** |

> 💵 Los precios son **referencia internacional (USD)**, **un solo valor por ítem = el más alto** de las referencias (cota superior), verificados por web (openmv.io, SparkFun, Pololu, Adafruit, DigiKey, banggood/Gens Ace para la batería, etc.); **desglose completo con URLs en `BOM-COSTOS-TEMPLATE.md`**. **Pendiente del equipo (chico):** precio real de la placa **Zircon** suelta (Robomov solo publica el kit completo USD 529), confirmar el **motor** real (se usó la cota alta Pololu HP $23.95; el TT genérico es ~$3), la **marca/C-rating/peso** de la batería y las **horas** de desarrollo. (El **tipo de cambio** ya está confirmado: **1480 ARS = 1 USD**, 2026-06-13 — equivalente en pesos = piso, el *landed* real es mayor por importación.) Si se confirma el motor TT (~$3) y bajan los estimados, el total/robot baja de ~$1.168 hacia ~$1.100.

### 3.2 Tiempo de desarrollo (trazable en journals)

| Hito | Fecha | Evidencia |
|---|---|---|
| Kickoff del proyecto | 2026-02-21 | `journal/2026-02-21-project-kickoff.md` |
| Arranque firmware 3 placas (Plan A) | 2026-05-10/11 → 2026-06-28 (8 hitos semanales) | `journal/2026-05-10-fase-0-y-arranque-fase-1.md` |
| 3 placas físicas existen ("robot casi completo mecánico/electrónico") | 2026-05-29 | `ESTADO-ACTUAL.md` L478-481 |
| Bring-up DOWN (32 sensores + OTOS) | 2026-05-24 | `journal/2026-05-24-*` |
| Bring-up TOP (4 ToF en bus único, BNO) | 2026-05-25 → 05-31 | `journal/2026-05-30-top-tof-4-en-bus-unico-*` |
| Árbitro homologado (mueve el robot end-to-end) | 2026-06-02/03 | `journal/2026-06-03-banco-*` |
| **Esfuerzo total de ingeniería** | **≈ 4 meses** (feb–jun 2026), desarrollo asistido por múltiples agentes en ramas | journals |

> 💡 **Métrica de proceso vendible:** **suite de tests host-native que crece de forma trazable sesión a sesión** — 246 → 262 → 324 → 354 → 403 → 470 → 545 → 658 → 834 → **858 tests / 61 suites / 0 failures (measured 2026-06-14 via `scripts/run-host-tests.sh`, g++ de Webots)**. Ver el gráfico de crecimiento en `docs/competencia/assets/fig8_test_growth.png` (generado por `gen_figuras.py`).

---

## 4. Resumen por placa (TDP → Mechanical/Electrical: trazabilidad de subsistemas · nivel apuntado: Excellent)

| Placa | MCU | Rol | Sensores/actuadores clave | Nuevo/Reusado | PCB |
|---|---|---|---|---|---|
| **TOP** | Teensy 4.0 | Percepción / "cerebro sensorial" | 2× OpenMV N6, 2× BNO055, 4× VL53L7CX, 1× HC-SR04, lee árbitro por GPIO | Nuevo | Custom `Roboliga2026_TOP` |
| **CENTRAL** | Teensy 4.1 | Decisión / FSM táctica + motores | 3× H-bridge → 3 motores omni KIWI | **Reusado** (campeón 2025) | **Zircon Rev v15 (COTS Robomov)** |
| **DOWN** | Teensy 4.0 | Piso / odometría (= **plato base**) | anillo 32 sensores línea (4× CD4051), 2× OTOS | Nuevo | Custom `Roboliga 2026 Futbol` |
| **COMM** | ESP32-C6 | Árbitro RCJ + partner | level shifter TXS0102, accel LIS3DH | Nuevo | Custom `PCB1` (fork RCJ) |

---

## 5. Open-source y replicabilidad de la BOM (Documentation & Community Contribution + TDP Bonus +1/+1 · nivel apuntado: Excellent)

La rúbrica otorga **+1 bonus por open-source de CAD/PCB/esquemáticos** y **+1 por open-source del software**, más el criterio **Documentation & Community Contribution (5 pts)**. Esta BOM habilita la **replicabilidad** (estándar de oro RCJ):

- **PCBs custom fabricables:** los proyectos **EasyEDA completos** (SCH JSON + PCB JSON + PDF + **BOM CSV** + Pick&Place + gerbers + FlyingProbeTesting) de **TOP** y **DOWN** están en `hardware/electronics/pcb_design/{top_board,down_board}/` y `gerber_file/`. Otro equipo puede **re-fabricar TOP y DOWN tal cual** (JLCPCB).
- **CENTRAL (Zircon):** placa comercial **Robomov Rev v15** con **esquemático público** (`hardware/electronics/Zircon.pdf`, robomov.net) → replicable comprándola o desde su schematic.
- **COMM:** **fork open-source** del módulo oficial RCJ (`IITA-Proyectos/rcj-soccer-open_communication_module`).
- **Software:** firmware C++/Python bajo licencia **MIT**, con **testing host-native** reproducible (`scripts/run-host-tests.sh`) y build **100 % offline**.
- **Las BOM de fabricación reales** (con LCSC part numbers y precios unitarios) están versionadas: `hardware/electronics/pcb_design/down_board/BOM_Roboliga_2026_Futbol_2026-04-12.csv`, `hardware/electronics/pcb_design/top_board/BOM_Roboliga2026_TOP_2026-04-12.csv`, `hardware/electronics/gerber_file/Placas/Comm/BOM_Board1_PCB1_2026-04-20.xlsx`.

---

## 6. Datos faltantes (Gaps) — completar antes de entregar poster/TDP

> Marcar cada uno como **dato real a conseguir del equipo**; sin esto el poster/TDP pierde puntos en *Method/Design* (costo/tiempo) y *Documentation* (replicabilidad).

| # | Gap | Tipo |
|---|---|---|
| 1 | **IITA Low Battery Messi** oficial registrado en RoboCup Incheon | Identificación |
| 2 | ✅ RESUELTO: Roboliga Argentina 2025 (final nacional, UAI) · Salta, Argentina | Identificación |
| 3 | ⚙️ **[COSTOS] valores de referencia CARGADOS** (2026-06-07, un solo valor = el más alto por ítem, §1.1–§1.7 con Costo total por fila). Pendiente: precios **reales/facturas** del equipo (vs referencia internacional) + confirmar Zircon suelto y motor real | Costo |
| 4 | ✅ **[COSTO TOTAL] CALCULADO**: ≈ **USD 1.168/robot** (todo nuevo) · ≈ **887** (reusando CENTRAL) · ≈ **2.055–2.336** los 2 robots (§3.1). **Conversión a ARS RESUELTA: TC 1480 ARS = 1 USD (2026-06-13)** → equivalente de referencia **MÍNIMA** ≈ ARS 1.728.640/robot; el *landed* real es mayor por impuestos/importación | Costo |
| 5 | **[SPEC motor]** modelo/voltaje/RPM/torque/reducción/encoder del motor TT 2026 | Mecánica |
| 6 | **[SPEC rueda]** Ø/material/n.º rodillos/origen (impresa o comprada) de la rueda omni 2026 | Mecánica |
| 7 | ⚙️ **[SPEC batería] PARCIAL**: ✅ **LiPo 2S · 7.4 V · 6800 mAh** (≈50 Wh, 1 pack/robot) cargada en §1.5. Pendiente: **C-rating, marca, peso** | Eléctrica |
| 8 | **[Nuevo/Reusado motores/ruedas]** confirmar si tracción es nueva 2026 o reusada 2025 | Sustentabilidad |
| 9 | ✅ Cifra final de tests **resuelta**: **858 tests / 61 suites / 0 failures (measured 2026-06-14 via `scripts/run-host-tests.sh`, g++ de Webots)** — figura en `docs/competencia/assets/fig8_test_growth.png` | Proceso |
| 10 | **[FOTO]** de cada PCB poblada (TOP/DOWN/Zircon/COMM) y del robot armado para etiquetar en el poster | Imágenes |
| 11 | **[GAP]** set-points reales de los MP1584 (trimpot, sin medir) y costo de PCBs prorrateado del lote JLCPCB | Eléctrica |
| 12 | **[PLANTILLA]** verificar si existe plantilla oficial de BOM RCJ y transcribir | Formato |

---

### Fuentes (citadas, originales del equipo)
- `hardware/electronics/pcb_design/down_board/BOM_Roboliga_2026_Futbol_2026-04-12.csv` (precios LCSC verbatim)
- `hardware/electronics/pcb_design/top_board/BOM_Roboliga2026_TOP_2026-04-12.csv`
- `hardware/electronics/gerber_file/Placas/Comm/BOM_Board1_PCB1_2026-04-20.xlsx` (BOM de fabricación COMM); `hardware/electronics/comm-board/2026-05-17-placa-comm-componentes-y-circuito.md` (BOM COMM en texto, 12 líneas)
- `hardware/electronics/MAPA-CONEXIONES-3-PLACAS.md`, `docs/ARQUITECTURA-3-PLACAS-2026.md` (L326: BNO055 ~USD 15)
- `docs/lidar-tof-slam-analysis.md` (USD 100 LiDAR vs USD 26 array ToF)
- `README.md` (equipo, licencia, clasificación)
