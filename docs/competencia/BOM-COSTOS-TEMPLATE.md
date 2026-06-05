# Planilla de Costos — BOM (plantilla de trabajo)
## [NOMBRE DEL EQUIPO] · RoboCupJunior Soccer **Open** · Incheon 2026

> 📝 **Planilla de trabajo — completar precios reales (facturas) y volcar el total a BOM.md §3.1 / POSTER Zona E / TDP §1.8 (items A4/A5/A11 de MEJORAS-PENDIENTES).**

> **Cómo usar esta planilla.** Las cantidades son **por robot** (salvo nota). Reemplazá cada `[COSTO?]` con el **precio real de factura** (ARS y/o USD). El **Subtotal/robot** = Precio unit. × Cant/robot. Cuando esté completa, sumá los subtotales en las filas de TOTAL de abajo, calculá el **Total 2 robots** (= Total/robot × 2, ajustando los ítems reusados/compartidos que correspondan) y volcá el resultado a **BOM.md §3.1**. **No inventar precios:** dejar `[COSTO?]` en lo que falte conseguir.

---

## Tabla de costos por robot

| # | Componente | Part/Modelo | Cant/robot | Precio unit. (ARS) | Precio unit. (USD) | Subtotal/robot | Nuevo/Reusado | Fuente (factura/link) |
|---|---|---|---|---|---|---|---|---|
| 1 | Cámara de visión | OpenMV Cam N6 (STM32N6 + NPU, sensor PAG7936) | 2 | [COSTO?] | [COSTO?] | [COSTO?] | Nuevo | [COSTO?] |
| 2 | Odometría óptica | SparkFun OTOS (QWIIC, designadores U5/U6) | 2 | [COSTO?] | [COSTO?] | [COSTO?] | Nuevo | [COSTO?] |
| 3 | MCU placa TOP | Teensy 4.0 (LCSC C99001332551) | 1 | [COSTO?] | [COSTO?] | [COSTO?] | Nuevo | [COSTO?] |
| 4 | MCU placa DOWN | Teensy 4.0 (LCSC C99001332551) | 1 | [COSTO?] | [COSTO?] | [COSTO?] | Nuevo | [COSTO?] |
| 5 | MCU placa CENTRAL | Teensy 4.1 (Cortex-M7 @600 MHz) | 1 | [COSTO?] | [COSTO?] | [COSTO?] | Reusado (Nacional 2025) | [COSTO?] |
| 6 | PCB/Shield CENTRAL | Zircon Rev v15 (Robomov, robomov.net) | 1 | [COSTO?] | [COSTO?] | [COSTO?] | Reusado (Nacional 2025) | [COSTO?] |
| 7 | Sensor ToF multizona | ST VL53L7CX (8×8 zonas, módulo Pololu) | 4 | [COSTO?] | [COSTO?] | [COSTO?] | Nuevo | [COSTO?] |
| 8 | Batería | LiPo 2S 7.4 V (mAh/C-rating/marca [SPEC?]) | 1 [¿1–2?] | [COSTO?] | [COSTO?] | [COSTO?] | Nuevo | [COSTO?] |
| 9 | Motor de tracción | Motor DC "TT" (KIWI 3 ruedas a 120°) | 3 | [COSTO?] | [COSTO?] | [COSTO?] | [¿Nuevo/Reusado?] | [COSTO?] |
| 10 | Rueda omnidireccional | Rueda omni (Ø/material/rodillos [SPEC?]) | 3 | [COSTO?] | [COSTO?] | [COSTO?] | [¿Nuevo/Reusado?] | [COSTO?] |
| 11 | IMU (heading/yaw) | Bosch BNO055 (designadores U10/U11) | 1–2 [prever 2–4 repuestos] | [COSTO?] | [COSTO?] | [COSTO?] | Nuevo | [COSTO?] |
| 12 | Regulador buck | MP1584-EN (módulo SIP 4-pin) | 6 (2/placa) | [COSTO?] | [COSTO?] | [COSTO?] | Nuevo | [COSTO?] |
| 13 | MCU placa COMM | ESP32-C6-MINI-1-N4 (en PCB COMM) | 1 | [COSTO?] | [COSTO?] | [COSTO?] | Nuevo | [COSTO?] |
| 14 | Ultrasonido | HC-SR04 (designador U6 en TOP) | 1 | [COSTO?] | [COSTO?] | [COSTO?] | Nuevo | [COSTO?] |
| 15 | Pasivos genéricos (agregado) | Resistencias/conectores/diodos/LDO + pasivos PCB custom (anillo línea, muxes, etc.) | 1 lote | [COSTO?] | [COSTO?] | [COSTO?] | Nuevo | [COSTO?] |

> Nota fila 6 (Zircon) y fila 5 (Teensy 4.1): ítems **reusados** del robot campeón Nacional 2025 — registrar precio si se conoce, pero no necesariamente se recompra para 2026.
> Nota fila 11 (BNO055): el repo monta 2 BNO055 pero 1 unidad (RIGHT, 0x29) está fallada; hoy compite con 1 BNO sano. Prever 2–4 unidades para Incheon (repuestos).
> Nota fila 15 (pasivos): subtotal con precio real ya conocido en repo ≈ **USD 8.20/robot** (ALS-PT19 3.71 + LED 0402 0.51 + CD4051 3.84 + B5819W 0.14); falta sumar resistencias, conectores Deans-T, MP1584 (fila 12), LDO UA78M33, TXS0102, LIS3DH, pulsadores, PCBs custom prorrateadas.

---

## Totales

| Concepto | ARS | USD |
|---|---|---|
| **Total / robot** | [COSTO?] | [COSTO?] |
| **Total 2 robots** | [COSTO?] | [COSTO?] |
| **Tipo de cambio ARS↔USD** | [TC del día] | — |
| **Tiempo de desarrollo** | [HORAS?] (≈ 4 meses feb–jun 2026, multi-agente; elemento obligatorio del poster) | — |

> **Recordatorio de cierre:** una vez completos los totales, volcarlos a **BOM.md §3.1** (Costo total estimado), al **POSTER Zona E** (Method/Design: costo + tiempo de desarrollo) y al **TDP §1.8**. Estos cubren los items **A4 (costo total + conversión ARS), A5 (precios reales por componente) y A11 (tiempo de desarrollo)** de MEJORAS-PENDIENTES.
