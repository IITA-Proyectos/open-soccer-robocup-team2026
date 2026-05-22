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
#include "config_down.h"
#include "line_ring.h"
#include "otos.h"

using namespace iitasoccer;

namespace {
elapsedMillis g_since_print;
elapsedMicros g_since_line_tick;
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

    const bool otos_ok = otos_init();
    Serial.print("[diag] OTOS init: L=");
    Serial.print(otos_is_left_ready() ? "OK" : "--");
    Serial.print(" R=");
    Serial.print(otos_is_right_ready() ? "OK" : "--");
    Serial.println(otos_ok ? "" : "  (ninguno responde)");
    Serial.println("[diag] OJO: OTOS esta en modo STUB (lib SparkFun sin");
    Serial.println("       integrar, TASK-012). Va a reportar valores 0");
    Serial.println("       aunque el chip este. Los sensores de LUZ son reales.");
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

        // --- OTOS (stub — ver aviso del setup) ---
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
