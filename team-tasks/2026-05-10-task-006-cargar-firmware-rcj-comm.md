---
id: TASK-006
title: "Cargar firmware oficial RCJ en la placa COMM"
date_created: 2026-05-10
date_updated: 2026-05-15
assigned: [mariaviollaz, elias]
priority: P0
status: pending
estimated_hours: 3
blocks: [Hito 5 — integración COMM, homologación Incheon]
tags: [firmware, comm-board, rcj, arbitros, esp32, esp32c6]
---

# TASK-006 — Cargar firmware oficial RCJ en placa COMM

## Resumen

La placa COMM (copia 100% del módulo oficial RCJ con ESP32 + OLED + acelerómetro + 2 botones) necesita firmware. Bajar el firmware oficial de RoboCupJunior, compilarlo y cargarlo. Verificar funcionamiento básico (display QR, botones, conexión WiFi a árbitros).

## Contexto

El módulo de comunicación con árbitros es **bloqueante para Incheon** (regla L1 documentada en `docs/internal/limitaciones-robot-marzo-2026.md`). Sin él, el robot no es homologable.

Como la placa fabricada es copia 100% del módulo oficial (dimensiones, hardware), el firmware oficial debería funcionar **tal cual** sin modificaciones. Confirmar esto primero. Después se evalúa si agregamos ESP-NOW para inter-robot (objetivo Hito 5 / post-mundial).

## Pasos concretos

### Procedure descubierto 2026-05-15 (post-investigación repo oficial)

**Resumen del hallazgo**: la placa COMM **no tiene botón RESET físico** — solo
BOOT (GPIO9) y CONNECT (GPIO10, software disconnect button del firmware). El
firmware oficial es **Arduino IDE puro** (no PlatformIO ni ESP-IDF), versión
v0.91 al 2026-05-15. El ESP32-C6 tiene **USB-Serial/JTAG nativo** por lo que el
auto-reset DTR/RTS de Arduino IDE **no funciona** — hay que entrar al bootloader
manualmente. Ver journal `2026-05-15-firmware-comm-c6-flash-procedure.md`.

### 1. Setup del entorno (una sola vez)

1. Instalar **Arduino IDE 2.x**.
2. Boards Manager → buscar `esp32` by Espressif Systems → **instalar versión ≥ 3.0.0**.
   La 2.x NO incluye soporte ESP32-C6. Si está la 2.x, actualizar.
3. Clonar el firmware oficial:
   ```
   git clone https://github.com/robocup-junior/soccer-communication-module
   ```
4. Abrir `firmware/RCj_comm_module/RCj_comm_module.ino` desde Arduino IDE.
5. Configurar Tools en Arduino IDE:
   - **Board**: `ESP32C6 Dev Module`
   - **USB CDC On Boot**: `Enabled` (crítico, la placa usa USB nativo del C6)
   - **JTAG Adapter**: `Disabled`
   - **Partition Scheme**: `Default 4MB with spiffs` (N4 = 4 MB)
   - **CPU Frequency**: `160 MHz`
   - **Flash Mode**: `QIO 80 MHz`
   - **Upload Speed**: `115200` (NO 921600 — falla en placas custom con USB nativo)

### 2. Entrar al bootloader (único procedure que funciona acá)

**Por qué este procedure**: el botón CONNECT NO es RESET — el firmware lo lee
como disconnect button BLE (hold 5s). La placa no tiene RESET físico. Sin DTR/RTS
útiles, la única forma de poner el chip en download mode es resetear vía
power-cycle con BOOT apretado.

6. **Desenchufar el USB** de la placa COMM.
7. Apretar y MANTENER apretado el botón **BOOT** (lleva GPIO9 a GND).
8. Sin soltar BOOT, **enchufar el USB-C**.
9. Esperar 2 segundos. Soltar BOOT.
10. En Administrador de dispositivos → Puertos (COM y LPT) debe aparecer un puerto
    nuevo estable. El nombre depende del path USB:
    - `USB JTAG/serial debug unit` → USB nativo del C6, no requiere drivers extra.
    - `CP210x` / `USB-SERIAL CH340` → puente externo, driver ya instalado.
    - Aparece y desaparece → saltar a "Diagnóstico hardware" (sección 4).

### 3. Cargar y probar

11. En Arduino IDE → **Upload**. NO tocar la placa durante el upload.
12. Esperar el mensaje `Hash of data verified` seguido de `Hard resetting`.
13. **Después del flash**: para correr el firmware normalmente, solo enchufar el USB
    (sin BOOT). Para reflashear, repetir pasos 6–9.
14. **Test 1 — Display OLED** (I2C en SDA=GPIO2, SCL=GPIO3): debe mostrar pantalla
    inicial con texto + indicador "--".
15. **Test 2 — Botón CONNECT** (GPIO10): hold 5s → `ble_disconnect()`. El display
    debe mostrar cambio de estado BLE.
