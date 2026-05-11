#include "sensors_imu.h"
#include "config_top.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <cmath>

namespace iitasoccer {

namespace {

// Dos instancias del BNO055 — una en cada bus I2C.
// Adafruit_BNO055 constructor: (sensorID, address, &Wire_instance).
Adafruit_BNO055 g_imu_left (55, BNO055_LEFT_I2C_ADDR,  &Wire);
Adafruit_BNO055 g_imu_right(56, BNO055_RIGHT_I2C_ADDR, &Wire1);

bool  g_left_ready = false;
bool  g_right_ready = false;

float g_left_offset = 0.0f;
float g_right_offset = 0.0f;

float g_left_heading = 0.0f;
float g_right_heading = 0.0f;

constexpr uint32_t INIT_TIMEOUT_MS    = 3000;
constexpr uint32_t STABILIZE_MS       = 1000;
constexpr uint32_t GYRO_CALIB_MS      = 2000;
constexpr int      HEADING_SAMPLES    = 10;

bool init_one_bno(Adafruit_BNO055& bno) {
    const uint32_t start = millis();
    while (!bno.begin(Adafruit_BNO055::OPERATION_MODE_IMUPLUS)) {
        if (millis() - start > INIT_TIMEOUT_MS) return false;
        delay(100);
    }
    bno.setExtCrystalUse(true);
    delay(STABILIZE_MS);

    // Esperar calibración del gyro (no bloquea forever).
    const uint32_t calib_start = millis();
    while (millis() - calib_start < GYRO_CALIB_MS) {
        uint8_t sys, gyro, accel, mag;
        bno.getCalibration(&sys, &gyro, &accel, &mag);
        if (gyro >= 3) break;
        delay(100);
    }
    return true;
}

float read_raw_yaw(Adafruit_BNO055& bno) {
    sensors_event_t event;
    bno.getEvent(&event);
    return event.orientation.x;
}

float capture_offset(Adafruit_BNO055& bno) {
    float sum = 0.0f;
    for (int i = 0; i < HEADING_SAMPLES; ++i) {
        sum += read_raw_yaw(bno);
        delay(20);
    }
    return sum / HEADING_SAMPLES;
}

float normalize_heading(float h) {
    while (h > 180.0f) h -= 360.0f;
    while (h < -180.0f) h += 360.0f;
    return h;
}

}  // namespace

bool sensors_imu_init() {
    // I2C bus 0 (Wire) — pines default 18/19. No requiere remap.
    Wire.begin();

    // I2C bus 1 (Wire1) — REMAPEADO a pines 24/25 (Q3 resolución).
    Wire1.setSCL(WIRE1_SCL_PIN);
    Wire1.setSDA(WIRE1_SDA_PIN);
    Wire1.begin();

    Serial.println("[IMU] Init BNO055 LEFT (Wire / I2C0)...");
    g_left_ready = init_one_bno(g_imu_left);
    Serial.println(g_left_ready ? "[IMU] LEFT OK" : "[IMU] LEFT FAIL (continuando)");

    Serial.println("[IMU] Init BNO055 RIGHT (Wire1 / I2C1)...");
    g_right_ready = init_one_bno(g_imu_right);
    Serial.println(g_right_ready ? "[IMU] RIGHT OK" : "[IMU] RIGHT FAIL (continuando)");

    if (g_left_ready)  g_left_offset  = capture_offset(g_imu_left);
    if (g_right_ready) g_right_offset = capture_offset(g_imu_right);

    return g_left_ready || g_right_ready;
}

void sensors_imu_tick() {
    if (g_left_ready) {
        g_left_heading = normalize_heading(read_raw_yaw(g_imu_left) - g_left_offset);
    }
    if (g_right_ready) {
        g_right_heading = normalize_heading(read_raw_yaw(g_imu_right) - g_right_offset);
    }
}

float sensors_imu_get_heading_deg() {
    // Preferir LEFT si está OK; fallback a RIGHT; 0 si ambos fallaron.
    if (g_left_ready)  return g_left_heading;
    if (g_right_ready) return g_right_heading;
    return 0.0f;
}

float sensors_imu_get_disagreement_deg() {
    if (!g_left_ready || !g_right_ready) return 0.0f;
    float diff = g_left_heading - g_right_heading;
    return std::abs(normalize_heading(diff));
}

void sensors_imu_recalibrate_zero() {
    if (g_left_ready)  g_left_offset  = capture_offset(g_imu_left);
    if (g_right_ready) g_right_offset = capture_offset(g_imu_right);
}

bool sensors_imu_left_ready()  { return g_left_ready; }
bool sensors_imu_right_ready() { return g_right_ready; }

}  // namespace iitasoccer
