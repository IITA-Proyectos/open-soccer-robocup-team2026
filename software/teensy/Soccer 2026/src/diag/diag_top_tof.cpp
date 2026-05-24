// diag_top_tof.cpp — Diagnostico del sensor ToF VL53L7CX frontal (placa TOP, U2).
//
// NO es firmware de competencia. Sketch standalone que valida que el sensor
// VL53L7CX soldado en U2 responde, se inicializa y mide. Sigue el patron de
// main_diag_down.cpp.
//
// Hardware esperado (segun pack 2026-05-24):
//   • Teensy 4.0 de la placa TOP
//   • VL53L7CX en posicion U2 del schematic
//   • Bus Wire (I2C0): SDA=18, SCL=19
//   • XSHUT del U2 conectado al pin 2 del Teensy
//
// Build / flash:
//   pio run -e diag_top_tof -t upload
//
// Modos (cambiar SOLO uno via build_flags):
//   -DDIAG_TOF_MODE_SINGLE   ; 1 numero promedio
//   -DDIAG_TOF_MODE_4X4      ; (default) grilla 4x4 ASCII
//   -DDIAG_TOF_MODE_8X8      ; grilla 8x8 ASCII
//
// Fallback si XSHUT pin 2 no esta ruteado:
//   -DDIAG_TOF_SKIP_XSHUT    ; asume sensor siempre alimentado
//
// Spec: docs/superpowers/specs/2026-05-24-diag-top-tof-vl53l7cx-design.md
// Creado 2026-05-24 para hardware-up del VL53L7CX frontal de la placa TOP.

#include <Arduino.h>
#include <Wire.h>
#include <vl53l7cx_class.h>

// ============================================================
// Configuracion de modo (mutuamente excluyente)
// ============================================================
#if defined(DIAG_TOF_MODE_SINGLE) + defined(DIAG_TOF_MODE_4X4) + defined(DIAG_TOF_MODE_8X8) > 1
#error "Solo UN modo a la vez: DIAG_TOF_MODE_SINGLE | DIAG_TOF_MODE_4X4 | DIAG_TOF_MODE_8X8"
#endif

#if !defined(DIAG_TOF_MODE_SINGLE) && !defined(DIAG_TOF_MODE_4X4) && !defined(DIAG_TOF_MODE_8X8)
#define DIAG_TOF_MODE_4X4   // default
#endif

// ============================================================
// Pinout y constantes
// ============================================================
constexpr int PIN_XSHUT_TOF_FRONT = 2;     // U2 del schematic — XSHUT al pin 2 del Teensy.
constexpr int PIN_LED_STATUS      = 13;    // LED_BUILTIN.
constexpr uint32_t I2C_CLOCK_HZ   = 400000;  // fast-mode standard; subir a 1 MHz si responde estable.

// ============================================================
// Globales
// ============================================================
namespace {
VL53L7CX g_sensor(&Wire, PIN_XSHUT_TOF_FRONT);   // constructor: (TwoWire*, lpn_pin/XSHUT)
bool g_init_ok = false;
}  // namespace

// ============================================================
// setup() / loop() — skeleton: solo banner + LED. Se llenan en Tasks 3-4.
// ============================================================
void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    Serial.begin(115200);
    while (!Serial && millis() < 3000) { /* esperar al monitor, max 3 s */ }

    Serial.println("\n=== diag_top_tof (skeleton) ===");
    Serial.println("Build OK. Se completa en Tasks 3-4 del plan.");
}

void loop() {
    // Heartbeat visible: LED parpadea a 1 Hz.
    static uint32_t last = 0;
    if (millis() - last >= 500) {
        last = millis();
        digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));
    }
}
