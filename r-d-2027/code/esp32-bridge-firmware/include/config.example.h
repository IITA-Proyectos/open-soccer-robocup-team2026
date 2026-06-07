// config.example.h — PLANTILLA del ESP32 Telemetry Bridge.
//
// Copiar este archivo a `config.h` (mismo directorio) y editarlo con TUS
// credenciales. `config.h` está en `.gitignore` y NO se commitea.
//
//     cp include/config.example.h include/config.h
//     (editar include/config.h)
//
// Nunca commitear contraseñas, MACs personales ni IPs internas.

#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// WiFi
// ─────────────────────────────────────────────────────────────────────────────
#define WIFI_SSID         "TU_SSID_AQUI"
#define WIFI_PASSWORD     "TU_PASSWORD_AQUI"
#define WIFI_HOSTNAME     "iita-bridge-1"        // que aparezca en el router

// Tiempo máximo para asociarse al boot (ms). Si vence, sigue intentando en background.
#define WIFI_CONNECT_TIMEOUT_MS  10000

// ─────────────────────────────────────────────────────────────────────────────
// UDP — telemetría OUT (ESP32 → laptop)
// ─────────────────────────────────────────────────────────────────────────────

// Dirección destino. Para broadcast en la subred: "255.255.255.255".
// Para apuntar a un laptop específico: "192.168.1.50".
#define UDP_OUT_TARGET    "255.255.255.255"
#define UDP_OUT_PORT      8765

// ─────────────────────────────────────────────────────────────────────────────
// UDP — comandos IN (laptop → ESP32 → UART → Teensy)
// ─────────────────────────────────────────────────────────────────────────────

// Puerto donde el ESP32 escucha comandos UDP de la app PC.
#define UDP_IN_PORT       8764

// Solo aceptar comandos desde estas IPs (lista blanca). Vacío = aceptar todo.
// Ejemplo: { "192.168.1.50" }. Define hasta UDP_IN_ALLOW_MAX_COUNT entradas.
#define UDP_IN_ALLOW_MAX_COUNT 4
static const char* const UDP_IN_ALLOW_LIST[] = {
    // "192.168.1.50",
    // "192.168.1.51",
};
static constexpr int UDP_IN_ALLOW_COUNT =
    sizeof(UDP_IN_ALLOW_LIST) / sizeof(UDP_IN_ALLOW_LIST[0]);

// ─────────────────────────────────────────────────────────────────────────────
// UART al Teensy
// ─────────────────────────────────────────────────────────────────────────────
#define UART_BAUD         115200
#define UART_RX_PIN       16     // GPIO del ESP32 que recibe del Teensy TX
#define UART_TX_PIN       17     // GPIO del ESP32 que transmite al Teensy RX
#define UART_BUF_SIZE     4096   // buffer software para evitar pérdidas

// ─────────────────────────────────────────────────────────────────────────────
// ESP-NOW (inter-robot) — APAGADO por default (FASE A entrega solo telemetría)
// ─────────────────────────────────────────────────────────────────────────────
#define ESPNOW_ENABLE     0    // 0 = off, 1 = on

// MAC del ESP32 gateway del robot compañero. Sin entradas si solo hay 1 robot.
// Encontrar la MAC del peer: el peer la imprime al boot.
#define ESPNOW_PEER_MAC   { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }   // ej: 24:6F:28:...

// Tasa nominal de envío de PartnerSnapshot al peer (Hz).
#define ESPNOW_TX_HZ      20

// ─────────────────────────────────────────────────────────────────────────────
// OTA (flasheo inalámbrico) — APAGADO por default
// ─────────────────────────────────────────────────────────────────────────────
#define OTA_ENABLE        0
#define OTA_PASSWORD      "iita2027"   // cambiar antes de usar

// ─────────────────────────────────────────────────────────────────────────────
// LED de diagnóstico
// ─────────────────────────────────────────────────────────────────────────────
#define LED_PIN           2     // GPIO 2 = built-in en WROOM-32 DevKitC.
                                // Para C3 SuperMini cambiar a 8.
#define LED_ACTIVE_LOW    0     // 0 si el LED se enciende con HIGH; 1 si con LOW.

// ─────────────────────────────────────────────────────────────────────────────
// Stats / debug por Serial0 (USB del ESP32)
// ─────────────────────────────────────────────────────────────────────────────
#define SERIAL0_DEBUG      1     // 1 = imprime stats periódicas por USB
#define SERIAL0_BAUD       115200
#define SERIAL0_STATS_MS   2000  // cada cuánto imprime stats
