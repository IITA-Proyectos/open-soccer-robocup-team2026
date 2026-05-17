---
title: "Placa COMM (PCB1) — Componentes y circuito completo (ESP32-C6)"
date: 2026-05-17
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [electronica, comm-board, esp32, esp32c6, rcj, arbitros, referencia, pinout]
robot: ambos
area: comunicacion
tipo: referencia
---

# Placa COMM (PCB1) — Componentes y circuito completo

> **Fuente de datos:** paquete de fabricación entregado por Enzo el 2026-04-20
> (`Comm-20260517T175201Z-3-001.zip`): `BOM_Board1_PCB1_2026-04-20.xlsx`,
> `PickAndPlace_PCB1_2026-04-20.xlsx`, `Gerber_PCB1_2026-04-20.zip` y
> `FlyingProbeTesting.json`. El netlist se recuperó **exacto** del archivo de
> test de sonda volante (no es inferencia). Cruzado y consistente con
> `hardware/electronics/mapa-pines-placas-nuevas.md` (conector 6P de la placa TOP).

Esta placa es la **fork IITA del módulo oficial RoboCupJunior Soccer**
(`robocup-junior/soccer-communication-module`). El repo fork
`IITA-Proyectos/rcj-soccer-open_communication_module` solo contiene archivos de
fabricación, **no el firmware** (el firmware vive en el repo oficial — ver
[procedimiento de flash](2026-05-17-procedimiento-flash-firmware-c6.md)).

---

## 1. Identificación de la placa

| Dato | Valor | Evidencia |
|------|-------|-----------|
| Nombre CAD | `PCB1` (proyecto `Gerber_PCB1_2026-04-20`) | header `G04` de los gerbers |
| Herramienta | EasyEDA **Pro** v2.2.37.7 | header `G04` |
| MCU | **ESP32-C6-MINI-1-N4** (Espressif) | BOM fila 7 / P&P designador `U1` |
| Dimensiones | **25.40 mm × 31.20 mm** (rectángulo con esquinas redondeadas R≈1 mm) | `Gerber_BoardOutlineLayer.GKO` |
| Montaje | Sin agujeros de montaje (NPTH) — placa **plug-in**, se sostiene por sus conectores | `Drill_NPTH` ausente |
| Test de fábrica | Flying-probe test (`FlyingProbeTesting.json` presente) | paquete de fab |

El ancho de **25.4 mm = exactamente 1 pulgada** es deliberado: footprint
estandarizado del módulo RCJ. Confirmar que encastra en el slot del módulo de
comunicación previsto en el robot (dato mecánico clave).

### El MCU NO es ESP32 clásico

| Característica | ESP32 clásico | **ESP32-C6 (esta placa)** |
|----------------|---------------|---------------------------|
| CPU | Xtensa LX6 dual-core | **RISC-V single-core** |
| WiFi | 4 (802.11n) | **6 (802.11ax)** |
| Bluetooth | 4.2 | **5.0 LE** |
| Extra | — | **Thread / Zigbee (802.15.4)** |
| USB | Externo (CP2102 etc.) | **USB-Serial/JTAG nativo en el chip** |
| Flash (N4) | — | **4 MB** |

El radio es **interno al módulo ESP32-C6-MINI-1**. **No hay módulo RF aparte,
ni cristal externo, ni display en esta placa** (el cristal es interno al
C6-MINI; el display OLED es externo y va por el bus I²C — ver §4).

---

## 2. Bill of Materials completo (12 líneas, 26 instancias)

Verbatim de `BOM_Board1_PCB1_2026-04-20.xlsx`:

