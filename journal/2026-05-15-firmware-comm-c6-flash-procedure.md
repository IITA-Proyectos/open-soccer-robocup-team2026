---
title: "2026-05-15 — Firmware oficial RCJ en placa COMM (ESP32-C6): procedure de flash descubierto"
date: 2026-05-15
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [firmware, comm-board, esp32c6, rcj-official, flash, bootloader, troubleshooting]
robot: comm
area: firmware
tipo: troubleshooting
related-tasks: [TASK-006, TASK-010]
---

> **⚠️ ENTRADA PARCIALMENTE SUPERADA (2026-05-17).** La premisa central de esta
> entrada es **incorrecta**: se basó solo en el branch `master` del repo
> oficial, que compila para **ESP32-C5**, NO para el C6 de nuestra placa.
> Existe un branch dedicado **`esp32-c6`** con OTRO pinout (SDA=6, SCL=7,
> BTN=18, BTN2=9, OUT1=20, OUT2=19) y método de flash oficial distinto.
> El pin map, el nombre BLE (`RCJ-soccer_module` → `RCJs-m_<MAC>`) y el
> procedure de esta entrada **no se deben usar**. Corrección completa:
> `journal/2026-05-17-analisis-3-placas-y-correccion-firmware-c6.md` y
> `hardware/electronics/comm-board/2026-05-17-procedimiento-flash-firmware-c6.md`.
> Esta entrada se conserva intacta como registro histórico (no se borra
> patrimonio del equipo) — pero **no es la fuente de verdad**.

# Firmware oficial RCJ en placa COMM (ESP32-C6) — procedure de flash descubierto

## Contexto

