// diag_sensors_tof_live.cpp — Test del MODULO MIGRADO src/top/sensors_tof.cpp.
//
// NO es firmware de competencia. Sketch standalone que reusa la API publica
// del modulo vivo (`sensors_tof_init/tick/get_distance_mm/get_min_distance_mm`)
// pero compila aislado: sin main_top, sin camaras, sin IMU, sin comm_*. Sirve
// para validar en banco que la migracion del stub TODO_TOF_LIB a la lib
// Adafruit_VL53L7CX (commit del 2026-05-24) funciona igual que el sketch de
// control standalone `[env:diag_top_tof_adafruit]`.
//
// Setup esperado:
//   • Teensy 4.0 (placa TOP del robot o uno fresco en protoboard)
//   • VL53L7CX en Wire (I2C0): SDA=18, SCL=19, addr 0x29
//   • HC-SR04 (opcional) en TRIG=6 / ECHO=7. Si no esta conectado, el modulo
//     retorna TOF_NO_READING y no rompe — el ToF sigue midiendo igual.
//
// Build / flash:
//   pio run -t clean -e diag_sensors_tof_live
//   pio run -e diag_sensors_tof_live -t upload
//   pio device monitor -b 115200
//
// Que esperar en el monitor:
//   • Banner inicial.
//   • Una linea del propio modulo: "[sensors_tof] OK: VL53L7CX U2 frontal ..."
//     (o WARN si init fallo — sigue corriendo igual).
//   • Cada 500 ms, una linea con tof[0], hc-sr04 y min global.
//   • LED ON heartbeat mientras hay lecturas; pattern 3 blinks/s si init fallo.
//
// Creado 2026-05-24 como parte de la migracion del stub a la lib Adafruit.
// Ver journal/2026-05-24-hardware-up-top-tof-frontal-resuelto.md.

#include <Arduino.h>

#include "sensors_tof.h"
#include "config_top.h"   // PIN_LED_STATUS, TOF_NO_READING (transitivo)

using iitasoccer::sensors_tof_init;
using iitasoccer::sensors_tof_tick;
using iitasoccer::sensors_tof_get_distance_mm;
using iitasoccer::sensors_hcsr04_get_distance_mm;
using iitasoccer::sensors_tof_get_min_distance_mm;
using iitasoccer::sensors_tof_is_ready;
using iitasoccer::TOF_NO_READING;
using iitasoccer::PIN_LED_STATUS;

namespace {
bool g_init_ok = false;

void print_mm(const char* label, uint16_t mm) {
    Serial.print(label);
    if (mm == TOF_NO_READING) {
        Serial.print(F("----"));
    } else {
        Serial.print(mm);
        Serial.print(F("mm"));
    }
}
}  // namespace

void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    Serial.begin(115200);
    while (!Serial && millis() < 3000) { /* espera el monitor max 3 s */ }

    Serial.println(F("\n========================================="));
    Serial.println(F("  diag_sensors_tof_live"));
    Serial.println(F("  Test del MODULO MIGRADO src/top/sensors_tof.cpp"));
    Serial.println(F("  (stub TODO_TOF_LIB -> Adafruit_VL53L7CX, 2026-05-24)"));
    Serial.println(F("========================================="));
    Serial.print  (F("Build       : ")); Serial.print(__DATE__); Serial.print(' '); Serial.println(__TIME__);
    Serial.println(F("ToF frontal : Wire (I2C0) SDA=18 SCL=19 @ 0x29 (idx 0)"));
    Serial.println(F("HC-SR04     : TRIG=6 ECHO=7 (opcional)"));
    Serial.println(F("-----------------------------------------"));

    // El modulo vivo loggea por su cuenta el OK/WARN del init.
    g_init_ok = sensors_tof_init();
    Serial.print(F("[diag] sensors_tof_init() -> "));
    Serial.println(g_init_ok ? F("true") : F("false"));
    Serial.print(F("[diag] is_ready(0)        -> "));
    Serial.println(sensors_tof_is_ready(0) ? F("true (ToF frontal listo)")
                                            : F("false (ToF frontal no listo)"));
    Serial.println();
}

void loop() {
    // Tick a ~30 Hz como hace main_top en produccion (TOF_TICK_INTERVAL_MS=30).
    static uint32_t t_last_tick = 0;
    const uint32_t now = millis();
    if (now - t_last_tick >= 30) {
        t_last_tick = now;
        sensors_tof_tick();
    }

    // Print cada 500 ms.
    static uint32_t t_last_print = 0;
    if (now - t_last_print >= 500) {
        t_last_print = now;

        Serial.print(F("[t=")); Serial.print(now); Serial.print(F("ms]  "));
        print_mm("tof[0]=",  sensors_tof_get_distance_mm(0));
        Serial.print(F("  "));
        print_mm("hc-sr04=", sensors_hcsr04_get_distance_mm());
        Serial.print(F("  "));
        print_mm("min=",     sensors_tof_get_min_distance_mm());
        Serial.println();
    }

    // LED:
    //   • init OK -> heartbeat 1 Hz (medio segundo on, medio off)
    //   • init FAIL (ToF no inicializo aunque init() devolvio true) ->
    //     pattern 3 blinks rapidos cada 1 s.
    const bool tof_alive = sensors_tof_is_ready(0);
    if (tof_alive) {
        digitalWrite(PIN_LED_STATUS, (now / 500) % 2 ? HIGH : LOW);
    } else {
        const uint32_t phase = now % 1000;
        const bool on = (phase < 100) || (phase >= 200 && phase < 300) ||
                        (phase >= 400 && phase < 500);
        digitalWrite(PIN_LED_STATUS, on ? HIGH : LOW);
    }
}