| # | Ref | Parte | Footprint | Función |
|---|-----|-------|-----------|---------|
| 1 | C1, C9 | 330 nF (0805B334K500NT) | C0805 | Filtro / bulk local |
| 2 | C2–C7, C11 | 100 nF (0603B104K500NT) | C0603 | Desacople de alimentación de los ICs |
| 3 | C10 | 100 µF (CA45-B-6.3V tántalo) | CAP-SMD | Bulk del rail 3.3 V |
| 4 | CONNECT, PROG | TS-1088-AR02016 | SW-SMD | **2 pulsadores táctiles** (botones de usuario / bootstrap) |
| 5 | R1–R5 | 10 kΩ | R0603 | Pull-ups (straps de boot, líneas de botón, I²C) |
| 6 | R6, R7 | 1 kΩ | R0603 | Limitación / serie |
| 7 | **U1** | **ESP32-C6-MINI-1-N4** | WIFIM-SMD 61P | **MCU + radio integrado** |
| 8 | U2 | UA78M33CDCYR (TI) | SOT-223 | **LDO lineal 3.3 V** (regulador a bordo) |
| 9 | U3 | 2541WV-06P | HDR-TH 6P | **Header 6 pines 2.54 mm — bus de interfaz al robot** |
| 10 | U4, U5 | 2541WV-04P | HDR-TH 4P | **2 headers 4 pines 2.54 mm — power-in (U5) e I²C aux (U4)** |
| 11 | U6 | TXS0102DCUR (TI) | VSON8 | **Level shifter bidireccional 2-bit** (UART robot ↔ ESP 3.3 V) |
| 12 | U7 | LIS3DHTR (ST) | LGA-16 | **Acelerómetro 3 ejes** (shake-to-start, en I²C) |

> **Nota sobre los botones:** la fork IITA usa `CONNECT` y `PROG` como
> serigrafía. En el netlist son `BUT_1` (CONNECT) y `BUT_2` (PROG). **No hay
> botón EN/RESET físico** — esto define el procedimiento de flash (ver doc
> aparte). El pulsador `PROG` lleva el strap de boot del C6 (GPIO9).

---

## 3. Pinout de conectores (recuperado exacto del netlist)

Recuperado de `FlyingProbeTesting.json` (26 componentes, 155 pads, nets reales).
Posiciones relativas al centroide de la placa.

### U5 — Header 4 pines, borde IZQUIERDO — ALIMENTACIÓN + nivel lógico

| Pin | Net | Descripción |
|-----|-----|-------------|
| U5_1 | **VIN** | Entrada de alimentación cruda desde el robot (rango placa: 5.3–25 V) |
| U5_2 | **LOGV** | Referencia de voltaje lógico del robot (3.3–5.5 V) para el level shifter |
| U5_3 | **3.3V** | Salida 3.3 V regulada (alternativa: alimentar la placa con 3.3 V directo) |
| U5_4 | **GND** | Masa |

### U3 — Header 6 pines, borde DERECHO — BUS DE DATOS AL ROBOT

| Pin | Net | Descripción |
|-----|-----|-------------|
| U3_1 | **OUT_1** | Salida árbitro: **3.3 V = GO, 0 V = STOP** (driven por el firmware) |
| U3_2 | **OUT_2** | Espejo de OUT_1 (redundante) |
| U3_3 | **RX_OUT** | UART hacia el robot — nivel adaptado por el level shifter (dominio LOGV) |
| U3_4 | **TX_OUT** | UART desde el robot — nivel adaptado por el level shifter |
| U3_5 | **USB_D+** | USB nativo del C6 — **usado para flashear** (ver doc de flash) |
| U3_6 | **USB_D−** | USB nativo del C6 — **usado para flashear** |

> **Confirmación cruzada:** la placa **TOP** (`mapa-pines-placas-nuevas.md`)
> tiene el conector `U1` 6P "PINES MODULO" con exactamente
> `OUT1, OUT2, RX_OUT, TX_OUT, USB_D+, USB_D-`. Ese conector **mate-ea con el
> U3 de la placa COMM**. El pinout está doblemente verificado.

### U4 — Header 4 pines, borde INFERIOR — I²C auxiliar (Qwiic-style)

| Pin | Net | Descripción |
|-----|-----|-------------|
| U4_1 | **GND** | Masa |
| U4_2 | **3.3V** | 3.3 V |
| U4_3 | **SCL** | I²C clock (compartido con el acelerómetro a bordo y display OLED externo) |
| U4_4 | **SDA** | I²C data |

El display OLED del módulo RCJ (si se usa) se conecta a **U4** (I²C). No está en
esta placa: es un módulo externo.

---

## 4. Reconstrucción del circuito, bloque por bloque

### Bloque A — Alimentación (verificado por netlist)

```
U5_1 VIN ──► U2 (UA78M33CDCYR LDO) ──► 3.3V rail ──► U1 (ESP32-C6), U6, U7
              │                          │
            GND (U2_2,U2_4)           C10 100µF + C2..C7,C11 100nF (desacople)
```

