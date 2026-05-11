---
title: "Análisis de los archivos de fabricación del 2026-04-20 (commits de Enzo)"
date: 2026-05-10
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: draft
tags: [pcb, gerbers, comparacion, fabricacion, comm-board]
robot: ambos
area: electronica
tipo: analisis
---

# Análisis archivos 04-20 vs 04-12 — placas Base, Comm, Top

## Contexto

Tras hacer el rebase del commit `a40cae6`, el repo incorporó 3 commits previos de Enzo Juarez (`85a54fa`, `343c420`, `d367733`) del 2026-04-20 14:31 que agregaron archivos de fabricación en `hardware/electronics/gerber_file/Placas/`:

```
Placas/Base/   BOM CSV + Gerber ZIP + PickAndPlace CSV
Placas/Comm/   BOM XLSX + Gerber ZIP + PickAndPlace XLSX  ← placa nueva en el repo
Placas/Tope/   Gerber ZIP (no BOM, no PickAndPlace)        ← typo "Tope" en vez de "Top"
```

Este documento analiza si estos archivos **resuelven o modifican** algo de lo que documentamos hoy temprano:
- `research/in-progress/2026-05-10-auditoria-pcb-down-unrouted-nets.md` (CRÍTICO: O1/O2/O3 + SDA1/SCL1 + 4 nets E sin rutear en el PCB JSON 04-12).
- `hardware/electronics/mapa-pines-placas-nuevas.md` (resolución Q3 del Wire1 remap en el TOP).
- 8 team-tasks (TASK-001 a TASK-008).

---

## Hallazgo 1: BOM Base 04-12 == BOM Base 04-20 (componentes idénticos)

Comparación línea por línea de los dos BOMs:

| Componente | 04-12 cant. | 04-20 cant. | Diferencia |
|-----------|------------|------------|-----------|
| ALS-PT19 (sensor luz) | 32 | 32 | igual |
| LED 0402 naranja | 32 | 32 | igual |
| 330 Ω | 32 | 32 | igual |
| 10 kΩ | 32 | 32 | igual |
| CD4051BM (mux 8:1) | 4 | 4 | igual |
| SparkFun OTOS | 2 | 2 | igual |
| Teensy 4.0 | 1 | 1 | igual |
| MP1584-EN | 2 | 2 | igual |
| 100nF | 6 | 6 | igual |
| B5819W (Schottky) | 2 | 2 | igual |
| 2541WV-04P (hdr 4P) | 2 | 2 | igual |
| Dean-T-F (batería) | 1 | 1 | igual |

**Cero diferencias** en componentes ni cantidades. **El orden de listado cambió** (Enzo probablemente re-exportó), pero las partes son las mismas.

### Implicación inmediata

El BOM no determina el routing — solo el conjunto de partes. **Las placas físicas que llegaron de JLCPCB pueden estar fabricadas con el design del 04-12 (con unrouted nets) o con un 04-20 modificado.** Sin acceso al PCB JSON / Schematic del 04-20, no se puede confirmar cuál es el caso.

**Si las placas físicas son del 04-12** → TASK-001 sigue como P0 crítico, hay que hacer rework manual con jumpers o pedir refabricación.

**Si Enzo modificó el routing entre 04-12 y 04-20** y los archivos `.json` del 04-12 quedaron en `pcb_design/down_board/` por error → las placas físicas pueden estar OK, pero el repo tiene un schematic stale.

---

## Hallazgo 2: Placa COMM revelada (BOM Comm 04-20)

Esta placa **NO estaba en el repo antes**. Lo que documenté hoy en `mapa-pines-placas-nuevas.md` era hipotético ("ESP32 + display + ESP-NOW"). El BOM Comm 04-20 confirma y especifica:

