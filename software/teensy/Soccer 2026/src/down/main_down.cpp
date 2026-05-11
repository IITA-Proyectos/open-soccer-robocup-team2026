// main_down.cpp — Firmware de la placa DOWN (Teensy 4.0)
//
// Responsabilidad: leer 32 sensores de luz multiplexados + 2 SparkFun OTOS,
// procesar línea y pose, y enviar ese estado al TOP a 100 Hz por Serial5.
//
// Frecuencias de los ticks:
//   • line_ring_tick(): 1 kHz (línea es lo más urgente — el robot la cruza
//     en pocos ms a velocidades altas).
//   • otos_tick(): 100 Hz (suficiente para tracking de posición).
//   • comm_top_send_status(): 100 Hz.
//   • comm_top_tick() (rx): cada loop, no bloquea.
//
// Build:
//   pio run -e down
//
// Flags opcionales (para soportar la placa 04-12 vs fixeada):
//   -DDOWN_NUM_MUXES_CONNECTED=4 → placa fixeada con todos los muxes
//   -DDOWN_NUM_OTOS_CONNECTED=2  → ambos OTOS conectados
//   (defaults conservadores: 1 mux + 1 OTOS — ver config_down.h)

#include <Arduino.h>
#include "config_down.h"
#include "line_ring.h"
#include "otos.h"
#include "comm_top.h"

using namespace iitasoccer;

namespace {

elapsedMicros g_since_line_tick;
elapsedMillis g_since_otos_tick;
elapsedMillis g_since_comm_send;

}  // namespace

void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    Serial.begin(115200);  // USB debug
    delay(50);

    line_ring_init();
    Serial.println("[DOWN] line_ring init OK");

    const bool otos_ok = otos_init();
    Serial.print("[DOWN] OTOS: ");
    Serial.print(otos_is_left_ready() ? "L=ok " : "L=- ");
    Serial.print(otos_is_right_ready() ? "R=ok " : "R=- ");
    Serial.println(otos_ok ? "(al menos uno OK)" : "(NINGUNO — degradacion total)");

    comm_top_init();
    Serial.println("[DOWN] comm_top init OK");

    // Calibración rápida: capturar carpet (asumir robot sobre carpet al encender).
    Serial.println("[DOWN] calibrando carpet... no mover el robot");
    line_ring_calibrate_carpet();
    Serial.println("[DOWN] calibracion carpet OK");

    digitalWrite(PIN_LED_STATUS, HIGH);
    Serial.println("[DOWN] listo, enviando datos al TOP");
}

void loop() {
    // RX: drenar UART desde TOP (cada loop, sin throttle).
    comm_top_tick();

    // line_ring: 1 kHz.
    if (g_since_line_tick >= LINE_TICK_INTERVAL_US) {
        g_since_line_tick = 0;
        line_ring_tick();
    }

    // otos + send: 100 Hz cada uno.
    if (g_since_otos_tick >= OTOS_TICK_INTERVAL_MS) {
        g_since_otos_tick = 0;
        otos_tick();
    }
    if (g_since_comm_send >= COMM_SEND_INTERVAL_MS) {
        g_since_comm_send = 0;
        comm_top_send_status();
    }
}
