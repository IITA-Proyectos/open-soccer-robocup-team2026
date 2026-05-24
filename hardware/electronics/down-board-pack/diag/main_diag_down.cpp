// main_diag_down.cpp — Diagnóstico de HARDWARE de la placa DOWN.
//
// ⚠️ NO es el firmware de competencia. El firmware real es `src/down/main_down.cpp`
// (env `down`). Esto es una herramienta de BANCO: cargá esto en la Teensy 4.0
// de la placa DOWN, abrí el Serial Monitor a 115200 baud, y vas a ver en vivo
// (refresco cada 300 ms):
//   • el valor crudo de cada sensor de luz + si ve blanco
//   • el ángulo / profundidad / flags de línea procesados
//   • el estado de los OTOS
//
// Sirve para verificar, ANTES de un partido, que cada sensor responde: pasás
// la línea blanca por delante de cada sensor y mirás que su número cambie.
//
// Build / flash:
//   pio run -e diag_down -t upload
//
// Reusa la API pública de line_ring.h y otos.h — NO modifica código de
// operación. Creado 2026-05-19 para el hardware-up de la placa DOWN.

#include <Arduino.h>
#include <Wire.h>
#include "config_down.h"
#include "line_ring.h"
#include "otos.h"

using namespace iitasoccer;

// I²C scanner — recorre todos los addresses 7-bit (1..127) en un bus dado y
// reporta los que responden con ACK. Útil para diagnosticar OTOS y otros chips
// I²C antes de cualquier inicialización específica de driver.
static void scan_i2c_bus(TwoWire& bus, const char* bus_name) {
    Serial.print("[i2c-scan] ");
    Serial.print(bus_name);
    Serial.print(": ");
    int found = 0;
    for (uint8_t addr = 1; addr < 128; ++addr) {
        bus.beginTransmission(addr);
        if (bus.endTransmission() == 0) {
            if (found > 0) Serial.print(", ");
            Serial.print("0x");
            if (addr < 16) Serial.print('0');
            Serial.print(addr, HEX);
            ++found;
        }
    }
    if (found == 0) Serial.print("(sin dispositivos — bus vacio o sin pull-ups o sin power 3V3)");
    Serial.println();
}

namespace {
elapsedMillis g_since_print;
elapsedMicros g_since_line_tick;
elapsedMillis g_since_i2c_scan;
}  // namespace

void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    Serial.begin(115200);
    while (!Serial && millis() < 3000) { /* esperar al monitor, máx 3 s */ }

    Serial.println("\n=========================================");
    Serial.println("  DIAG DOWN — diagnostico de sensores");
    Serial.println("  (herramienta de banco, NO es competencia)");
    Serial.println("=========================================");
    Serial.print("Sensores de luz configurados : ");
    Serial.println(NUM_LINE_SENSORS);
    Serial.print("OTOS configurados            : ");
    Serial.println(NUM_OTOS);
    Serial.println();

    line_ring_init();
    Serial.println("[diag] line_ring init OK");

    // I²C scan ANTES de otos_init: dice qué dispositivos hay físicamente en
    // cada bus. Si los buses dan vacío en ambos, problema de hardware (power
    // 3V3 caído, pull-ups SDA/SCL ausentes, o chips desoldados).
    Wire.begin();
    Wire1.begin();
    delay(50);  // dejar que los buses se estabilicen
    scan_i2c_bus(Wire,  "Wire  (I2C0, SDA=18 SCL=19, OTOS U5)");
    scan_i2c_bus(Wire1, "Wire1 (I2C1, SDA=17 SCL=16, OTOS U6)");
    Serial.println();

    const bool otos_ok = otos_init();
    Serial.print("[diag] OTOS init: L=");
    Serial.print(otos_is_left_ready() ? "OK" : "--");
    Serial.print(" R=");
    Serial.print(otos_is_right_ready() ? "OK" : "--");
    Serial.println(otos_ok ? "" : "  (ninguno responde)");
    Serial.println();

    Serial.println("[diag] Calibrando carpet... NO mover la placa (~0.5 s)");
    line_ring_calibrate_carpet();
    Serial.println("[diag] calibracion carpet OK");
    Serial.println();
    Serial.println("Pasa la linea blanca por cada sensor y observa su valor.");
    Serial.println("Un sensor que NO cambia nunca = sensor muerto o mal soldado.");
    Serial.println("---------------------------------------------------------");
}

void loop() {
    // Muestrear la línea a 1 kHz (igual frecuencia que el firmware real).
    if (g_since_line_tick >= 1000) {
        g_since_line_tick = 0;
        line_ring_tick();
    }
    if (NUM_OTOS >= 1) {
        otos_tick();
    }

    // I²C scan cada 5 segundos — útil para diagnóstico en vivo.
    if (g_since_i2c_scan >= 5000) {
        g_since_i2c_scan = 0;
        scan_i2c_bus(Wire,  "Wire  (I2C0, OTOS U5)");
        scan_i2c_bus(Wire1, "Wire1 (I2C1, OTOS U6)");
    }

    // Volcar el estado por Serial cada 300 ms.
    if (g_since_print >= 300) {
        g_since_print = 0;
        digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));  // latido visual

        // --- Sensores de luz: valor crudo + marca de blanco ---
        Serial.print("LUZ ");
        for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
            Serial.print("S");
            Serial.print(i);
            Serial.print(":");
            Serial.print(line_ring_get_raw(i));
            Serial.print(line_ring_get_white(i) ? "*" : " ");  // * = ve blanco
            Serial.print(" ");
        }
        Serial.println();

        // --- Línea procesada ---
        Serial.print("    linea: angulo=");
        Serial.print(line_ring_get_angle_deg(), 1);
        Serial.print("deg  depth=");
        Serial.print(line_ring_get_depth());
        Serial.print("  salida_inminente=");
        Serial.print(line_ring_get_imminent_exit() ? "SI" : "no");
        Serial.print("  levantada=");
        Serial.println(line_ring_is_lifted() ? "SI" : "no");

        // --- OTOS (pose fusionada + estado de los 2 chips) ---
        Serial.print("    OTOS: x=");
        Serial.print(otos_get_x_mm(), 1);
        Serial.print(" y=");
        Serial.print(otos_get_y_mm(), 1);
        Serial.print(" hdg=");
        Serial.print(otos_get_heading_deg(), 1);
        Serial.print("  [L=");
        Serial.print(otos_is_left_ready() ? "OK" : "--");
        Serial.print(" R=");
        Serial.print(otos_is_right_ready() ? "OK" : "--");
        Serial.println("]");
        Serial.println();
    }
}
