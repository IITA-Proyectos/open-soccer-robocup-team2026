---
title: "Placa BASE / DOWN (Roboliga 2026 Futbol) — Componentes y circuito"
date: 2026-05-17
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [electronica, base-board, down-board, teensy, sensores-linea, otos, referencia]
robot: ambos
area: electronica
tipo: referencia
---

# Placa BASE / DOWN (Roboliga 2026 Futbol) — Componentes y circuito

> **Fuente:** `Base-20260517T175213Z-3-001.zip` (entregado 2026-04-20):
> `BOM_Roboliga_2026_Futbol_2026-04-20.csv`,
> `PickAndPlace_PCB_Roboliga_2026_Futbol_2026-04-20.csv`, gerbers.
> EasyEDA v6.5.40, gerbers generados 2026-04-20 12:49:57.
> Esta es la placa que `mapa-pines-placas-nuevas.md` llama **DOWN**.

## 1. Qué es esta placa

**Motherboard central Teensy 4.0 + plato estructural del robot con anillo de
32 sensores ópticos de línea.** No es placa de motores (no tiene drivers ni
H-bridges ni MOSFETs de potencia).

| Dato | Valor |
|------|-------|
| Dimensiones | **≈ 175.1 mm × 165.7 mm** (`Gerber_BoardOutlineLayer.GKO`) — contorno redondeado tipo plato de chasis |
| MCU | **Teensy 4.0** (`U7`, BOM fila 7) — NXP i.MX RT1062, Cortex-M7 @ 600 MHz |
| Caras | 142 de 148 componentes en cara **Bottom** (anillo de sensores); 6 en Top (power + MCU + conectores) |
| Montaje | Agujeros NPTH 3.0 mm y 3.5 mm = montaje M3 al chasis (es el plato base estructural) |

## 2. BOM completo (13 líneas, 474+ instancias + Teensy)

| Ref | Parte | Qty | Función |
|-----|-------|-----|---------|
| U5, U6 | SparkFun OTOS (header) | 2 | **Sensor óptico de odometría** (X/Y/heading). 2 posiciones (izq/der; confirmar cuál se puebla) |
| LED1–LED32 | LED 0402 | 32 | Emisores del anillo de sensores de línea |
| F1–F32 | ALS-PT19 (Everlight) | 32 | Fototransistores — detectores del anillo (línea blanca / borde de cancha) |
| R1–R32 | 330 Ω | 32 | Limitación de corriente, 1 por LED emisor |
| R33–R64 | 10 kΩ | 32 | Carga/bias de cada fototransistor |
| **U7** | **Teensy 4.0** | 1 | **MCU principal** del robot |
| U8, U9 | MP1584EN (módulo SIP 4-pin) | 2 | **Reguladores buck DC-DC** (2 rails independientes) |
| XP1 | Dean-T-F | 1 | **Conector batería** LiPo (entrada cruda) |
| C1–C6 | 100 nF | 6 | Desacople (muxes + OTOS + reguladores) |
| D1, D2 | B5819W Schottky 1A/40V | 2 | Protección de polaridad / OR-ing en la entrada |
| U1–U4 | **CD4051BM** | 4 | **Mux analógico 8:1** ×4 = 32 canales → multiplexa los 32 fototransistores a pocos ADC del Teensy |
| U10, U11 | 2541WV-04P (header 4P) | 2 | Conectores 4 pines 2.54 (probable I²C/Qwiic o UART a periféricos) |

**No hay** IMU, driver de motor, ESP32/STM32 ni radio en esta placa. Toda la
sensórica de movimiento es el OTOS. El cerebro es el Teensy 4.0.

## 3. Bloques de circuito

- **A — Power (cara Top):** `XP1` Dean (LiPo) → `D1/D2` Schottky (protección) →
  `U8/U9` MP1584 buck ×2 (2 rails, p.ej. lógica 5 V y rail de sensores/3.3 V).
  `C1–C6` 100 nF desacople. (Set-points de los trimpots de U8/U9: medir en
  hardware — no documentado.)
- **B — MCU (Top):** `U7` Teensy 4.0. Lee los COM de los 4 muxes por ADC,
  maneja líneas de selección S0/S1/S2 compartidas, I²C al OTOS.
- **C — Anillo de 32 sensores ópticos (Bottom, 142/148 placements):** 32 pares
  LED+fototransistor distribuidos en anillo al perímetro (coordenadas P&P
  trazan un arco), 4× CD4051 (cada mux cubre un sector de ~8 sensores).
  Detección de **línea blanca / borde de cancha** RCJ.
- **D — Odometría:** `U5/U6` headers OTOS (I²C/Qwiic), 2 posiciones simétricas.
- **E — Conectores aux:** `U10/U11` 4-pin (pinout VCC/GND/SDA/SCL o UART —
  **no confirmable desde serigrafía gerber**, requiere schematic o render).

## 4. Open items (no fabricar evidencia — medir/confirmar)

1. Voltajes de salida de `U8/U9` MP1584 — medir trimpots reales.
2. Pinout de `U10/U11` (I²C vs UART vs power) — leer del schematic / render.
3. ¿Ambas posiciones OTOS (`U5` y `U6`) pobladas, o una es spare?
4. ¿El anillo de LEDs es always-on o gateado por un pin del Teensy?
5. Qué ADC del Teensy lee cada mux COM y cómo se comparten S0/S1/S2 — schematic.

> La serigrafía no se puede extraer del gerber (trazo vectorial, no ASCII). Los
> rótulos/pinouts impresos solo se leen con visor de gerber o el proyecto
> EasyEDA fuente.

## 5. Fuentes

`C:\Users\violl\iitasoccer\placaspedidas\Base-20260517T175213Z-3-001.zip` →
`BOM_Roboliga_2026_Futbol_2026-04-20.csv`,
`PickAndPlace_PCB_Roboliga_2026_Futbol_2026-04-20.csv`,
`Gerber_BoardOutlineLayer.GKO`, `Drill_NPTH_Through.DRL`.
Cruzado con `hardware/electronics/mapa-pines-placas-nuevas.md` (placa DOWN).
