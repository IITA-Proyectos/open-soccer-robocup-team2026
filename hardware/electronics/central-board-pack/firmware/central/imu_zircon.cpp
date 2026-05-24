#include "imu_zircon.h"
#include "config_central.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

namespace iitasoccer {

namespace {

Adafruit_BNO055 g_bno(55, BNO055_I2C_ADDR);
float g_heading_offset = 0.0f;
bool  g_ready = false;

float read_raw_yaw() {
    sensors_event_t event;
    g_bno.getEvent(&event);
    return event.orientation.x;
}

void capture_heading_offset() {
    float sum = 0.0f;
    for (int i = 0; i < BNO055_HEADING_SAMPLES; ++i) {
        sum += read_raw_yaw();
        delay(20);
    }
    g_heading_offset = sum / static_cast<float>(BNO055_HEADING_SAMPLES);
}

bool wait_for_gyro_calibration() {
    const uint32_t start = millis();
    while (millis() - start < BNO055_GYRO_CALIB_MS) {
        uint8_t sys, gyro, accel, mag;
        g_bno.getCalibration(&sys, &gyro, &accel, &mag);
        if (gyro >= 3) return true;
        delay(100);
    }
    return false;  // timeout — seguir igual, calibración parcial es mejor que nada
}

}  // namespace

bool imu_init() {
    const uint32_t start = millis();

    // Paso 1: Detectar sensor con timeout (NO while(1) — degradación elegante).
    // OPERATION_MODE_IMUPLUS es enum global de adafruit_bno055_opmode_t,
    // NO member estático de la clase (fix de build 2026-05-19).
    while (!g_bno.begin(OPERATION_MODE_IMUPLUS)) {
        if (millis() - start > BNO055_INIT_TIMEOUT_MS) {
            g_ready = false;
            return false;
        }
        delay(100);
    }

    // Paso 2: Cristal externo (si está disponible).
    g_bno.setExtCrystalUse(true);

    // Paso 3: Estabilización post-init (datasheet: mín. 650ms; usamos 1000ms).
    delay(BNO055_STABILIZE_MS);

    // Paso 4: Esperar calibración del gyro (robot debe estar quieto).
    wait_for_gyro_calibration();

    // Paso 5: Capturar heading inicial promediando.
    capture_heading_offset();

    g_ready = true;
    return true;
}

bool imu_is_ready() { return g_ready; }

float imu_get_heading() {
    if (!g_ready) return 0.0f;
    float heading = read_raw_yaw() - g_heading_offset;
    // Normalizar a [-180, +180]
    while (heading > 180.0f)  heading -= 360.0f;
    while (heading < -180.0f) heading += 360.0f;
    return heading;
}

void imu_recalibrate_heading_zero() {
    if (!g_ready) return;
    capture_heading_offset();
}

}  // namespace iitasoccer
