---
title: "Procedimiento de flash — Firmware oficial RCJ en placa COMM (ESP32-C6)"
date: 2026-05-17
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [firmware, comm-board, esp32c6, rcj, arbitros, protocolo, flash]
robot: ambos
area: comunicacion
tipo: protocolo
---

# Procedimiento de flash — Firmware oficial RCJ en placa COMM

> **⚠️ ESTE DOCUMENTO CORRIGE documentación previa equivocada.**
> El journal `journal/2026-05-15-firmware-comm-c6-flash-procedure.md` y la
> versión vieja de `team-tasks/2026-05-10-task-006-*` apuntaban al branch
> **`master`** del repo oficial, que compila para **ESP32-C5**, con un pinout
> equivocado para nuestra placa. El firmware correcto está en el branch
> **`esp32-c6`**. Detalle de la corrección en
> `journal/2026-05-17-analisis-3-placas-y-correccion-firmware-c6.md`.

---

## 0. Resumen ejecutivo (leer esto primero)

| Qué | Valor correcto |
|-----|----------------|
| Repo | `https://github.com/robocup-junior/soccer-communication-module` |
| **Branch** | **`esp32-c6`** (NO `master` — master es para el chip C5) |
| Commit recomendado | `ffb4e3c1a1ddac2b3d3ed7bd8a24aacc19ea0081` (2026-04-01) |
| Tags/releases | **No existen** — un tip de branch es la única referencia estable |
| Sketch | `firmware/RCj_comm_module/RCj_comm_module.ino` (Arduino IDE puro) |
| Arduino-ESP32 core | **exactamente 3.2.2** (el `update.md` oficial fija esta versión) |
| Board en Arduino IDE | `ESP32C6 Dev Module` |
| Versión firmware | v0.91 |
| Nombre BLE | **`RCJs-m_<MAC>`** (NO `RCJ-soccer_module`, ese es el de master/C5) |
| Método oficial de flash | Cablear USB a los pines del header (no hay USB-C en la placa) |

---

## 1. Pin map correcto del firmware (branch `esp32-c6`)

De `firmware/RCj_comm_module/definitions.h` en el branch `esp32-c6`:

| Función | `#define` | **GPIO (C6 — USAR ESTE)** | GPIO (C5/master — NO USAR) |
|---------|-----------|---------------------------|----------------------------|
| I²C SDA (OLED + accel) | `I2C_SDA_GPIO` | **6** | 2 |
| I²C SCL (OLED + accel) | `I2C_SCL_GPIO` | **7** | 3 |
| Botón 1 (CONNECT, hold 5 s = disconnect BLE) | `BUTTON_GPIO` | **18** | 10 |
| Botón 2 (PROG, doble pulsación = penalización) | `BUTTON2_GPIO` | **9** | 7 |
| OUTPUT1 (start/stop: 3.3V=GO, 0V=STOP) | `OUTPUT1_GPIO` | **20** | 9 |
| OUTPUT2 (espejo de OUTPUT1) | `OUTPUT2_GPIO` | **19** | 8 |
| UART speed (debug `PLAY`/`STOP`) | `UART_SPEED` | 115200 | 115200 |

> GPIO9 (Botón 2 / `PROG`) **es también el strap de boot del C6**. Relevante
> para el flasheo (sección 4 fallback).

---

## 1.bis — GUÍA DE CAMPO LISTA PARA INCHEON (Windows, paso a paso)

> El método más confiable y rápido. Pensado para hacerlo en el torneo bajo
> presión. Armar el arnés **antes de viajar** y dejarlo etiquetado.

### ¿Necesito fuente externa? — NO

El puerto USB de la PC (5 V) alimenta la placa a través de su regulador
interno. **No hace falta fuente ni batería externa para flashear.** (Variante
ultra-robusta opcional: ver "Plan robustez" más abajo.)

### Materiales (preparar y etiquetar antes del viaje)

| Item | Detalle |
|------|---------|
| **USB breakout board** | Hembra USB-A o USB-C que exponga `VBUS / D+ / D− / GND` en pines. **Preferir breakout a cable cortado** (el cable cortado tiene colores ambiguos y soldaduras frágiles). |
| 4 cables dupont hembra-hembra | Para U5/U3 (headers macho 2.54 mm). Idealmente: rojo, negro, verde, blanco. |
| Multímetro | Verificar VBUS=5 V y continuidad antes de conectar. |
| Notebook Windows | Con el entorno ya instalado (sección 2) y el firmware ya clonado en el branch `esp32-c6`. |
| Foto/diagrama del pinout impreso | Llevarlo plastificado. |

### Cableado exacto (4 hilos — NADA más)

Pines según el doc de circuito (header **U5** = 4P borde izquierdo, header
**U3** = 6P borde derecho):

| Hilo | Breakout USB | → | Pin placa COMM |
|------|--------------|---|----------------|
| Negro (GND) — **conectar 1°, desconectar último** | GND | → | **U5_4 (GND)** |
| Rojo (5 V) | VBUS / +5V | → | **U5_1 (VIN)** |
| Verde (D+) | D+ | → | **U3_5 (USB_D+)** |
| Blanco (D−) | D− | → | **U3_6 (USB_D−)** |

