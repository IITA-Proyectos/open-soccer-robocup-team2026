// ESP32 Telemetry Bridge v0 — UART ↔ WiFi (UDP).
//
// Lee líneas (JSON Lines) por UART desde el Teensy y las reenvía por UDP al
// laptop. Recibe comandos por UDP del laptop y los reenvía por UART al Teensy.
// Stub opcional de ESP-NOW para inter-robot (apagado por default).
//
// Diseño:
//   - Loop cooperativo, no bloqueante (delay máximo 1 ms).
//   - Buffer software de UART para no perder bursts del Teensy.
//   - Reconexión automática a WiFi si se cae.
//   - LED de diagnóstico (apagado/lento/rápido/fijo según estado).
//
// Ver r-d-2027/specs/esp32-telemetry-bridge-v0-spec.md para la descripción
// completa.

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "config.h"

#if ESPNOW_ENABLE
  #include <esp_now.h>
  #include <esp_wifi.h>
#endif

#if OTA_ENABLE
  #include <ArduinoOTA.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Estado global
// ─────────────────────────────────────────────────────────────────────────────
static WiFiUDP g_udp_out;     // socket que envía al laptop
static WiFiUDP g_udp_in;      // socket que escucha comandos del laptop
static IPAddress g_target_ip; // resuelve UDP_OUT_TARGET → IP al boot

static HardwareSerial& g_uart = Serial1;  // alias claro

// UART RX state machine (acumula caracteres hasta '\n').
static char     g_rx_line[UART_BUF_SIZE];
static size_t   g_rx_len = 0;

// UDP IN state machine (idem para comandos del laptop).
static char     g_cmd_line[UART_BUF_SIZE];

// Stats
struct Stats {
    uint32_t uart_lines_in   = 0;
    uint32_t udp_out_packets = 0;
    uint32_t udp_in_packets  = 0;
    uint32_t uart_lines_out  = 0;
    uint32_t udp_in_dropped  = 0;   // por filtro o por sobre-largo
    uint32_t wifi_reconnects = 0;
};
static Stats g_stats;
static uint32_t g_last_stats_print_ms = 0;
static uint32_t g_last_traffic_ms = 0;

// ─────────────────────────────────────────────────────────────────────────────
// LED helpers
// ─────────────────────────────────────────────────────────────────────────────
static inline void led_set(bool on) {
    if (LED_ACTIVE_LOW) digitalWrite(LED_PIN, on ? LOW : HIGH);
    else                digitalWrite(LED_PIN, on ? HIGH : LOW);
}

static void led_tick(uint32_t now) {
    // Patrones:
    //   sin WiFi:         apagado
    //   WiFi sin tráfico: parpadeo lento (1 Hz)
    //   tráfico reciente: parpadeo rápido (5 Hz)
    if (WiFi.status() != WL_CONNECTED) { led_set(false); return; }

    const uint32_t since_traffic = now - g_last_traffic_ms;
    if (since_traffic < 500) {
        // 5 Hz: 100 ms on / 100 ms off
        led_set(((now / 100) % 2) == 0);
    } else {
        // 1 Hz: 500 ms on / 500 ms off
        led_set(((now / 500) % 2) == 0);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// WiFi
// ─────────────────────────────────────────────────────────────────────────────
static void wifi_begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(WIFI_HOSTNAME);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.printf("[wifi] connecting to '%s' ...\n", WIFI_SSID);
    const uint32_t deadline = millis() + WIFI_CONNECT_TIMEOUT_MS;
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
        delay(100);
        Serial.print('.');
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[wifi] ok, ip=%s, mac=%s\n",
                      WiFi.localIP().toString().c_str(),
                      WiFi.macAddress().c_str());
    } else {
        Serial.println("[wifi] timeout — sigue intentando en background.");
    }
}