- `U2_1 = VIN`, `U2_3 = 3.3V`, `U2_2/U2_4 = GND` → confirma regulación VIN→3.3 V.
- El UA78M33 es un LDO lineal 3.3 V (SOT-223). Tolera la entrada amplia del
  conector. `C10` (100 µF tántalo) = bulk del rail; `C1/C9` (330 nF) y los
  100 nF = desacople.
- Alternativa documentada: alimentar directamente 3.3 V por `U5_3` si el robot
  no entrega VIN en rango.

### Bloque B — MCU + radio (U1 ESP32-C6-MINI-1-N4)

- WiFi 6 / BLE 5 / 802.15.4 integrados en el módulo. Sin cristal externo.
- `USB_D+/USB_D−` (USB nativo del C6) salen al header **U3** (pines 5 y 6) →
  **no hay conector USB-C en la placa**: se flashea cableando USB a esos pines.
- `BUT_1` → botón CONNECT; `BUT_2` → botón PROG (= strap boot GPIO9).
- I²C (SDA/SCL) hacia el acelerómetro U7 y el header U4.
- UART interno del ESP (`RX_ESP`/`TX_ESP`) → al level shifter U6.

### Bloque C — Level shifter UART (U6 TXS0102DCUR)

```
Robot (dominio LOGV)            ESP32-C6 (dominio 3.3V)
   U3_3 RX_OUT ◄──┐                ┌──► RX_ESP (U1)
   U3_4 TX_OUT ◄──┤  TXS0102 (U6)  ├──► TX_ESP (U1)
   U5_2 LOGV ─────┘ (VccA=LOGV)    └─ (VccB=3.3V)
```

- El TXS0102 traslada el UART entre el voltaje lógico del robot (pin `LOGV`,
  3.3–5.5 V) y los 3.3 V del ESP. Por eso el conector de power **U5** incluye
  `LOGV`: el robot le dice a la placa con qué nivel lógico habla.
- `U6` tiene ambos rails (`LOGV` y `3.3V`) y conecta `RX_OUT/TX_OUT` ↔
  `RX_ESP/TX_ESP` — verificado en netlist.

### Bloque D — Acelerómetro (U7 LIS3DHTR)

