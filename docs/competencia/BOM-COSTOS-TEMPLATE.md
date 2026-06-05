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
| 1 | Cámara de visión | OpenMV Cam N6 (STM32N6 + NPU) | 2 | **165** (alt. 120 Kickstarter) | **330** | Nuevo | openmv.io |
| 2 | Odometría óptica | SparkFun OTOS PAA5160E1 (SEN-24904) | 2 | **84.95** | **169.90** | Nuevo | SparkFun |
| 3 | MCU placa TOP | Teensy 4.0 (DEV-15583) | 1 | **23.80** | 23.80 | Nuevo | SparkFun/DigiKey |
| 4 | MCU placa DOWN | Teensy 4.0 (DEV-15583) | 1 | **23.80** | 23.80 | Nuevo | SparkFun/DigiKey |
| 5 | MCU placa CENTRAL | Teensy 4.1 (DEV-16771) | 1 | **31.50** | 31.50 | Reusado 2025 | SparkFun/DigiKey |
| 6 | PCB/Shield CENTRAL | Zircon Rev v15 (Robomov) | 1 | **~150–250** [verificar] · kit completo USD 529 | ~200 | Reusado 2025 | robomov.net |
| 7 | Sensor ToF multizona | ST VL53L7CX 8×8 (carrier Pololu #3418) | 4 | **19.95** (17.96 desde 5 u) | **79.80** | Nuevo | Pololu |
| 8 | Batería | LiPo 2S 7.4 V 2200 mAh (Turnigy/Zeee) | 1–2 | **~12** | 12–24 | Nuevo | HobbyKing/Amazon |
| 9 | Motor de tracción | Motor DC (TT genérico **o** Pololu 30:1 HP #1093) | 3 | **~3 (TT) / 23.95 (Pololu HP)** [confirmar cuál usan] | 9–72 | [confirmar] | AliExpress / Pololu |
| 10 | Rueda omnidireccional | Omni 38 mm (Nexus 14184) + cubo eje 3 mm | 3 | **4.00** (+~2.5 cubo) | 12 (+7.5) | [confirmar] | Oz Robotics/Nexus |
| 11 | IMU (heading/yaw) | Bosch BNO055 (Adafruit #2472; Qwiic #4646 = 29.95) | 1–2 | **34.95** | 34.95–69.90 | Nuevo | Adafruit/DigiKey |
| 12 | Regulador buck | MP1584-EN (módulo, se vende en pack de 6) | 6 | **~0.90** (pack 6 = 5.39) | 5.40 | Nuevo | Amazon/Walmart |
| 13 | MCU placa COMM | ESP32-C6-MINI-1-N4 | 1 | **4.53** (devkit ~8–10) | 4.53 | Nuevo | DigiKey/Mouser |
| 14 | Ultrasonido | HC-SR04 | 1 | **5.25** (clon genérico ~1–2) | 5.25 | Nuevo | SparkFun |
| 15 | Pasivos + PCBs custom | R/C/conectores/diodos/LDO + fab PCB TOP/DOWN | 1 lote | **~10–15** | ~12 | Nuevo | LCSC (BOM del repo) |

> Nota fila 6 (Zircon) y 5 (Teensy 4.1): **reusados** del robot campeón 2025. Robomov **no** vende la placa Zircon suelta (solo el kit completo USD 529); el ~150–250 es estimación — confirmar por contacto directo (Discord/email Robomov).
> Nota fila 9 (motor) y 10 (rueda): **confirmar con el equipo** qué motor usan (TT barato vs Pololu HP) y si son nuevos o reusados; la rueda Nexus necesita un **cubo/acople** al eje de 3 mm (sumar ~USD 2.5 c/u).
> Nota fila 11 (BNO055): el robot monta 2 pero 1 (0x29) está fallada → hoy corre con 1 sano. Prever 1–2 repuestos.

---

## Totales (referencia internacional, USD)

| Concepto | USD (ref. int'l) | Notas |
|---|---|---|
| **Total / robot — todo nuevo** (motores TT, Zircon ~200) | **≈ 1.000** | dominado por 2× cámara N6 (330) + 2× OTOS (170) + Zircon (~200) |
| **Total / robot — reusando CENTRAL** (Zircon + Teensy 4.1 del 2025) | **≈ 770** | sustentabilidad: se reúsa el cerebro campeón 2025 |
| **Total 2 robots** (según reusados) | **≈ 1.800 – 2.000** | ajustar si comparten/reúsan piezas entre arquero y delantero |
| **Tipo de cambio ARS↔USD** | [TC del día] | el equipo pone el del momento |
| **Tiempo de desarrollo** | [HORAS? — confirmar] | ≈ 4 meses (feb–jun 2026); elemento obligatorio del poster |

> **Pendiente del equipo (chico):** (a) precio real de la placa **Zircon** suelta; (b) qué **motor** usan y si es nuevo/reusado; (c) **tipo de cambio** del día y **horas** de desarrollo. El resto ya está con precio internacional de referencia verificado.
> **Cierre:** estos totales ya se pueden volcar a **BOM.md §3.1** / **POSTER Zona E** / **TDP** (cubren A4/A5/A11 de MEJORAS-PENDIENTES).