`LOGV`, `3.3V`, `OUT_1/2`, `RX/TX` → **NO se conectan** para flashear.

> Si en ~10 s Windows no detecta nada → **intercambiar D+ y D−** (verde↔blanco).
> Es el error #1 con cables/breakouts genéricos.

### Secuencia confiable de flasheo (la que SIEMPRE funciona)

No depender del auto-reset. Usar el método manual con el botón **PROG** (es el
strap de boot del C6) — es determinístico:

1. Breakout USB **desconectado** de la PC.
2. Conectar los 4 hilos a la placa (negro primero).
3. Apretar y **mantener apretado el botón PROG** de la placa.
4. Sin soltar PROG, **enchufar el breakout USB a la PC**.
5. Contar 2 segundos. Soltar PROG.
6. En Windows: *Administrador de dispositivos → Puertos (COM y LPT)* debe
   aparecer un COM nuevo (`USB JTAG/serial debug unit` o `USB Serial Device`).
7. Arduino IDE → `Tools > Port` → ese COM nuevo.
8. `Sketch > Upload`. **No tocar la placa** durante el upload.
9. Esperar `Hash of data verified` + `Hard resetting`. Listo.
10. Para correr el firmware normal: desenchufar y reenchufar **sin** apretar PROG.

> Tiempo total una vez armado el arnés y con el entorno listo: **~2 minutos**.

### Drivers en Windows

- El C6 usa **USB-Serial/JTAG nativo** (no hay chip CP2102/CH340 en la placa).
- Windows 10/11 instala el driver CDC solo (`usbser.sys`). **No instalar
  CP210x ni CH340.**
- Si aparece como dispositivo desconocido: instalar el core esp32 3.2.2 (trae
  lo necesario) y/o el driver USB-JTAG de Espressif. Reintentar el paso 3–6.

### Plan robustez (opcional, máxima confiabilidad)

Si en el torneo el USB 5 V diera problemas de corriente (la placa pide hasta
500 mA y un USB de notebook flojo puede no darlo):

- Alimentar **VIN (U5_1) + GND (U5_4)** desde 5 V de una fuente USB con buena
  corriente (cargador/powerbank) o desde la batería 2S del robot (7.4 V, dentro
  del rango 5.3–25 V).
- Del breakout USB usar **solo D+ (U3_5), D− (U3_6) y GND** (GND común con la
  fuente). **NO** conectar VBUS del breakout en este caso (evitar doble
  alimentación).
- El resto de la secuencia (botón PROG) es igual.

Usar un **puerto USB directo de la notebook o un hub con alimentación** — nunca
un hub pasivo barato.

---

## 2. Preparación del entorno (una sola vez)

1. Instalar **Arduino IDE 2.x**.
2. `Tools > Board > Board Manager` → buscar `esp32` →
   instalar **`esp32 by Espressif Systems` versión EXACTA 3.2.2**.
   (El `update.md` oficial fija 3.2.2. C6 necesita core ≥ 3.0, pero usar la
   versión que el proyecto oficial probó.)
3. Clonar y posicionarse en el branch correcto:
   ```bash
   git clone https://github.com/robocup-junior/soccer-communication-module
   cd soccer-communication-module
   git checkout esp32-c6
   git rev-parse HEAD   # debe dar ffb4e3c1a1ddac2b3d3ed7bd8a24aacc19ea0081
   ```
4. Abrir `firmware/RCj_comm_module/RCj_comm_module.ino` en Arduino IDE.
5. `Tools > Board > esp32` → **`ESP32C6 Dev Module`**.

> Existe una copia local clonada por el coach en
> `C:\Users\violl\iitasoccer\_official_fw` (fuera del repo del equipo). Igual
> conviene re-clonar fresco y verificar el commit.

---

## 3. Conexión física (método oficial — `update.md` del branch `esp32-c6`)

**La placa COMM no tiene conector USB-C.** El USB nativo del C6 sale por el
header de 6 pines (`U3`). Hay que cablear un cable USB cortado o un breakout.

Mapear contra el pinout de la placa (ver
[`2026-05-17-placa-comm-componentes-y-circuito.md`](2026-05-17-placa-comm-componentes-y-circuito.md)):

| Cable USB | Pin de la placa COMM |
|-----------|----------------------|
| GND (negro) — **conectar PRIMERO, desconectar ÚLTIMO** | `U5_4` GND |
| VCC +5 V (rojo) | `U5_1` VIN |
| D+ (verde) | `U3_5` USB_D+ |
| D− (blanco) | `U3_6` USB_D− |

> ⚠️ **SIN GARANTÍA POR HARDWARE DESTRUIDO** (texto del `update.md` oficial).
> USB entrega 5 V; cablear mal puede dañar la placa o la PC. Usar breakout si
> no se está seguro. Verificar colores con multímetro (pueden variar).
> Si la PC no detecta el dispositivo en ~10 s, **intercambiar D+ y D−**.

4. Conectar el cable a la PC. Debe aparecer un puerto COM nuevo (el C6 expone
   `USB JTAG/serial debug unit` nativo — no requiere drivers extra).
