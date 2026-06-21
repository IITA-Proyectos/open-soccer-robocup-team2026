# Planilla de Costos — BOM (plantilla de trabajo)
## IITA Low Battery Messi · RoboCupJunior Soccer **Open** · Incheon 2026

> 📝 **Planilla de costos del BOM.** Los precios son **valores internacionales de referencia (USD)** tomados de retailers internacionales (DigiKey, Mouser, SparkFun, Pololu, openmv.io, etc.), **no** facturas locales.
>
> 🇦🇷 **Por qué de referencia:** en Argentina la **importación está restringida** (se traen pocas unidades por operación, del orden de **3 por ítem por vez** → hay que **fraccionar las compras entre varios proveedores**) y se pagan **múltiples impuestos y costos** (aranceles, IVA, percepciones, courier). Por eso el costo *landed* local es **mayor**; publicamos el **precio internacional de referencia** como base reproducible para otro equipo.

> **Cómo usar.** Cantidades **por robot** (salvo nota). Cada celda de precio lleva el **valor internacional de referencia (USD)** con su **fuente/URL**. Total/robot = suma de subtotales; Total 2 robots ajustando los ítems reusados/compartidos. Volcar el resultado a **BOM.md §3.1 / POSTER Zona E / TDP** (items A4/A5/A11 de MEJORAS-PENDIENTES).

---

## Tabla de costos por robot

> 💵 **Precios = referencia internacional (USD), verificados por web el 2026-06-05** en las tiendas oficiales/distribuidores citados. URLs completas en BOM.md §3. El costo *landed* en Argentina es mayor (ver nota de importación arriba).