static bool g_was_connected = false;
static void wifi_watchdog(uint32_t now) {
    const bool conn = (WiFi.status() == WL_CONNECTED);
    if (conn != g_was_connected) {
        g_was_connected = conn;
        if (conn) {
            Serial.printf("[wifi] reconectado, ip=%s\n",
                          WiFi.localIP().toString().c_str());
            g_udp_in.stop();
            g_udp_in.begin(UDP_IN_PORT);
            g_target_ip.fromString(UDP_OUT_TARGET);
        } else {
            Serial.println("[wifi] desconectado");
            g_stats.wifi_reconnects++;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// UDP IN — filtro de IP de origen
// ─────────────────────────────────────────────────────────────────────────────
static bool udp_in_ip_allowed(const IPAddress& ip) {
    if (UDP_IN_ALLOW_COUNT == 0) return true;  // lista vacía = aceptar todo
    char buf[16];
    snprintf(buf, sizeof buf, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    for (int i = 0; i < UDP_IN_ALLOW_COUNT; ++i) {
        if (strcmp(buf, UDP_IN_ALLOW_LIST[i]) == 0) return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pump 1 — UART → UDP OUT
//
// Acumula chars en g_rx_line hasta encontrar '\n'. Cuando completa una línea,
// la envía como un único datagrama UDP (incluyendo el '\n' final si conviene
// — acá lo recorto antes para que el receptor pueda usarlo como separador).
// ─────────────────────────────────────────────────────────────────────────────
static void pump_uart_to_udp(uint32_t now) {
    while (g_uart.available()) {
        int c = g_uart.read();
        if (c < 0) break;
        if (c == '\r') continue;  // ignorar CR
        if (c == '\n') {
            // línea completa
            if (g_rx_len > 0) {
                if (WiFi.status() == WL_CONNECTED) {
                    g_udp_out.beginPacket(g_target_ip, UDP_OUT_PORT);
                    g_udp_out.write(reinterpret_cast<const uint8_t*>(g_rx_line), g_rx_len);
                    g_udp_out.endPacket();
                    g_stats.udp_out_packets++;
                    g_last_traffic_ms = now;
                }
                g_stats.uart_lines_in++;
            }
            g_rx_len = 0;
            continue;
        }
        if (g_rx_len + 1 < sizeof g_rx_line) {
            g_rx_line[g_rx_len++] = static_cast<char>(c);
        } else {
            // línea más larga que el buffer → descartar y reportar
            g_rx_len = 0;
            g_stats.udp_in_dropped++;
            Serial.println("[uart] línea > buf, descartada");
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Pump 2 — UDP IN → UART
// ─────────────────────────────────────────────────────────────────────────────
static void pump_udp_to_uart(uint32_t now) {
    int size = g_udp_in.parsePacket();
    if (size <= 0) return;
    IPAddress src = g_udp_in.remoteIP();
    if (!udp_in_ip_allowed(src)) {
        // drenar y descartar
        while (g_udp_in.available()) g_udp_in.read();
        g_stats.udp_in_dropped++;
        return;
    }
    if (size >= static_cast<int>(sizeof g_cmd_line)) {
        while (g_udp_in.available()) g_udp_in.read();
        g_stats.udp_in_dropped++;
        Serial.println("[udp] cmd > buf, descartado");
        return;
    }
    int n = g_udp_in.read(reinterpret_cast<uint8_t*>(g_cmd_line), size);
    if (n <= 0) return;
    g_cmd_line[n] = '\0';
    // Asegurar terminador en \n para que el firmware del Teensy parsee línea.
    g_uart.write(reinterpret_cast<const uint8_t*>(g_cmd_line), n);
    if (n > 0 && g_cmd_line[n - 1] != '\n') g_uart.write('\n');
    g_stats.udp_in_packets++;
    g_stats.uart_lines_out++;
    g_last_traffic_ms = now;
}

// ─────────────────────────────────────────────────────────────────────────────
// ESP-NOW stub (FASE A no envía nada por default)
// ─────────────────────────────────────────────────────────────────────────────
#if ESPNOW_ENABLE

typedef struct __attribute__((packed)) {
    int16_t  my_x_mm, my_y_mm, my_heading_centideg;
    int16_t  ball_x_mm, ball_y_mm;
    int16_t  ball_vx_mm_s, ball_vy_mm_s;
    uint8_t  ball_confidence;
    uint8_t  fsm_intent;
    uint32_t timestamp_ms;
} PartnerSnapshot;

static const uint8_t g_peer_mac[6] = ESPNOW_PEER_MAC;
static uint32_t g_last_espnow_tx_ms = 0;

static void on_espnow_recv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != sizeof(PartnerSnapshot)) return;
    // FASE A: solo reenvía al UART por si el Teensy quiere mirar.
    PartnerSnapshot p;
    memcpy(&p, data, sizeof p);
    char line[200];
    int n = snprintf(line, sizeof line,
                     "{\"v\":1,\"src\":\"partner\",\"x\":%d,\"y\":%d,\"h\":%d,"
                     "\"bx\":%d,\"by\":%d,\"bvx\":%d,\"bvy\":%d,\"bc\":%u,"
                     "\"fsm\":%u,\"ts\":%u}\n",
                     p.my_x_mm, p.my_y_mm, p.my_heading_centideg,
                     p.ball_x_mm, p.ball_y_mm, p.ball_vx_mm_s, p.ball_vy_mm_s,
                     p.ball_confidence, p.fsm_intent, p.timestamp_ms);
    if (n > 0) g_uart.write(reinterpret_cast<const uint8_t*>(line), n);
}

static void espnow_setup() {
    if (esp_now_init() != ESP_OK) {
        Serial.println("[espnow] init FAIL");
        return;
    }
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, g_peer_mac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[espnow] add_peer FAIL");
        return;
    }
    esp_now_register_recv_cb(on_espnow_recv);
    Serial.println("[espnow] peer registrado");
}

static void espnow_tick(uint32_t now) {
    // FASE A: cero envío. La lista blanca se llena cuando se decida compartir.
    (void)now;
}

#endif  // ESPNOW_ENABLE

// ─────────────────────────────────────────────────────────────────────────────
// Stats periódicas por USB
// ─────────────────────────────────────────────────────────────────────────────
static void print_stats(uint32_t now) {
    if (!SERIAL0_DEBUG) return;
    if (now - g_last_stats_print_ms < SERIAL0_STATS_MS) return;
    g_last_stats_print_ms = now;
    Serial.printf("[stats] uart_in=%u udp_out=%u | udp_in=%u uart_out=%u | "
                  "dropped=%u wifi_re=%u rssi=%d\n",
                  g_stats.uart_lines_in, g_stats.udp_out_packets,
                  g_stats.udp_in_packets, g_stats.uart_lines_out,
                  g_stats.udp_in_dropped, g_stats.wifi_reconnects,
                  WiFi.RSSI());
}

// ─────────────────────────────────────────────────────────────────────────────
// setup / loop
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    pinMode(LED_PIN, OUTPUT);
    led_set(false);

    Serial.begin(SERIAL0_BAUD);
    delay(50);
    Serial.println("\n[boot] ESP32 Telemetry Bridge v0");
    Serial.printf("[boot] hostname=%s\n", WIFI_HOSTNAME);

    g_uart.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    g_uart.setRxBufferSize(UART_BUF_SIZE);

    wifi_begin();
    g_target_ip.fromString(UDP_OUT_TARGET);
    g_udp_in.begin(UDP_IN_PORT);

#if ESPNOW_ENABLE
    espnow_setup();
#endif

#if OTA_ENABLE
    ArduinoOTA.setHostname(WIFI_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.begin();
    Serial.println("[ota] habilitado");
#endif
}

void loop() {
    const uint32_t now = millis();

    wifi_watchdog(now);
    pump_uart_to_udp(now);
    pump_udp_to_uart(now);

#if ESPNOW_ENABLE
    espnow_tick(now);
#endif
#if OTA_ENABLE
    ArduinoOTA.handle();
#endif

    led_tick(now);
    print_stats(now);

    // No delay agresivo — el RX del UART y del UDP son lo más prioritario.
    delay(1);
}
