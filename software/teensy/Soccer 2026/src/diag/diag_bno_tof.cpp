// diag_bno_tof.cpp — BISECT: BNO055 0x28 (lectura directa) + los 4 ToF ACTIVOS en el
// MISMO bus Wire (18/19). Sin camaras ni HC-SR04 ni la fusion de sensors_imu.
//
// Objetivo: aislar si el "heading clavado" del firmware integrado es por la COEXISTENCIA
// BNO + ToF en el bus I2C. En diag_bno_left (ToF dormidos) el yaw seguia el giro; aca los
// ToF estan despiertos y rangeando -> si el yaw se CLAVA, es la coexistencia BNO+ToF.
//
// Clock del bus: 400 kHz por default; compilar con -DDIAG_BNO_TOF_100K para 100 kHz
// (env diag_bno_tof_slow) y ver si bajar la velocidad lo arregla.
//
//   pio run -e diag_bno_tof       -t upload   (400 kHz)
//   pio run -e diag_bno_tof_slow  -t upload   (100 kHz)
//   pio device monitor -b 115200
//   -> POWER-CYCLE tras flashear (los ToF persisten direccion). Despues GIRA el robot.
//
// Atribucion: Claude Opus 4.8 (Anthropic), pedido de Gustavo Viollaz (@gviollaz).

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_VL53L7CX.h>

namespace {

#ifdef DIAG_BNO_TOF_100K
constexpr uint32_t I2C_HZ = 100000;
#else
constexpr uint32_t I2C_HZ = 400000;
#endif

constexpr uint8_t BNO_ADDR = 0x28;
Adafruit_BNO055 g_bno(55, BNO_ADDR, &Wire);
bool g_bno_ok = false;

constexpr uint8_t LP_PIN[4] = {9, 10, 11, 12};
constexpr uint8_t TOF_ADDR[4] = {0x2A, 0x2B, 0x2C, 0x2D};
Adafruit_VL53L7CX g_tof[4];
VL53L7CX_ResultsData g_res;
bool     g_tof_ok[4] = {false, false, false, false};
uint16_t g_dist[4]   = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};

elapsedMillis g_since_print;

bool acks(uint8_t a) { Wire.beginTransmission(a); return Wire.endTransmission() == 0; }

}  // namespace

void setup() {
    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) {}

    Serial.print(F("\n==== diag_bno_tof : BNO 0x28 + 4 ToF en Wire @ "));
    Serial.print(I2C_HZ / 1000); Serial.println(F(" kHz ===="));

    // Dormir ToF -> bus limpio para iniciar el BNO primero.
    for (uint8_t p : LP_PIN) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
    delay(120);
    Wire.begin();
    Wire.setClock(I2C_HZ);

    Serial.print(F("BNO begin(IMUPLUS)... "));
    uint8_t tr = 0;
    while (!(g_bno_ok = g_bno.begin(OPERATION_MODE_IMUPLUS)) && tr++ < 5) delay(300);
    Serial.println(g_bno_ok ? F("OK") : F("FALLO"));
    if (g_bno_ok) g_bno.setExtCrystalUse(true);

    // Enumerar los 4 ToF (despertar de a uno -> begin en 0x29 -> mover a 0x2A..0x2D).
    for (uint8_t i = 0; i < 4; ++i) {
        digitalWrite(LP_PIN[i], HIGH); delay(120);
        if (!acks(0x29)) { Serial.print(F("ToF#")); Serial.print(i); Serial.println(F(" no aparece -> salteo")); digitalWrite(LP_PIN[i], LOW); continue; }
        if (!g_tof[i].begin(0x29, &Wire, I2C_HZ)) { Serial.print(F("ToF#")); Serial.print(i); Serial.println(F(" begin fallo")); continue; }
        if (!g_tof[i].setAddress(TOF_ADDR[i])) continue;
        g_tof[i].setResolution(16);
        g_tof[i].setRangingFrequency(15);
        if (!g_tof[i].startRanging()) continue;
        g_tof_ok[i] = true;
    }
    int n = 0; for (int i = 0; i < 4; ++i) if (g_tof_ok[i]) ++n;
    Serial.print(F("ToF midiendo: ")); Serial.print(n); Serial.println(F("/4"));
    Serial.println(F("GIRA el robot: yaw debe SEGUIR. Si se clava con los ToF activos -> coexistencia BNO+ToF."));
}

void loop() {
    for (int i = 0; i < 4; ++i) {
        if (g_tof_ok[i] && g_tof[i].isDataReady() && g_tof[i].getRangingData(&g_res)) {
            uint32_t s = 0; uint16_t c = 0;
            for (int z = 0; z < 16; ++z) {
                const uint8_t st = g_res.target_status[z];
                const int16_t mm = g_res.distance_mm[z];
                if ((st == 5 || st == 6 || st == 9) && mm >= 0) { s += mm; ++c; }
            }
            g_dist[i] = c ? static_cast<uint16_t>(s / c) : 0xFFFF;
        }
    }

    if (g_bno_ok && g_since_print >= 300) {
        g_since_print = 0;
        const float yaw = g_bno.getVector(Adafruit_BNO055::VECTOR_EULER).x();
        const float gz  = g_bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE).z();
        Serial.print(F("yaw=")); Serial.print(yaw, 1);
        Serial.print(F(" gyroZ=")); Serial.print(gz, 1); Serial.print(F("dps"));
        Serial.print(F(" | ToF "));
        for (int i = 0; i < 4; ++i) {
            if (g_dist[i] == 0xFFFF) Serial.print(F("---- "));
            else { Serial.print(g_dist[i]); Serial.print(F("mm ")); }
        }
        Serial.println();
    }
}
