---
title: "2026-05-17 — Análisis de las 3 placas fabricadas + corrección firmware COMM (branch esp32-c6)"
date: 2026-05-17
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [electronica, comm-board, base-board, top-board, esp32c6, firmware, correccion, analisis]
robot: ambos
area: electronica
tipo: analisis
related-tasks: [TASK-006, TASK-010, TASK-013]
supersedes-partially: journal/2026-05-15-firmware-comm-c6-flash-procedure.md
---

# Análisis de las 3 placas fabricadas + corrección firmware COMM

## Contexto

Gustavo entregó 3 paquetes de fabricación (2026-04-20) en
`C:\Users\violl\iitasoccer\placaspedidas\`: **Base**, **Comm**, **Tope**.
Pidió: (1) identificar el modelo exacto de ESP32 de la placa Comm, (2) análisis
exhaustivo de circuitos de las 3 placas, (3) encontrar el firmware oficial
correcto, (4) dejar la documentación del repo correcta (componentes + circuito
+ procedimiento de flash). Trabajo hecho con 3 procesos en paralelo
(firmware oficial / placa Base / placa Tope + gerbers Comm).

## Qué se hizo

- Extracción y parseo de los 3 ZIP (BOM xlsx/csv, Pick&Place, gerbers, drills,
  `FlyingProbeTesting.json`).
- Comm: BOM completo + netlist **recuperado exacto** del flying-probe JSON.
- Base/Tope: BOM/P&P/gerbers analizados; Tope solo trae gerbers.
- Firmware: clonado el repo oficial `robocup-junior/soccer-communication-module`
  a `C:\Users\violl\iitasoccer\_official_fw`, revisados los 4 branches.

## Qué se midió / observó (datos crudos)

### 1. Modelo de ESP32 (respuesta a la pregunta principal)

**ESP32-C6-MINI-1-N4** (Espressif). Designador `U1`. Aparece en
`BOM_Board1_PCB1_2026-04-20.xlsx` fila 7 y en el Pick&Place. RISC-V single-core,
WiFi 6 + BLE 5 + 802.15.4, 4 MB flash, USB-Serial/JTAG nativo. **No** es ESP32
clásico ni S3/C3.

### 2. Placa Comm (PCB1) — 25.4 × 31.2 mm, plug-in

Componentes: U1 ESP32-C6-MINI-1-N4, U2 UA78M33 LDO 3.3 V, U6 TXS0102 level
shifter, U7 LIS3DH accel, U3 header 6P, U4/U5 headers 4P, botones CONNECT/PROG.
Sin OLED ni cristal en placa. Pinout recuperado exacto del netlist (U5=power,
U3=bus al robot, U4=I²C aux). El conector U3 6P (`OUT_1,OUT_2,RX_OUT,TX_OUT,
USB_D+,USB_D−`) **coincide con el conector 6P de la placa TOP** documentado en
`mapa-pines-placas-nuevas.md` → pinout doblemente verificado.
→ `hardware/electronics/comm-board/2026-05-17-placa-comm-componentes-y-circuito.md`

### 3. Placa Base/DOWN (Roboliga 2026 Futbol) — ≈175×166 mm

Motherboard **Teensy 4.0** (U7) + anillo de 32 sensores ópticos de línea
(32 LED + 32 ALS-PT19 + 4× CD4051 mux 8:1) + 2× SparkFun OTOS + 2× MP1584 buck
+ Dean. Sin drivers de motor. Es el plato base estructural.
→ `hardware/electronics/2026-05-17-placa-base-down-componentes-y-circuito.md`

### 4. Placa TOP (Roboliga2026 TOP) — ≈230×104 mm — GAP

**El paquete solo trae gerbers: NO hay BOM ni Pick&Place.** Componentes no
determinables desde gerber. Lo conocido viene del schematic decodificado en
`mapa-pines-placas-nuevas.md` (Teensy 4.0 U14, 2× BNO055, 4× ToF, etc.).
→ `hardware/electronics/2026-05-17-placa-top-analisis-gerbers.md` + TASK-013.

### 5. Firmware — CONTRADICCIÓN con documentación previa

El repo oficial **no tiene tags/releases**. Tiene 4 branches:

| Branch | Chip objetivo | Pin map (SDA/SCL/BTN/BTN2/OUT1/OUT2) |
|--------|---------------|--------------------------------------|
| `master` | **C5** (`//ESP-C5` NO es typo) | 2 / 3 / 10 / 7 / 9 / 8 |
| `esp32-c5` | C5 | 2 / 3 / 10 / 7 / 9 / 8 |
| **`esp32-c6`** ← **EL CORRECTO** | **C6** | **6 / 7 / 18 / 9 / 20 / 19** |

La doc previa (`journal/2026-05-15-...`, TASK-006, pin map propagado a
TASK-010) miró solo `master` y asumió que el chip era C6 con el pin map de C5.
**Equivocado.** Lo correcto: branch **`esp32-c6`**, commit
`ffb4e3c1a1ddac2b3d3ed7bd8a24aacc19ea0081`, core Arduino-ESP32 **3.2.2** exacto,
board `ESP32C6 Dev Module`, nombre BLE **`RCJs-m_<MAC>`** (no
`RCJ-soccer_module`), método de flash oficial = cablear USB D+/D−/VIN/GND a los
pines del header (no hay USB-C). El firmware imprime `PLAY`/`STOP` por Serial
@115200 en runtime (sí sirve el Serial Monitor). Start/stop al robot es **nivel
en OUT1/OUT2**, no UART; el árbitro habla por **BLE**. El firmware oficial v0.91
**no** usa el acelerómetro ni hace puente ESP-NOW.

## Conclusión

- ESP32 = **ESP32-C6-MINI-1-N4**, confirmado por BOM y netlist.
- Las 3 placas quedaron documentadas en `hardware/electronics/` (Comm con
  pinout exacto; Base completo; TOP con gap explícito).
- La decisión de TASK-010 ("Plan A, no portar") **se mantiene** — pero su
  evidencia y el pin map estaban mal. Firmware correcto = branch `esp32-c6`.
- Documentación previa equivocada corregida sin borrarla (banners + notas
  fechadas que linkean a la fuente de verdad nueva).

## Próximos pasos

- **TASK-006** (P0, pending): ejecutar el flasheo con el procedimiento correcto
  (`hardware/electronics/comm-board/2026-05-17-procedimiento-flash-firmware-c6.md`).
- **TASK-013** (P1, nueva): Enzo recupera BOM + Pick&Place + render de
  serigrafía de la placa TOP.
- Verificación física P1: confirmar OUT1/OUT2 con multímetro (issue oficial #5:
  rotulación históricamente inconsistente) antes de confiar en cancha.
- P2: render de serigrafía de la Comm a PNG en el repo como referencia de cableado.

## Fuentes

- `C:\Users\violl\iitasoccer\placaspedidas\{Base,Comm,Tope}-*.zip`
- Clon firmware oficial: `C:\Users\violl\iitasoccer\_official_fw`
- https://github.com/robocup-junior/soccer-communication-module (branch `esp32-c6` @ `ffb4e3c`)
- Issue #5: https://github.com/robocup-junior/soccer-communication-module/issues/5
- Cruce interno: `hardware/electronics/mapa-pines-placas-nuevas.md`
- Entrada parcialmente superada: `journal/2026-05-15-firmware-comm-c6-flash-procedure.md`
