---
id: TASK-006
title: "Cargar firmware oficial RCJ en la placa COMM"
date_created: 2026-05-10
date_updated: 2026-06-02
assigned: [mariaviollaz, elias]
priority: P0
status: done
estimated_hours: 3
blocks: [Hito 5 — integración COMM, homologación Incheon]
tags: [firmware, comm-board, rcj, arbitros, esp32, esp32c6]
---

> **✅ CERRADA 2026-06-02 — COMM FLASHEADA.** Gustavo confirmó que la placa
> COMM ya tiene el firmware oficial RCJ cargado (flasheada "hace rato"). Como
> es el humano que tiene la placa en la mano, su confirmación cierra esta TASK
> de hardware. **El flasheo del módulo ya NO es bloqueante para Incheon.**
>
> **Lo que SÍ queda pendiente** (NO es parte de esta TASK, se movió a su
> propia tarea): verificar a qué pines del Teensy del TOP llegan las salidas
> **OUT_1/OUT_2** del árbitro y adaptar el firmware del TOP para leerlas. Hoy
> el TOP escucha el árbitro por UART, pero la COMM lo entrega como NIVEL de
> tensión → **el START/STOP no le llega al Teensy todavía**. Eso lo trackea
> **TASK-204** (`2026-06-02-task-204-verificar-pines-arbitro-comm-top.md`) +
> journal `2026-06-02-arbitro-gap-y-ultrasonido-top.md`.

# TASK-006 — Cargar firmware oficial RCJ en placa COMM

> **⚠️ CORRECCIÓN 2026-05-17 — LEER ANTES DE EJECUTAR.**
> El "Procedure descubierto 2026-05-15" de abajo y el pinout en "Notas /
> decisiones" están **EQUIVOCADOS**: se basaron en el branch `master` (que
> compila para **ESP32-C5**, no el C6 de nuestra placa). El firmware correcto
> está en el branch **`esp32-c6`** (commit `ffb4e3c`), core Arduino-ESP32
> **exactamente 3.2.2**, pin map C6 (SDA=6, SCL=7, BTN=18, BTN2=9, OUT1=20,
> OUT2=19), nombre BLE **`RCJs-m_<MAC>`**, y método de flash oficial =
> cablear USB a los pines del header (no hay USB-C en la placa).
>
> **Fuente de verdad (seguir ESTO, no las secciones viejas):**
> - Procedimiento: [`hardware/electronics/comm-board/2026-05-17-procedimiento-flash-firmware-c6.md`](../hardware/electronics/comm-board/2026-05-17-procedimiento-flash-firmware-c6.md)
> - Componentes/pinout: [`hardware/electronics/comm-board/2026-05-17-placa-comm-componentes-y-circuito.md`](../hardware/electronics/comm-board/2026-05-17-placa-comm-componentes-y-circuito.md)
> - Análisis: `journal/2026-05-17-analisis-3-placas-y-correccion-firmware-c6.md`
>
> Las secciones originales se conservan abajo como registro histórico (no se
> borra patrimonio del equipo) pero **no se deben usar como guía**.

> **📁 Repo oficial YA está clonado localmente (2026-05-19)** en
> `C:\Users\violl\iitasoccer\_official_fw\` (al lado de este repo), branch
> `esp32-c6` ya checkout-eado en el commit correcto `ffb4e3c`.
> **NO hace falta `git clone` ni cambiar de branch.** Solo abrir Arduino IDE
> en:
> `C:\Users\violl\iitasoccer\_official_fw\firmware\RCj_comm_module\RCj_comm_module.ino`
> y seguir los pasos del procedimiento del 2026-05-17 (link arriba).

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

> Criterio de cierre **corregido 2026-05-17** (pines/branch/BLE actualizados):

- [ ] Firmware oficial v0.91 del branch **`esp32-c6`** (commit `ffb4e3c`)
      cargado siguiendo el procedimiento de `hardware/electronics/comm-board/`.
- [ ] Display OLED (en I²C `U4`) muestra logo RC + `v 0.91` + QR de la MAC.
- [ ] Botón CONNECT (**GPIO18**) responde con `ble_disconnect()` al hold 5 s.
- [ ] Bluetooth **`RCJs-m_<MAC>`** (prefijo `RCJs-m_`) visible desde un celular.
- [ ] OUTPUT1 (**GPIO20**, pin `U3_1`) / OUTPUT2 (**GPIO19**, `U3_2`) miden
      3.3 V con PLAY y 0 V con STOP desde la app de árbitro.
- [ ] Serial Monitor @115200 imprime `PLAY`/`STOP` en runtime.
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
- 2026-05-17: **corregido** — la investigación del 2026-05-15 solo miró el
  branch `master` (C5). Se descubrió el branch `esp32-c6` (commit `ffb4e3c`)
  con pin map distinto (SDA=6/SCL=7/BTN=18/BTN2=9/OUT1=20/OUT2=19), core
  Arduino-ESP32 fijado en 3.2.2, nombre BLE `RCJs-m_<MAC>`, y método de flash
  oficial (cablear USB al header). Pin map/branch/procedure viejos marcados
  como históricos. Fuente de verdad movida a
  `hardware/electronics/comm-board/`. Análisis completo en journal 2026-05-17.
  Status sigue `pending` (falta ejecutar el flasheo con el procedure correcto).
- 2026-06-02: **CERRADA (done)** — Gustavo confirmó que la COMM ya está
  flasheada con el firmware oficial RCJ. Cierre ordenado por el humano que
  tiene la placa (a pedido suyo en esta sesión). El único pendiente del
  árbitro pasa a **TASK-204** (verificar pines OUT_1/OUT_2 → Teensy del TOP
  + adaptar `comm_arbiter.cpp` a leer niveles). Ver journal 2026-06-02.