5. `Tools > Port` → seleccionar el puerto nuevo.

---

## 4. Cargar el firmware

6. `Sketch > Upload`. No tocar la placa durante el upload.
7. Esperar `Hash of data verified` y `Hard resetting`.

### Fallback (si el upload no arranca — modo download manual)

El método oficial confía en que el USB-Serial/JTAG nativo del C6 entra solo a
modo download (con los defaults del board `ESP32C6 Dev Module` y `USB CDC On
Boot` habilitado). Si **no** entra:

**Opción A — botón PROG (strap boot GPIO9):**
1. Desconectar el USB de la placa (GND último).
2. Apretar y mantener **PROG** (lleva GPIO9 a GND).
3. Sin soltar, reconectar el USB (GND primero).
4. Esperar 2 s, soltar PROG.
5. `Tools > Port` → re-seleccionar el puerto que aparezca → `Upload`.

> Este truco es **no oficial** (no está en `update.md`); es un fallback
> razonable porque GPIO9 es el strap de boot del C6. La serigrafía de la fork
> IITA llama a ese botón `PROG`. Verificar con multímetro que apretar `PROG`
> lleva GPIO9 a 0 V antes de confiar en esto.

**Opción B — esptool directo (diagnóstico):**
```bash
pip install esptool
esptool.py --chip esp32c6 --port COMx --baud 115200 \
  --before no_reset --after no_reset flash_id
```
`--before no_reset` desactiva el toggle DTR/RTS (clave en USB nativo). Si
responde con tamaño de flash y MAC, el chip está vivo en bootloader; debe
reportar **4 MB** (confirma C6 **N4**).

---

## 5. Verificación E2E (criterios de aceptación medibles)

| # | Test | Criterio | Qué confirma |
|---|------|----------|--------------|
| 1 | Display OLED (si está conectado a `U4` I²C) | Logo RC + `v 0.91`, luego QR de la MAC + "wait for connection" | I²C SDA=6/SCL=7 OK, display vivo |
| 2 | Scan BLE desde celular | Aparece dispositivo **`RCJs-m_<MAC>`** (prefijo `RCJs-m_`) | BLE OK, branch C6 correcto |
| 3 | Serial Monitor @ **115200** | Tras conectar la app de árbitro y dar PLAY/STOP, imprime `PLAY` / `STOP` | FSM corriendo (el firmware SÍ imprime en runtime) |
| 4 | Multímetro en `U3_1` (OUT1, GPIO20) y `U3_2` (OUT2, GPIO19) | **3.3 V con PLAY, 0 V con STOP** | El robot recibirá start/stop correctamente |
| 5 | Botón CONNECT (GPIO18) hold 5 s | Display muestra desconexión BLE | Botón mapeado correcto |

> Corrección a doc previa: el firmware **sí** imprime `PLAY`/`STOP` por Serial
> @115200 en runtime (no en `setup()`). El Serial Monitor **es** una
> verificación válida, además del OLED y el BLE.

---

## 6. Notas / advertencias

- **No flashear `master` ni `esp32-c5`** sobre esta placa: mapean I²C/botones/
  outputs a pines equivocados → OLED en blanco, OUT1/OUT2 muertos, botones
  inertes. El robot **no recibiría START** = no compite.
- El firmware oficial v0.91 **no usa el acelerómetro** ni hace puente
  robot-a-robot (no hay ESP-NOW). El UART `RX_OUT/TX_OUT` del header es
  hardware pasivo que el firmware oficial no toca. Cualquier coordinación
  inter-robot vía esta placa es trabajo futuro (Hito 5 / post-mundial).
- Antes de confiar en OUT1=GPIO20 / OUT2=GPIO19 en cancha, **verificar
  físicamente** el pin (issue oficial #5: rotulación OUT1/OUT2 históricamente
  inconsistente). Ver tema-a-analizar P1 en el doc de circuito.

## 7. Escalación

Si Plan A + fallback + esptool fallan con 3V3 OK (3.20–3.40 V medido):
- Foro RCJ: https://junior.forum.robocup.org/t/documentation-communication-module/3269
- Issues del repo: https://github.com/robocup-junior/soccer-communication-module/issues
- Discord RCJ: https://discord.gg/45pxMQY4nJ

Adjuntar: foto de la placa, output completo de esptool, qué chip USB se enumera.

> **Todos los enlaces, la app de árbitro y el checklist offline para Incheon:**
> [`RECURSOS-Y-ENLACES.md`](RECURSOS-Y-ENLACES.md)

## 8. Fuentes

- `update.md` del branch `esp32-c6` (commit `e66b614`, 2025-10-30) del repo oficial.
- `firmware/RCj_comm_module/definitions.h` branch `esp32-c6` @ `ffb4e3c`.
- Clon local: `C:\Users\violl\iitasoccer\_official_fw`.
- Issue #5 (OUT1/OUT2): https://github.com/robocup-junior/soccer-communication-module/issues/5
- PR #6 (nombre BLE con MAC, merged): https://github.com/robocup-junior/soccer-communication-module/pull/6
