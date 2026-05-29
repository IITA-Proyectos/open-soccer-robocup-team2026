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
#include "comm_central.h"

using namespace iitasoccer;

namespace {

elapsedMicros g_since_line_tick;
elapsedMillis g_since_otos_tick;
elapsedMillis g_since_top_send;        // 100 Hz odometría a ARRIBA
elapsedMillis g_since_central_send;    // 100-200 Hz línea a CENTRAL

constexpr uint32_t LINE_URGENT_INTERVAL_MS = 5;  // 200 Hz hacia CENTRAL

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

    comm_top_init();          // Serial5 → ARRIBA (odometría)
    comm_central_init();      // Serial1 → CENTRAL (bus emergencia: línea)
    Serial.println("[DOWN] comm_top + comm_central init OK");

    // Calibración rápida: capturar carpet (asumir robot sobre carpet al encender).
    Serial.println("[DOWN] calibrando carpet... no mover el robot");
    line_ring_calibrate_carpet();
    Serial.println("[DOWN] calibracion carpet OK");

    // Cargar calibración persistida en EEPROM (audit P0.2 — 2026-05-29).
    // Si existe una calib manual previa (carpet+blanco), gana sobre la
    // derivación boot-time porque trae una referencia de BLANCO real. El
    // carpet recién medido arriba se usa solo como fallback si la EEPROM
    // está vacía/inválida (primer arranque o tras ec_erase_calibration()).
    if (comm_central_load_persisted_calib()) {
        Serial.println("[DOWN] calib cargada de EEPROM (persistida)");
    } else {
        Serial.println("[DOWN] EEPROM sin calib valida — usando carpet recien medido");
    }

    digitalWrite(PIN_LED_STATUS, HIGH);
    Serial.println("[DOWN] listo: odometria a ARRIBA + linea urgente a CENTRAL");
}

void loop() {
    // RX: drenar ambos UARTs cada loop (no bloquea).
    comm_top_tick();          // comandos desde ARRIBA (legacy, low priority)
    comm_central_tick();      // comandos desde CENTRAL (calib línea, reset OTOS)

    // line_ring: 1 kHz (lectura urgente).
    if (g_since_line_tick >= LINE_TICK_INTERVAL_US) {
        g_since_line_tick = 0;
        line_ring_tick();
    }

    // OTOS: 100 Hz.
    if (g_since_otos_tick >= OTOS_TICK_INTERVAL_MS) {
        g_since_otos_tick = 0;
        otos_tick();
    }

    // Send odometría a ARRIBA: 100 Hz (Serial5).
    if (g_since_top_send >= COMM_SEND_INTERVAL_MS) {
        g_since_top_send = 0;
        comm_top_send_status();
    }

    // Send línea urgente a CENTRAL: 200 Hz (Serial1, bus emergencia).
    if (g_since_central_send >= LINE_URGENT_INTERVAL_MS) {
        g_since_central_send = 0;
        comm_central_send_line_urgent();
    }
}
