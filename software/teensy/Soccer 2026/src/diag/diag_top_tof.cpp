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

// I2C scanner — recorre addresses 7-bit (1..127) en bus Wire y reporta los
// que responden con ACK. Util para distinguir "VL53L7CX no responde I2C"
// vs "responde pero la lib falla en init".
void scan_i2c_bus(TwoWire& bus, const char* bus_name) {
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
    if (found == 0) Serial.print("(sin dispositivos)");
    Serial.println();
}

const char* mode_name() {
#if defined(DIAG_TOF_MODE_SINGLE)
    return "SINGLE";
#elif defined(DIAG_TOF_MODE_4X4)
    return "4x4";
#elif defined(DIAG_TOF_MODE_8X8)
    return "8x8";
#endif
}
}  // namespace

// ============================================================
// setup() / loop() — skeleton: solo banner + LED. Se llenan en Tasks 3-4.
// ============================================================
void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    Serial.begin(115200);
    while (!Serial && millis() < 3000) { /* esperar al monitor, max 3 s */ }

    // ------ Banner ------
    Serial.println("\n=========================================");
    Serial.println("  diag_top_tof — VL53L7CX frontal U2");
    Serial.println("  (herramienta de banco, NO es competencia)");
    Serial.println("=========================================");
    Serial.print  ("Board       : Teensy 4.0 (TOP)\n");
    Serial.print  ("Bus         : Wire (I2C0)  SDA=18  SCL=19\n");
#ifdef DIAG_TOF_SKIP_XSHUT
    Serial.print  ("XSHUT       : SKIPPED por define\n");
#else
    Serial.print  ("XSHUT       : pin 2 (sera HIGH)\n");
#endif
    Serial.print  ("Mode        : "); Serial.println(mode_name());
    Serial.print  ("I2C clock   : "); Serial.print(I2C_CLOCK_HZ / 1000); Serial.println(" kHz");
    Serial.print  ("Build       : "); Serial.print(__DATE__); Serial.print(" "); Serial.println(__TIME__);
    Serial.println("-----------------------------------------");

    // ------ XSHUT del U2 ------
#ifndef DIAG_TOF_SKIP_XSHUT
    pinMode(PIN_XSHUT_TOF_FRONT, OUTPUT);
    digitalWrite(PIN_XSHUT_TOF_FRONT, LOW);
    delay(10);
    digitalWrite(PIN_XSHUT_TOF_FRONT, HIGH);
    delay(10);
    Serial.println("[xshut] pin 2 HIGH — sensor power-on");
#endif

    // ------ Wire init + scan ANTES de tocar la lib ------
    Wire.begin();
    Wire.setClock(I2C_CLOCK_HZ);
    delay(5);
    scan_i2c_bus(Wire, "Wire (I2C0)");
    // Si NO aparece 0x29: el sensor no responde por I2C. Stop, problema fisico.

    // ------ Init del sensor ------
    Serial.print("[init] g_sensor.begin() ... ");
    g_sensor.begin();
    Serial.println("ok");

    Serial.print("[init] loading firmware ULD (~3 s) ... ");
    int err = g_sensor.init_sensor();
    if (err != 0) {
        Serial.print("FAILED err="); Serial.println(err);
        Serial.println("[diag] init fallo — sensor no se inicializa. Posibles causas:");
        Serial.println("       - alimentacion 3V3 baja en el VL53L7CX");
        Serial.println("       - XSHUT mal ruteado (probar -DDIAG_TOF_SKIP_XSHUT)");
        Serial.println("       - lib vendoreada incompatible con esta variante de L7CX");
        Serial.println("[diag] entrando a loop con LED en error pattern (3 blinks rapidos).");
        return;  // g_init_ok queda false → loop entra a error pattern
    }
    Serial.println("OK");

    // ------ Configurar resolucion + frecuencia ------
    // Nota: la lib STM32duino expone los metodos con prefix `vl53l7cx_`
    // (no `set_resolution` plain). Ver lib/STM32duino_VL53L7CX/src/vl53l7cx_class.h.
#if defined(DIAG_TOF_MODE_8X8)
    g_sensor.vl53l7cx_set_resolution(VL53L7CX_RESOLUTION_8X8);
    Serial.println("[init] resolution = 8x8");
#else
    g_sensor.vl53l7cx_set_resolution(VL53L7CX_RESOLUTION_4X4);
    Serial.println("[init] resolution = 4x4");
#endif
    g_sensor.vl53l7cx_set_ranging_frequency_hz(15);
    g_sensor.vl53l7cx_set_ranging_mode(VL53L7CX_RANGING_MODE_CONTINUOUS);

    g_sensor.vl53l7cx_start_ranging();
    g_init_ok = true;
    Serial.println("[init] start_ranging — listo. Esperando frames...");
    Serial.println();
}

void loop() {
    // Heartbeat visible: LED parpadea a 1 Hz.
    static uint32_t last = 0;
    if (millis() - last >= 500) {
        last = millis();
        digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));
    }
}
