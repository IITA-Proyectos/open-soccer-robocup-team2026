# Comunicación

Protocolos y código de comunicación entre componentes.

## Comunicación interna (dentro del robot)

- UART entre OpenMV y Teensy: formato de paquetes de datos

## Módulo de comunicación con árbitros (placa COMM — ESP32-C6)

Placa COMM = fork IITA del módulo oficial RCJ, MCU **ESP32-C6-MINI-1-N4**.
Documentación autoritativa (analizada del paquete de fab 2026-04-20):

- **Componentes + circuito + pinout exacto:**
  [`hardware/electronics/comm-board/2026-05-17-placa-comm-componentes-y-circuito.md`](../../hardware/electronics/comm-board/2026-05-17-placa-comm-componentes-y-circuito.md)
- **Procedimiento de flash (firmware oficial branch `esp32-c6`):**
  [`hardware/electronics/comm-board/2026-05-17-procedimiento-flash-firmware-c6.md`](../../hardware/electronics/comm-board/2026-05-17-procedimiento-flash-firmware-c6.md)

Start/stop del árbitro = nivel en OUT1/OUT2 (no UART); el árbitro habla por BLE.

## Comunicación entre robots

_No implementada en el firmware oficial v0.91 (sin ESP-NOW). Trabajo futuro
Hito 5 / post-mundial — ver TASK-006 sección "próximos pasos"._

## Referencia 2025

Ver `software/vision/` para el protocolo UART usado.