- En el bus I²C (`SDA/SCL/3.3V/GND`), también expuesto en `U4`.
- Uso previsto: **"shake to start"** del módulo RCJ.
- **Nota firmware:** el firmware oficial v0.91 **NO usa todavía** el
  acelerómetro ("Enabling giro/accelerometer" figura como "currently working
  on" en el README oficial). El hardware está, el software aún no lo explota.

### Bloque E — Botones y straps

- `CONNECT` (BUT_1) → botón de aplicación BLE (en el firmware C6 = GPIO18,
  hold 5 s = `ble_disconnect()`).
- `PROG` (BUT_2) → GPIO9 = **strap de boot del C6** + Button 2 del firmware
  (doble pulsación = pedir penalización). Comparte pin con el bootstrap, lo que
  importa para el flasheo.
- `R1–R5` 10 kΩ = pull-ups de straps/botones/I²C.

### Bloque F — Drills (consistencia mecánica)

`Drill_PTH_Through.DRL`: **14 agujeros de 1.10 mm = 6 + 4 + 4** = exactamente
los pines de U3 + U4 + U5. Confirma los tres headers pasantes. 54 vías de
0.305 mm. Sin NPTH → la placa **no se atornilla**, se sostiene por los headers.

---

## 5. Diagrama de conexión al robot (resumen operativo)

```
            ┌──────────────── PLACA COMM (25.4 × 31.2 mm) ────────────────┐
            │                                                              │
ROBOT  VIN ─┤U5_1                                          U3_1├─ OUT_1  ──► START/STOP al robot
       LOGV─┤U5_2  [TXS0102]                               U3_2├─ OUT_2     (3.3V=GO, 0V=STOP)
   (3.3V)  ─┤U5_3   level shift   [ESP32-C6-MINI-1-N4]     U3_3├─ RX_OUT ──► UART al robot
       GND ─┤U5_4                  [UA78M33 LDO 3.3V]      U3_4├─ TX_OUT ◄── UART desde robot
            │                      [LIS3DH accel]          U3_5├─ USB_D+ ──► (solo para flashear)
            │      U4_1 GND                                U3_6├─ USB_D−
   I²C aux ─┤U4_2 3.3V  U4_3 SCL  U4_4 SDA                      │
  (OLED RCJ)│                                                   │
            │  [CONNECT btn]   [PROG btn = strap boot GPIO9]    │
            └───────────────────────────────────────────────────┘

⚠️ VIN va directo: rango 5.3–25 V (LiPo 2S/3S OK). El módulo regula internamente.
⚠️ El robot lee START/STOP como NIVEL en OUT_1/OUT_2 (no por serial).
```

El **arranque/parada del árbitro NO es un mensaje UART**: es un **nivel de
tensión** en `OUT_1`/`OUT_2`. El firmware recibe el comando del árbitro por
**BLE** desde la app móvil del árbitro y lo traduce a ese nivel. El UART
`RX_OUT/TX_OUT` es hardware pasivo: el firmware oficial v0.91 **no lo usa** (no
hay puente robot-a-robot ni ESP-NOW en el firmware oficial).

---

## 6. Temas a analizar (frame coach)

### Pinout OUT1/OUT2 — verificar físicamente antes de confiar

**Categoría:** electrónica · **Robot:** ambos · **Prioridad:** P1

**Qué observo.** El issue oficial abierto
[#5](https://github.com/robocup-junior/soccer-communication-module/issues/5)
(2025-02-09, sin respuesta del mantenedor) reporta que la rotulación de
OUT1/OUT2 entre schematic, serigrafía y hub-board es **inconsistente** en el
proyecto oficial. Nuestro netlist da `U3_1=OUT_1`, `U3_2=OUT_2`, y el firmware
C6 los mapea a GPIO20/GPIO19. Hay tres fuentes que deben coincidir: netlist
(✓ recuperado), serigrafía física (no legible desde gerber — son trazos
vectoriales, no ASCII), y el firmware.

**Risk-no-fix.** Si la serigrafía física no coincide con el netlist, el robot
podría leer START/STOP del pin equivocado → no arranca en cancha = no compite.
**Risk-fix.** Ninguno: es verificación con multímetro, no cambia hardware.
**Tiempo estimado.** 30 min.

**Plan de prueba en hardware real.**
1. Placa COMM alimentada por `U5` (VIN+GND), firmware C6 cargado.
2. Conectar app de árbitro por BLE, dar START.
3. Multímetro en `U3_1` y `U3_2`: deben ir a **3.3 V** con START y **0 V** con
   STOP. Anotar qué pin físico del header de 6 corresponde.
4. Comparar con la serigrafía impresa de la placa (foto, archivar en repo).
5. Criterio de aceptación: netlist = serigrafía = comportamiento medido.

### Serigrafía no recuperable desde gerber

**Categoría:** docs · **Prioridad:** P2

La serigrafía de los gerbers es trazo vectorial, no texto ASCII — los rótulos
de pines impresos **no se pueden extraer del gerber**. El pinout de §3 se
recuperó del `FlyingProbeTesting.json`. **Recomendación:** abrir
`Gerber_TopSilkscreenLayer.GTO`/`.GBO` en un visor (KiCad GerbView / gerbv),
confirmar visualmente que los rótulos impresos coinciden con §3, y archivar un
PNG renderizado en esta carpeta como referencia permanente de cableado.

---

## 7. Fuentes

- Paquete de fab: `C:\Users\violl\iitasoccer\placaspedidas\Comm-20260517T175201Z-3-001.zip`
  → `BOM_Board1_PCB1_2026-04-20.xlsx`, `PickAndPlace_PCB1_2026-04-20.xlsx`,
  `Gerber_PCB1_2026-04-20.zip` (incl. `FlyingProbeTesting.json`,
  `Gerber_BoardOutlineLayer.GKO`, `Drill_PTH_Through.DRL`).
- Cruzado con `hardware/electronics/mapa-pines-placas-nuevas.md` (conector 6P TOP).
- Repo fork fab: https://github.com/IITA-Proyectos/rcj-soccer-open_communication_module
- Repo firmware oficial: https://github.com/robocup-junior/soccer-communication-module
- Issue OUT1/OUT2: https://github.com/robocup-junior/soccer-communication-module/issues/5
- Procedimiento de flash: [`2026-05-17-procedimiento-flash-firmware-c6.md`](2026-05-17-procedimiento-flash-firmware-c6.md)
