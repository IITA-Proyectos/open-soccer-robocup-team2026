---
title: "Placa COMM — Recursos, enlaces RCJ y checklist offline para Incheon"
date: 2026-05-17
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [comm-board, rcj, enlaces, recursos, incheon, checklist]
robot: ambos
area: comunicacion
tipo: referencia
---

# Placa COMM — Recursos y enlaces RCJ (todo en un solo lugar)

> Pedido del equipo: tener TODOS los links y software en el repo, para no
> buscarlos a las apuradas en Incheon. Bajar lo marcado **OFFLINE** antes de viajar.

## 1. Repos oficiales RoboCupJunior

| Qué | Repo / URL | Notas |
|-----|-----------|-------|
| **Firmware del módulo** | https://github.com/robocup-junior/soccer-communication-module | **Branch `esp32-c6`** (NO `master`, que es C5). Commit ref: `ffb4e3c`. |
| **App de árbitro (Android)** | https://github.com/robocup-junior/soccer-referee-app | "RCJ Soccer RefMate". APK en *Releases* → **v0.9.6** (`RCJ_Soccer_RefMate.apk`). v0.9.7 no tiene APK adjunto. |
| Reglas Soccer 2026 | https://github.com/robocup-junior/soccer-rules | |
| Recursos curados | https://github.com/robocup-junior/awesome-rcj-soccer | |
| Fork IITA (solo fabricación) | https://github.com/IITA-Proyectos/rcj-soccer-open_communication_module | Gerbers/BOM; el firmware NO está acá. |

## 2. Soporte / comunidad

- Foro RCJ Soccer: https://junior.forum.robocup.org/c/robocupjunior-soccer/5
- Thread del módulo: https://junior.forum.robocup.org/t/documentation-communication-module/3269
- Issues del firmware: https://github.com/robocup-junior/soccer-communication-module/issues
- Discord RCJ: https://discord.gg/45pxMQY4nJ
- Issue OUT1/OUT2 (rotulación inconsistente, verificar con multímetro): https://github.com/robocup-junior/soccer-communication-module/issues/5

## 3. Software para flashear (PC Windows)

| Software | De dónde | Versión |
|----------|----------|---------|
| Arduino IDE 2.x | https://www.arduino.cc/en/software | última |
| Core ESP32 (Espressif) | Boards Manager → `esp32 by Espressif Systems` | **3.2.2 exacta** |
| URL boards manager | `https://espressif.github.io/arduino-esp32/package_esp32_index.json` | en Preferences |

Librerías que el firmware necesita (vienen DENTRO del repo del firmware en
`firmware/RCj_comm_module/libraries/`, hay que copiarlas a la carpeta global
de Arduino — en esta PC: `C:\Users\violl\OneDrive\Documentos\Arduino\libraries\`):

- `ESP8266_and_ESP32_OLED_driver_for_SSD1306_displays` (trae `OLEDDisplay.h`, `SSD1306.h`)
- `QRcodeDisplay` (`qrcodedisplay.h`)
- `QRcodeOled-2.0.0` (`qrcodeoled.h`)

## 4. App RefMate — cómo emparejar el módulo

La app NO se conecta desde el Bluetooth del celular ni un scanner genérico.
Dentro de **RCJ Soccer RefMate**:

1. **Mantener presionado** un botón de robot (A1, A2, …).
2. Emparejar por: **BLE scan** (elegir `RCJs-m_<MAC>`) **o** escanear el **QR**
   del OLED del módulo.
3. Sin OLED conectado → usar **BLE scan** dentro de la app.

El módulo se anuncia como **`RCJs-m_<MAC>`** (prefijo `RCJs-m_`). El nombre
viejo `RCJ-soccer_module` es del firmware C5 — no aplica.

## 5. Checklist OFFLINE antes de viajar a Incheon

Bajar y guardar en un pendrive + en la notebook (sin depender de internet allá):

- [ ] Instalador Arduino IDE 2.x (`.exe` Windows).
- [ ] Core ESP32 3.2.2 ya instalado y probado en la notebook.
- [ ] Repo firmware clonado en branch `esp32-c6` (local: `C:\Users\violl\iitasoccer\_official_fw`).
- [ ] Las 3 librerías ya copiadas a la carpeta de Arduino (ver §3).
- [ ] `RCJ_Soccer_RefMate.apk` (v0.9.6) descargado y **ya instalado** en el celular del equipo.
- [ ] Arnés USB→4 dupont armado y etiquetado (ver procedimiento de flash).
- [ ] Probado el flasheo completo end-to-end ANTES de viajar (no estrenar en cancha).
- [ ] Foto/diagrama plastificado del pinout (`GND BAT … D- D+`) y de qué botón es PROG.

## 6. Documentos relacionados en este repo

- Componentes y circuito: [`2026-05-17-placa-comm-componentes-y-circuito.md`](2026-05-17-placa-comm-componentes-y-circuito.md)
- Procedimiento de flash: [`2026-05-17-procedimiento-flash-firmware-c6.md`](2026-05-17-procedimiento-flash-firmware-c6.md)
- Análisis y corrección: `journal/2026-05-17-analisis-3-placas-y-correccion-firmware-c6.md`
- Tarea operativa: `team-tasks/2026-05-10-task-006-cargar-firmware-rcj-comm.md`