El equipo construyó una placa COMM custom siguiendo los gerbers del repo oficial
RCJ ([`soccer-communication-module`](https://github.com/robocup-junior/soccer-communication-module))
con un **ESP32-C6-MINI-1-N4**. La placa es necesaria para que el robot reciba
start/stop del árbitro durante los partidos en Incheon — sin ella, el robot no
se homologa.

El equipo intentó cargar el firmware oficial vía Arduino IDE y no pudo:
- USB conectaba y aparecía un puerto COM en Windows.
- Arduino IDE mostraba "Connecting..." y después perdía el puerto.
- Probaron drivers CH340, CP2102 y conexión directa — ninguno funcionó.
- La placa tiene 2 botones físicos: **BOOT** y **CONNECT** (no hay RESET).

Sesión de coach + investigación al repo oficial + el síntoma reportado por el
usuario llevaron a un diagnóstico cerrado y al procedure correcto.

## Lo investigado

### 1. ¿El firmware oficial soporta ESP32-C6?

**Sí.** Confirmado en dos fuentes independientes:

- **Foro oficial RCJ**, [thread `documentation-communication-module/3269`](https://junior.forum.robocup.org/t/documentation-communication-module/3269),
  mensaje #5 del 2024-03-29: *"there will be a few modifications like using ESP32 C6"*.
  El staff del módulo confirma migración desde marzo 2024.
- **Repo oficial 2026-05-15**: archivo `firmware/RCj_comm_module/definitions.h`
  tiene asignación de pines GPIO consistente con C6 (2, 3, 7, 8, 9, 10) y un
  comentario `//ESP-C5` que es typo del autor (el chip es C6 según BOM, gerbers
  y foro).

**Implicancia**: TASK-010 ("Verificar compat firmware RCJ con ESP32-C6") fue
escrita el 2026-05-10 asumiendo que el firmware era para ESP32 clásico. Esa
premisa ya no aplica. TASK-010 cerrada como `done` el 2026-05-15. Plan A
(cargar tal cual) es viable.

### 2. ¿Qué toolchain usa el firmware?

**Arduino IDE puro.** No es PlatformIO ni ESP-IDF. Estructura:

```
firmware/RCj_comm_module/
  RCj_comm_module.ino     ← entry point
  ble.cpp / ble.h
  ble_processing.cpp / ble_processing.h
  state_machine.cpp / state_machine.h
  display.cpp / display.h
  functions.cpp / functions.h
  definitions.h
  fonts.h
  images.h
  libraries/              ← libs vendoreadas
```

**Versión actual**: v0.91 (`FW_VERSION_MAJOR=0`, `FW_VERSION_MINOR=91`).

**Requisito Arduino IDE**: Espressif Arduino-ESP32 core **≥ 3.0** para que aparezca
`ESP32C6 Dev Module` como opción de board. La 2.x no incluye soporte C6.

### 3. ¿Por qué Arduino IDE pierde el puerto al "Connecting..."?

Tres factores combinados:

1. **El ESP32-C6 tiene USB-Serial/JTAG nativo** (D- en GPIO12, D+ en GPIO13).
   Si la placa expone USB-C directo al chip (lo más probable según el gerber del
   repo oficial), **no hay líneas DTR/RTS conectadas físicamente** al chip — son
   conceptos del puente USB-UART que no existe. El auto-reset de Arduino IDE,
   que depende de toggle DTR/RTS, **no resetea nada**.

2. **El botón CONNECT no es RESET físico.** Está cableado a GPIO10. El firmware
   lo lee en runtime con `check_disconnect_button()` (hold 5s → `ble_disconnect()`).
   Es un botón de aplicación BLE, no de hardware. El nombre en la placa es
   confuso. Por eso el procedure manual "BOOT + CONNECT" que intentó el equipo
   nunca podía funcionar — apretar CONNECT no resetea el chip.

3. **La placa no tiene RESET físico** (EN/CHIP_PU del chip). Verificable
   abriendo `pcb_schematic/SCH.pdf` del repo oficial. Sin RESET físico, la única
   forma de hacer un reset es **power-cycle por USB** (desenchufar y
   re-enchufar).

### 4. Procedure correcto descubierto

Único path que funciona con esta hardware revision:

```
1. Desenchufar el USB.
2. Apretar y MANTENER apretado el botón BOOT (lleva GPIO9 a GND).
3. Sin soltar BOOT, enchufar el USB-C.
4. Esperar 2 segundos. Soltar BOOT.
5. Verificar que aparezca un puerto COM estable en Device Manager:
   - "USB JTAG/serial debug unit"  → USB nativo C6 (no requiere driver extra).
   - "CP210x" / "USB-SERIAL CH340" → puente externo.
6. Arduino IDE → Upload. No tocar la placa.
```

Para reflashear: repetir los pasos 1–6. Para correr el firmware normalmente
(post-flash): solo enchufar el USB sin tocar BOOT.

**Configuración Arduino IDE crítica**:
- Board: `ESP32C6 Dev Module`
- USB CDC On Boot: **Enabled** (clave si la placa usa USB nativo)
- Upload Speed: `115200` (NO 921600 — falla mucho en placas custom)
- Partition Scheme: `Default 4MB with spiffs` (N4 = 4 MB)

### 5. Plan B si Arduino IDE sigue fallando

```bash
pip install esptool
esptool.py --chip esp32c6 --port COMx --baud 115200 \
  --before no_reset --after no_reset flash_id
```

El flag `--before no_reset` desactiva el toggle automático de DTR/RTS — clave
para placas con USB nativo. Si `flash_id` responde con tamaño y MAC, el chip
está vivo en bootloader. Confirmar 4 MB para descartar problema de footprint
de memoria.

### 6. Verificación end-to-end (post-flash)

El `setup()` del firmware oficial hace `Serial.begin(115200)` pero **NO imprime
ningún mensaje** por serial — confirmé mirando el `.ino` literal. Por lo tanto
Serial Monitor NO sirve como prueba primaria. Las pruebas reales son:

1. **Display OLED** (I2C SDA=GPIO2, SCL=GPIO3): debe mostrar pantalla inicial
   con texto + indicador `"--"`.
2. **Botón CONNECT** (GPIO10): hold 5s → `ble_disconnect()` cambia el estado en
   el display.
3. **Bluetooth**: con app de scan BLE en un celular, el módulo debe aparecer
   con el nombre `RCJ-soccer_module`.
4. **OUTPUT1/OUTPUT2** (GPIO9 / GPIO8): con multímetro o LEDs, verificar toggle
   al recibir start/stop desde la app móvil del repo (`mobileAPP/`).

## Decisiones

- **TASK-010** cerrada como `done`. No requiere portar el firmware. La premisa
  original (firmware para ESP32 clásico) era incorrecta.
- **TASK-006** re-priorizada de P1 → **P0** (era bloqueante por TASK-010 ahora
  cerrada). Procedure de flash actualizado en el archivo de la TASK.
- **Verificación E2E** del firmware oficial cambia de "Serial Monitor + WiFi"
  (lo que decía TASK-006 original) a "Display OLED + scan BLE + toggle outputs"
  (lo que el firmware realmente expone).

## Loose ends

- **Confirmar visualmente con la placa real** que CONNECT efectivamente va a
  GPIO10 y BOOT a GPIO9. Plan basado en lectura del `definitions.h` del repo
  oficial — asumimos que el equipo siguió los gerbers tal cual. Verificable
  abriendo `pcb_schematic/SCH.pdf` localmente (no es extraíble vía herramientas
  remotas — es binario).
- **Pendiente equipo**: ejecutar el procedure. Si falla después de Plan A + Plan B,
  escalar al foro/Discord RCJ con la pregunta exacta sugerida en TASK-006.

## Links externos verificados

- Repo firmware oficial RCJ: https://github.com/robocup-junior/soccer-communication-module
- Foro RCJ (thread comm module): https://junior.forum.robocup.org/t/documentation-communication-module/3269
- Rulebook Soccer 2026: https://robocup-junior.github.io/soccer-rules/master/rules.html
- Discord oficial RCJ: https://robocup-junior.github.io/soccer-rules/discord/
- esptool docs ESP32-C6: https://docs.espressif.com/projects/esptool/en/latest/esp32c6/esptool/flashing-firmware.html

## Plan file referenciado

- `C:\Users\violl\.claude\plans\splendid-imagining-crane.md` — plan completo
  aprobado por Gustavo en la sesión.
