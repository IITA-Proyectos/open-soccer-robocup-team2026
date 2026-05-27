// diag_localization_live.cpp — Diagnostico en banco del modulo localization.
//
// NO es firmware de competencia. Sketch standalone que llama al modulo
// localization_runtime y printea pose cada 500 ms por Serial. Permite
// validar la trilateracion en banco con el robot real puesto a mano en
// posiciones conocidas de la cancha.
//
// Build / flash:
//   pio run -t clean -e diag_localization_live
//   pio run -e diag_localization_live -t upload
//   pio device monitor -b 115200

#include <Arduino.h>
#include "localization_runtime.h"
#include "sensors_tof.h"
#include "sensors_imu.h"
#include "config_top.h"

using namespace iitasoccer;

void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    Serial.begin(115200);
    while (!Serial && millis() < 3000) { /* esperar monitor */ }

    Serial.println("\n=========================================");
    Serial.println("  diag_localization_live");
    Serial.println("  (banco, NO es competencia)");
    Serial.println("  ROBOT DEBE APUNTAR AL ARCO RIVAL");
    Serial.println("=========================================");

    sensors_imu_init();
    sensors_tof_init();
    delay(500);  // estabilizacion sensores

    localization_runtime_init();
    Serial.println("[diag] init OK. Pose cada 500 ms.\n");
}

void loop() {
    static elapsedMillis since_tick;
    static elapsedMillis since_print;

    if (since_tick >= 33) {
        since_tick = 0;
        sensors_imu_tick();
        sensors_tof_tick();
        localization_runtime_tick();
    }

    if (since_print >= 500) {
        since_print = 0;
        auto pose = localization_runtime_get_pose();
        Serial.print("[");
        Serial.print(millis());
        Serial.print(" ms]  ");
        if (pose.valid) {
            Serial.print("x=");      Serial.print(pose.x_mm);
            Serial.print("mm  y="); Serial.print(pose.y_mm);
            Serial.print("mm  hdg="); Serial.print(pose.heading_centideg / 100.0);
            Serial.print("deg  src=0b");
            for (int i = 3; i >= 0; --i) Serial.print((pose.source_flags >> i) & 1);
            Serial.println();
            digitalWrite(PIN_LED_STATUS, HIGH);
        } else {
            Serial.println("pose INVALID (no hay suficientes TOFs utiles)");
            digitalWrite(PIN_LED_STATUS, LOW);
        }
    }
}
