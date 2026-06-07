# esp32-bridge-firmware — ESP32 Telemetry Bridge v0

Firmware del ESP32 que recibe por UART el stream de telemetría JSON Lines del
Teensy (TOP/CENTRAL/DOWN) y lo reenvía por **WiFi UDP** al laptop, en ambas
direcciones (comandos del laptop vuelven por UART al Teensy).

> R&D 2027. NO es código de competencia. Compila standalone.

## Setup

### Hardware
- 1 ESP32 (DevKitC con WROOM-32, ó ESP32-C3 SuperMini, ó cualquier ESP32 con WiFi).
- 4 cables dupont al Teensy: GND, 3V3 (o 5V Vin), TX, RX.

### Software
- [PlatformIO](https://platformio.org/) (CLI o IDE).
- Arduino-ESP32 core (lo instala PlatformIO automáticamente).

### Pasos
1. **Copiar la plantilla de config**:
   ```bash
   cp include/config.example.h include/config.h
   ```
2. **Editar `include/config.h`** con tu SSID, password WiFi, IP/puerto destino,
   y (opcional) MAC del ESP32 del robot compañero para ESP-NOW.
3. **Compilar y flashear**:
   ```bash
   pio run -e esp32-bridge -t upload
   ```
4. **Monitorear** (opcional, para debug):
   ```bash
   pio device monitor -e esp32-bridge -b 115200
   ```

## Cableado (ejemplo CENTRAL con Serial2)

| Teensy 4.1 (CENTRAL) | ESP32-DevKitC |
|---|---|
| GND | GND |
| 5V (USB / batería con regulador) | Vin |
| pin 7 (Serial2 TX) | GPIO 16 (RX2) |
| pin 8 (Serial2 RX) | GPIO 17 (TX2) |

⚠️ **Verificar contra `hardware/electronics/mapa-pines-teensy-ambos-robots.md`
antes de soldar nada.** El UART libre puede variar.

## Modos

- **Default**: bridge UART↔UDP activo, ESP-NOW peer apagado.
- **Con `ESPNOW_ENABLE=1` en config.h**: además abre peer con el robot
  compañero (lista blanca de mensajes a reenviar).
- **Con `OTA_ENABLE=1` en config.h**: OTA habilitado para flashear inalámbrico
  con `pio run -e esp32-bridge-ota -t upload --upload-port <ip>`.

## Estados del LED (GPIO 2 — built-in del WROOM)

| Patrón | Significado |
|---|---|
| Apagado | Sin WiFi. |
| Parpadeo lento (1 Hz) | WiFi conectado, sin tráfico UART. |
| Parpadeo rápido (5 Hz) | Tráfico UART activo (datos viajando). |
| Encendido fijo | Error fatal (revisar Serial Monitor). |

## Limitaciones conocidas (v0)

- WiFi station mode (no AP). Necesita router/AP en el lugar.
- UDP broadcast → cualquier host en la subred recibe. No usar en redes
  públicas (filtra por IP en producción si hace falta).
- Sin autenticación de comandos. Quien tenga acceso a la red puede mandar
  `CAL CARPET`. **Solo entrenamiento, nunca partido.**

## Archivos

```
esp32-bridge-firmware/
├── README.md                    # este archivo
├── platformio.ini               # configuración de envs
├── include/
│   └── config.example.h         # plantilla (sin secretos)
└── src/
    └── main.cpp                 # firmware completo
```

`include/config.h` está en `.gitignore` — cada quien tiene su copia local.