| Designador | Componente | Función |
|-----------|-----------|---------|
| **U1** | **ESP32-C6-MINI-1-N4** | MCU principal — WiFi 6 / BLE 5.0 / Thread / Zigbee / ESP-NOW. **NO ES ESP32 clásico** — es C6 (RISC-V). |
| **U2** | UA78M33CDCYR | Regulador lineal 3.3V (TI) |
| **U3** | 2541WV-06P | Header 6P |
| **U4, U5** | 2541WV-04P | 2 conectores UART 4P (UART_OUT + UART_IN, probable: hacia TOP + spare) |
| **U6** | **TXS0102DCUR** | **Level shifter I2C bidireccional 2 canales** (1.65-5.5V) |
| **U7** | **LIS3DHTR** | **Acelerómetro triaxial I2C** — para detección de movimiento ("shake to start" del módulo árbitros) |
| CONNECT, PROG | TS-1088-AR02016 | 2 push-button switches (programación + connect/start) |
| C1, C9 | 330nF | Capacitores |
| C2-C7, C11 | 100nF | Decoupling caps |
| C10 | 100µF | Capacitor de bulk |
| R1-R5 | 10 kΩ | Pull-ups (probables I2C + boot strap) |
| R6, R7 | 1 kΩ | (otras resistencias) |

### Notables

- **Display OLED:** no aparece en el BOM. Probablemente es un módulo separado (breakout SSD1306 típico) que se conecta por el header 6P. **Por eso el TXS0102 level shifter** — si la OLED es 5V, el ESP32-C6 (3.3V) la habla a través del shifter.
- **ESP32-C6 vs ESP32 clásico:** el firmware oficial del repo `robocup-junior/soccer-communication-module` fue diseñado para ESP32 clásico (Xtensa LX6). **El ESP32-C6 usa RISC-V** y SDK distinto. **TASK-006 puede necesitar port** — no es solo "flashear y listo".
- **Acelerómetro LIS3DHTR:** confirma la especificación oficial RCJ "shake to start" — el árbitro mueve el módulo para activar. Es estándar del módulo RCJ Communication Module.

---

## Hallazgo 3: PickAndPlace Base 04-20 — componentes en ambas caras

El archivo de pick&place tiene 148 componentes (149 líneas incluyendo header). Estructura:

```
Designator | Footprint | Mid X (mm) | Mid Y (mm) | Ref X | Ref Y | Pad X | Pad Y | Layer | Rotation | Comment
```

**Datos relevantes:**
- Capacitores C1-C6 → **Layer B (Bottom)** con rotation 180°. Confirma componentes SMD en ambas caras.
- Diodos D1, D2 → Layer B.
- Sensores F1-F32 (ALS-PT19) → Layer B en su mayoría (los sensores miran hacia abajo, hacia el carpet — coherente).
- Total de 32 sensores + 32 LEDs + 32 R330 + 32 R10k + 4 CD4051 + 2 OTOS + 1 Teensy + reguladores + conectores ≈ 148 ✓

No hay jumpers o vías "manuales" agregadas que sugieran fix post-routing. Esto es consistente con la hipótesis "design 04-12 == 04-20".

---

## Hallazgo 4: Gerbers — sin medio limpio de comparación

| Archivo | Tamaño | D01 (draw) | Aperture defs |
|---------|--------|-----------|---------------|
| `/Base/Gerber_TopLayer.GTL` (04-20) | 13.1 KB | 220 | 10 |
| `/Base/Gerber_BottomLayer.GBL` (04-20) | 60.9 KB | 1251 | 23 |

Comparando con PCB JSON 04-12 (que tenía 142 tracks top + 454 tracks bottom):
- Top: 142 tracks → 220 D01 (ratio 1.55×). Cada track del JSON se segmenta en varios D01 en Gerber.
- Bottom: 454 tracks → 1251 D01 (ratio 2.76×). Mucha más segmentación en bottom.

El ratio asimétrico es **consistente con un routing denso en bottom** (donde van los 32 sensores), no necesariamente con tracks nuevos.

**Conclusión:** los Gerbers RS-274X no incluyen net names. Comparar visualmente Top04-20 vs Top04-12 requeriría tener ambos PCB JSONs (no los tenemos del 04-20) o usar una herramienta de diff de Gerbers (gerbv, gerber-viewer).

---

## Hallazgo 5: Placa Tope/Top — solo 1 archivo entregado

El commit `d367733` "add files to top board" agregó únicamente el **Gerber ZIP** de la placa TOP, sin BOM ni PickAndPlace separados. Esto contrasta con Base (3 archivos) y Comm (3 archivos).