| # | Componente | Part/Modelo | Cant/robot | Precio unit. USD (ref. int'l) | Subtotal/robot (USD) | Nuevo/Reusado | Fuente |
|---|---|---|---|---|---|---|---|
| 1 | Cámara de visión | OpenMV Cam N6 (STM32N6 + NPU) | 2 | **165** | **330.00** | Nuevo | openmv.io |
| 2 | Odometría óptica | SparkFun OTOS PAA5160E1 (SEN-24904) | 2 | **84.95** | **169.90** | Nuevo | SparkFun |
| 3 | MCU placa TOP | Teensy 4.0 (DEV-15583) | 1 | **23.80** | 23.80 | Nuevo | SparkFun/DigiKey |
| 4 | MCU placa DOWN | Teensy 4.0 (DEV-15583) | 1 | **23.80** | 23.80 | Nuevo | SparkFun/DigiKey |
| 5 | MCU placa CENTRAL | Teensy 4.1 (DEV-16771) | 1 | **31.50** | 31.50 | Reusado 2025 | SparkFun/DigiKey |
| 6 | PCB/Shield CENTRAL | Zircon Rev v15 (Robomov) | 1 | **250** (ref. máx.; kit completo 529) | **250.00** | Reusado 2025 | robomov.net |
| 7 | Sensor ToF multizona | ST VL53L7CX 8×8 (carrier Pololu #3418) | 4 | **19.95** | **79.80** | Nuevo | Pololu |
| 8 | Batería | LiPo 2S 7.4 V **6800 mAh** (≈50 Wh; 1 pack/robot) | 1 | **42.99** | **42.99** | Nuevo | banggood (Gens Ace 50C) |
| 9 | Motor de tracción | Motor DC brushed "TT" 5 V (ref. máx. Pololu 30:1 HP #1093) | 3 | **23.95** (TT genérico ~3) | **71.85** | [confirmar] | Pololu / AliExpress |
| 10 | Rueda omnidireccional | Omni 38 mm (Nexus 14184) + cubo eje 3 mm | 3 | **6.50** (omni 4.00 + cubo 2.50) | **19.50** | [confirmar] | Oz Robotics/Nexus |
| 11 | IMU (heading/yaw) | Bosch BNO055 (Adafruit #2472) | 2 | **34.95** | **69.90** | Nuevo | Adafruit/DigiKey |
| 12 | Regulador buck | MP1584-EN (módulo, pack de 6) | 6 | **0.90** (pack 6 = 5.39) | 5.40 | Nuevo | Amazon/Walmart |
| 13 | MCU placa COMM | ESP32-C6-MINI-1-N4 | 1 | **4.53** | 4.53 | Nuevo | DigiKey/Mouser |
| 14 | Ultrasonido | HC-SR04 | 1 | **5.25** | 5.25 | Nuevo | SparkFun |
| 15 | Pasivos + PCBs custom | Fab PCB TOP/DOWN/COMM (25.00) + anillo/power/COMM passives (R/C/diodos/conectores/LDO, ~14.84) | 1 lote | — | **39.84** | Nuevo | LCSC + JLCPCB |
| | **TOTAL / robot (todo nuevo, valor más alto por ítem)** | | | | **≈ 1.168** | | |

> Nota fila 6 (Zircon) y 5 (Teensy 4.1): **reusados** del robot campeón 2025. Robomov **no** vende la placa Zircon suelta (solo el kit completo USD 529); el ~150–250 es estimación — confirmar por contacto directo (Discord/email Robomov).
> Nota fila 9 (motor) y 10 (rueda): **confirmar con el equipo** qué motor usan (TT barato vs Pololu HP) y si son nuevos o reusados; la rueda Nexus necesita un **cubo/acople** al eje de 3 mm (sumar ~USD 2.5 c/u).
> Nota fila 11 (BNO055): el robot monta **2 BNO055, ambos @ 0x28 en buses separados** (primario Wire2 24/25 sin ToF; secundario Wire 18/19 con ToF). **Ambos sanos** (corrección 2026-06-15; heading validado en banco 2026-06-21). NO hay ningún BNO en 0x29 (0x29 es solo la dir de fábrica de los ToF). Prever 2–4 repuestos para Incheon.

---

## Totales (referencia internacional, USD)

| Concepto | USD (ref. int'l) | Notas |
|---|---|---|
| **Total / robot — todo nuevo** (valor más alto por ítem) | **≈ 1.168** | dominado por 2× N6 (330) + Zircon (250) + 2× OTOS (170) |
| **Total / robot — reusando CENTRAL** (Zircon 250 + Teensy 4.1 31.50 del 2025) | **≈ 887** | sustentabilidad: se reúsa el cerebro campeón 2025 |
| **Total 2 robots** — 1 reusa el CENTRAL + 1 todo nuevo | **≈ 2.055** | el arquero reúsa el cerebro 2025 |
| **Total 2 robots** — ambos todo nuevo | **≈ 2.336** | cota superior |
| **Tipo de cambio ARS↔USD** | **1480 ARS = 1 USD** (2026-06-13) | ⚠️ **ARS = USD × 1480 es el MÍNIMO**; el costo *landed* local real es **MAYOR** por impuestos y restricciones de importación (aranceles, IVA, percepciones, courier). Equivalentes de referencia mínima: ≈ **ARS 1.728.640/robot** (USD 1.168), ≈ **ARS 1.312.760** reusando CENTRAL (USD 887), ≈ **ARS 3.041.400** los 2 robots (USD 2.055) |
| **Tiempo de desarrollo** | [HORAS? — confirmar] | ≈ 4 meses (feb–jun 2026); elemento obligatorio del poster |

> **Pendiente del equipo (chico):** (a) precio real de la placa **Zircon** suelta; (b) qué **motor** usan y si es nuevo/reusado (se cargó la cota alta Pololu HP $23.95; el TT genérico es ~$3); (c) **C-rating / marca / peso** de la batería (capacidad **6800 mAh** y **7.4 V** ya cargados); (d) **horas** de desarrollo. (El **tipo de cambio** ya está confirmado: **1480 ARS = 1 USD**, 2026-06-13 — el equivalente en pesos es el **piso**; el *landed* real es mayor por importación.) El resto ya está con precio internacional de referencia (**un solo valor = el más alto por ítem**). **Total/robot ≈ USD 1.168.**
> **Cierre:** estos totales ya se pueden volcar a **BOM.md §3.1** / **POSTER Zona E** / **TDP** (cubren A4/A5/A11 de MEJORAS-PENDIENTES).