16. **Test 3 — Bluetooth**: con un celular y app de scan BLE, confirmar que aparece
    el dispositivo `RCJ-soccer_module`.
17. **Test 4 — OUTPUT1/OUTPUT2** (GPIO9 / GPIO8): con multímetro, verificar que
    cambian al recibir start/stop del árbitro (requiere la app móvil del repo —
    `mobileAPP/`).

### 4. Plan B si Arduino IDE falla (esptool directo)

```
pip install esptool
esptool.py --chip esp32c6 --port COMx --baud 115200 --before no_reset --after no_reset flash_id
```

`--before no_reset` desactiva el toggle automático de DTR/RTS — clave para placas
con USB nativo. Si `flash_id` responde con tamaño y MAC, el chip está vivo en
modo bootloader. Reportar 4 MB confirma que es C6 N4.

### 5. Diagnóstico hardware si nada funciona

Con multímetro:
- **3V3 rail**: 3.20–3.40 V estable entre VCC del C6 y GND.
- **EN/CHIP_PU**: ~3.3 V en operación normal.
- **GPIO9**: HIGH normal (pull-up). Al apretar BOOT debe ir a 0 V.
- **GPIO12 (USB D-)** y **GPIO13 (USB D+)** deben llegar al conector USB-C.

Si el COM aparece y desaparece → corto en 3V3 o problema en diferencial USB.

### 6. Plan C — escalación a RCJ

Si Plan A y Plan B fallan después de verificar 3V3 OK:
- Foro: https://junior.forum.robocup.org/t/documentation-communication-module/3269
- Discord oficial RCJ: https://robocup-junior.github.io/soccer-rules/discord/

Adjuntar foto de la placa, output completo de esptool, qué chip USB se enumera.

### 7. Documentar

18. Actualizar este archivo en "Notas / decisiones" con resultados de cada test.
19. Si algo no funciona, crear nueva task para investigar.

### 8. Próximos pasos (futuro, NO en esta tarea)

- Evaluar si agregar ESP-NOW para inter-robot. Posibilidades:
  - (a) Modificar firmware oficial (riesgo: romper la certificación de homologación).
  - (b) App secundaria en el mismo ESP32 que se activa después del start/stop.
  - (c) Otro ESP32 separado solo para ESP-NOW.
- Esta decisión queda para Hito 5 / post-mundial.

## Criterio de cierre

- [ ] Firmware oficial RCJ v0.91 cargado en placa COMM con el procedure descubierto.
- [ ] Display OLED muestra pantalla inicial.
- [ ] Botón CONNECT (GPIO10) responde con `ble_disconnect()` al hold 5s.
- [ ] Bluetooth `RCJ-soccer_module` aparece visible desde un celular.
- [ ] OUTPUT1/OUTPUT2 cambian con start/stop desde la app móvil del repo.
- [ ] Journal entry actualizado con resultados de los tests.

## Notas / decisiones

### 2026-05-15 — Procedure de flash descubierto

Sesión de coach con investigación al repo oficial. Hallazgos clave:

1. **Firmware es Arduino IDE puro**, no PlatformIO. Versión actual v0.91.
2. **Botón CONNECT NO es RESET físico** — es un botón de aplicación BLE (GPIO10).
   La placa **no tiene RESET físico**. El procedure manual BOOT+CONNECT que
   intentaron antes nunca podía funcionar porque CONNECT no resetea el chip.
3. **ESP32-C6 usa USB-Serial/JTAG nativo** → DTR/RTS auto-reset de Arduino IDE
   falla. Hay que entrar al bootloader manualmente.
4. **Procedure correcto**: unplug → hold BOOT → plug USB → soltar BOOT.
5. Ningún `Serial.println` en `setup()` del firmware → la verificación NO es por
   Serial Monitor, es por **display OLED + scan BLE**.
6. Pinout del firmware (`definitions.h`):
   - I2C: SDA=2, SCL=3
   - BUTTON (CONNECT): GPIO10 (disconnect BLE, hold 5s)
   - BUTTON2 (PENALIZATION): GPIO7
   - OUTPUT1: GPIO9 (también pin BOOT del chip — dual use)
   - OUTPUT2: GPIO8
   - BLE name: `RCJ-soccer_module`

Ver journal `journal/2026-05-15-firmware-comm-c6-flash-procedure.md` con la
investigación completa, links a foro RCJ, repo oficial y diagnóstico paso a paso.

## Cambios de estado

- 2026-05-10: creado por Claude bajo requerimiento de Gustavo Viollaz.
- 2026-05-15: actualizado — prio P1 → P0 (era bloqueante por motivo equivocado en
  TASK-010 ahora cerrada); procedure de flash descubierto; verificación E2E
  ajustada de Serial Monitor a display+BLE.