**Hipótesis:** Enzo asumió que el BOM/PickAndPlace de la TOP siguen siendo los del 04-12 que ya están en `pcb_design/top_board/`. Sería bueno confirmar.

También: la carpeta se llama "**Tope**" (typo). Renombrar a "**Top**" en una próxima sesión de cleanup.

---

## Impacto en las tareas existentes

### TASK-001 (PCB DOWN unrouted nets) — **sin cambios**

El BOM idéntico no confirma ni descarta que el routing haya cambiado. Por defecto: **asumir que las placas físicas tienen los unrouted nets del 04-12**. TASK-001 sigue como P0 crítico hasta que Enzo confirme.

### TASK-002 (DRC + ERC del proyecto completo) — **sin cambios**

Sigue siendo crítico. El proceso de hacer DRC en EasyEDA debería ejecutarse sobre el último design (sea 04-12 o 04-20).

### TASK-003 (confirmar Wire1 remap TOP) — **sin cambios**

El Gerber TOP 04-20 no incluye net names. La confirmación requiere abrir el PCB JSON 04-12 (que sí tengo) o multímetro.

### TASK-005 (exportar Gerbers) — **parcialmente resuelto**

- ✅ Gerbers Base 04-20 disponibles.
- ✅ Gerbers Comm 04-20 disponibles.
- ✅ Gerbers Top 04-20 disponibles.
- ❌ Faltan PickAndPlace y BOM **del TOP** 04-20.
- ❌ Faltan PCB JSON / Schematic JSON / Schematic PDF de 04-20 para las 3 placas. Estos son **útiles** para revisión humana en EasyEDA (más que los Gerbers, que son para fabricación).

### TASK-006 (firmware RCJ en COMM) — **MODIFICADO**

Originalmente asumí ESP32 clásico. Con el BOM confirmando **ESP32-C6**, hay que verificar:
1. **¿El firmware oficial RCJ soporta ESP32-C6?** El repo `robocup-junior/soccer-communication-module` debe revisarse — si solo soporta ESP32 clásico, hay que **portar** (cambiar SDK, recompilar para target RISC-V, posiblemente ajustar pinout porque C6 tiene menos GPIO).
2. **Si no soporta C6**, opciones:
   - (a) Port del firmware oficial al C6 (trabajo no trivial, ~días).
   - (b) Escribir firmware propio que cumpla la especificación del protocolo RCJ (más rápido si se conoce el protocolo).
   - (c) Cambiar el ESP32-C6 por uno clásico (rework de la placa — no recomendable).

---

## Tareas nuevas que se desprenden

**TASK-009 (nueva):** Confirmar si el design del PCB Base cambió entre 04-12 y 04-20. Pedir a Enzo los archivos JSON (PCB + Schematic) del 04-20 para las 3 placas. Es lo único que da certeza sin abrir EasyEDA manualmente.

**TASK-010 (nueva):** Verificar compatibilidad del firmware oficial RCJ con ESP32-C6. Si no es compatible, decidir entre port o firmware propio.

**TASK-011 (sugerida):** Renombrar carpeta `hardware/electronics/gerber_file/Placas/Tope/` → `Top/` (typo a corregir).

---

## Resumen de qué cambia y qué no

| Tema | Estado |
|------|--------|
| Componentes de la placa Base | Confirmado idéntico a 04-12 |
| **Routing de la placa Base** | **No se puede confirmar sin PCB JSON 04-20** |
| **Placa COMM — hardware definitivo** | **Revelado por primera vez en el repo** |
| ESP32-C6 confirmado (no clásico) | Modifica supuestos de firmware (TASK-006) |
| Wire1 remap del TOP | Aún por confirmar (TASK-003) |
| Unrouted nets de la Base | Aún por confirmar (TASK-001) |
| Acelerómetro LIS3DHTR en COMM | Confirmado |
| Level shifter TXS0102 para OLED | Confirmado |

## Próximo paso recomendado

Pedir a Enzo el **PCB JSON del 04-20** para las 3 placas. Con eso resolvemos en ~10 minutos si los unrouted nets se rerutearon, sin necesidad de abrir EasyEDA manualmente.
