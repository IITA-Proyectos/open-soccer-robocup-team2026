// central_telemetry_esp32_glue.example.cpp — EJEMPLO (no aplicado).
//
// Muestra cómo el firmware de CENTRAL podría agregar el ESP32 Telemetry Bridge
// como segundo canal de telemetría, sin tocar el USB CDC actual ni el binario
// de competencia.
//
// REGLAS:
//   1. Todo gateado por -DENABLE_TELEMETRY_ESP32 (default OFF en envs de
//      competencia). Con el flag OFF, este archivo no aporta nada al binario.
//   2. NO reemplaza el envío por USB CDC — lo DUPLICA. Misma línea JSON va a
//      ambos puertos.
//   3. NO modifica el módulo puro (telemetry_central.cpp). Solo el glue de
//      Serial.
//
// Cómo integrarlo (cuando el coach lo apruebe):
//   - Copiar este archivo a software/teensy/Soccer 2026/src/central/ (o
//      mover el contenido al glue existente de telemetría de CENTRAL si
//      ya hay uno equivalente al de DOWN/TOP).
//   - Agregar el env central_robot1_telem_esp32 en platformio.ini.
//   - Compilar: pio run -e central_robot1_telem_esp32.
//   - Cablear el ESP32 (ver r-d-2027/specs/esp32-telemetry-bridge-v0-spec.md §3.2).

#ifdef ENABLE_TELEMETRY_ESP32

#include <Arduino.h>

namespace iitasoccer {

// ─────────────────────────────────────────────────────────────────────────────
// Configuración
// ─────────────────────────────────────────────────────────────────────────────
static constexpr long  TELEM_ESP32_BAUD     = 115200;
static constexpr int   TELEM_ESP32_RX_BUF   = 4096;
static HardwareSerial& telem_esp32_serial   = Serial2;  // pin 7 TX, pin 8 RX

// Buffer para acumular bytes recibidos del ESP32 hasta '\n'.
static char    s_cmd_buf[1024];
static size_t  s_cmd_len = 0;

// ─────────────────────────────────────────────────────────────────────────────
// Init — llamar UNA vez desde setup() de main_central.cpp
// ─────────────────────────────────────────────────────────────────────────────
void telemetry_esp32_begin() {
    telem_esp32_serial.begin(TELEM_ESP32_BAUD);
    // Algunos cores Teensy tienen setRxBufferSize, otros no. Si no compila,
    // omitir esta línea — el buffer software del core suele alcanzar para 50 Hz.
    // telem_esp32_serial.setRxBufferSize(TELEM_ESP32_RX_BUF);
}

// ─────────────────────────────────────────────────────────────────────────────
// Emit — llamar DESPUÉS del Serial.print(line) actual.
// "line" tiene que terminar en '\n' (lo que hace el módulo puro).
// ─────────────────────────────────────────────────────────────────────────────
void telemetry_esp32_emit_line(const char* line, size_t len) {
    telem_esp32_serial.write(reinterpret_cast<const uint8_t*>(line), len);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick — llamar cada loop. Drena Serial2 hasta '\n' y entrega la línea al
// parser de comandos existente del firmware (firma de ejemplo:
// tc_parse_command(const char*, size_t)).
// ─────────────────────────────────────────────────────────────────────────────
extern void tc_parse_command(const char* cmd, size_t len);   // implementado en otro lado

void telemetry_esp32_tick() {
    while (telem_esp32_serial.available() > 0) {
        int c = telem_esp32_serial.read();
        if (c < 0) break;
        if (c == '\r') continue;
        if (c == '\n') {
            if (s_cmd_len > 0) {
                s_cmd_buf[s_cmd_len] = '\0';
                tc_parse_command(s_cmd_buf, s_cmd_len);
            }
            s_cmd_len = 0;
            continue;
        }
        if (s_cmd_len + 1 < sizeof s_cmd_buf) {
            s_cmd_buf[s_cmd_len++] = static_cast<char>(c);
        } else {
            s_cmd_len = 0;  // descartar línea sobre-larga
        }
    }
}

}  // namespace iitasoccer

#endif  // ENABLE_TELEMETRY_ESP32

// ─────────────────────────────────────────────────────────────────────────────
// Cómo se usaría desde main_central.cpp (snippet conceptual):
//
//     #include "telemetry_esp32_glue.h"   // un .h equivalente al glue actual
//
//     void setup() {
//         // ... lo de hoy ...
//         #ifdef ENABLE_TELEMETRY_ESP32
//             iitasoccer::telemetry_esp32_begin();
//         #endif
//     }
//
//     void loop() {
//         // ... lo de hoy ...
//
//         // En el punto donde el firmware actual hace:
//         //   Serial.print(line);          // USB CDC (sigue igual)
//         #ifdef ENABLE_TELEMETRY_ESP32
//             iitasoccer::telemetry_esp32_emit_line(line, len);
//         #endif
//
//         #ifdef ENABLE_TELEMETRY_ESP32
//             iitasoccer::telemetry_esp32_tick();
//         #endif
//     }
//
// El env de competencia (central_robot1) no tiene -DENABLE_TELEMETRY_ESP32,
// así que estos bloques desaparecen del binario → byte-idéntico al actual.
// ─────────────────────────────────────────────────────────────────────────────
