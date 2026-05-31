// sensors_imu.cpp — Capa HARDWARE de los 2 BNO055 del TOP.
//
// Lee los 2 chips y delega TODA la inteligencia de fusión al módulo PURO
// src/shared/imu_fusion.{h,cpp} (host-testeable). Esta capa solo hace I/O:
//   • leer yaw + gyroZ + calib de cada BNO por I2C,
//   • detectar presencia (ACK en el bus),
//   • ejecutar lo que el módulo pide (soft-resync de un sensor que driftó),
//   • persistir/restaurar la calibración de cada chip en EEPROM (Capa 2).
//
// Arquitectura del hardware (recableado 2026-05-31, confirmado en banco):
//   AMBOS BNO en el bus Wire (18/19). LEFT=0x28 (ADR flotante),
//   RIGHT=0x29 (ADR puenteado a 3V3). Wire1 (24/25) quedó LIBRE para DOWN.
//
// Convención de heading: CCW-positivo (IZQUIERDA sube), [-180,180]. El chip da
// yaw CW-positivo, lo invertimos con HEADING_SIGN. Mismo signo al gyroZ para que
// el test de glitch del módulo (cambio de heading vs gyro*dt) sea consistente.

#include "sensors_imu.h"
#include "config_top.h"
#include "imu_fusion.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <EEPROM.h>
#include <cmath>

namespace iitasoccer {

namespace {

// --- Los 2 BNO en el mismo bus Wire (ver cabecera) ---
Adafruit_BNO055 g_bno_left (55, BNO055_LEFT_I2C_ADDR,  &Wire);
Adafruit_BNO055 g_bno_right(56, BNO055_RIGHT_I2C_ADDR, &Wire);
Adafruit_BNO055* const g_bno[IMU_FUSION_N]  = { &g_bno_left, &g_bno_right };
const uint8_t          g_addr[IMU_FUSION_N] = { BNO055_LEFT_I2C_ADDR, BNO055_RIGHT_I2C_ADDR };

bool  g_ready[IMU_FUSION_N]       = { false, false };
float g_offset[IMU_FUSION_N]      = { 0.0f, 0.0f };  // cero capturado (yaw crudo)
bool  g_calib_saved[IMU_FUSION_N] = { false, false };

// Módulo de fusión (estado + config).
ImuFusion    g_fusion;
ImuFusionCfg g_fcfg;
ImuSensorCfg g_scfg[IMU_FUSION_N];

uint32_t g_last_tick_ms   = 0;
uint32_t g_calib_check_ctr = 0;

constexpr uint32_t INIT_TIMEOUT_MS = 3000;
constexpr uint32_t STABILIZE_MS    = 1000;
constexpr uint32_t GYRO_CALIB_MS   = 2000;
constexpr int      HEADING_SAMPLES = 10;

// Signo del heading. MEDIDO EN BANCO 2026-05-31: el chip da yaw CRECIENTE al
// girar a la DERECHA (CW). La convención del firmware es CCW-positiva. Lo
// invertimos ACÁ, en la fuente, para TODO el firmware. Igual al gyroZ.
constexpr float HEADING_SIGN = -1.0f;

// --- EEPROM (Capa 2): perfil de calibración POR sensor ---
// Región propia (el DOWN usa su propia región en eeprom_calib.cpp; esta base
// 320 no la pisa). Layout: [magic][ver] + por sensor [valid][blob 22B].
constexpr int     EE_BASE         = 320;
constexpr uint8_t EE_MAGIC        = 0xB2;
constexpr uint8_t EE_VERSION      = 1;
constexpr int     BNO_CALIB_BYTES = 22;   // NUM_BNO055_OFFSET_REGISTERS
constexpr int     EE_SENSOR_STRIDE = 1 + BNO_CALIB_BYTES;
int ee_sensor_addr(int i) { return EE_BASE + 2 + i * EE_SENSOR_STRIDE; }

bool ee_header_ok() {
    return EEPROM.read(EE_BASE) == EE_MAGIC && EEPROM.read(EE_BASE + 1) == EE_VERSION;
}
void ee_write_header() {
    EEPROM.update(EE_BASE, EE_MAGIC);
    EEPROM.update(EE_BASE + 1, EE_VERSION);
}
bool ee_load_into_bno(int i, Adafruit_BNO055& bno) {
    if (!ee_header_ok()) return false;
    const int a = ee_sensor_addr(i);
    if (EEPROM.read(a) != 1) return false;
    uint8_t blob[BNO_CALIB_BYTES];
    for (int k = 0; k < BNO_CALIB_BYTES; ++k) blob[k] = EEPROM.read(a + 1 + k);
    bno.setSensorOffsets(blob);
    return true;
}
bool ee_save_from_bno(int i, Adafruit_BNO055& bno) {
    uint8_t blob[BNO_CALIB_BYTES];
    if (!bno.getSensorOffsets(blob)) return false;  // aún no calibrado
    ee_write_header();
    const int a = ee_sensor_addr(i);
    EEPROM.update(a, 1);
    for (int k = 0; k < BNO_CALIB_BYTES; ++k) EEPROM.update(a + 1 + k, blob[k]);
    return true;
}

// --- Lecturas crudas del chip ---
float read_raw_yaw(Adafruit_BNO055& bno) {
    return bno.getVector(Adafruit_BNO055::VECTOR_EULER).x();
}
float read_gyro_z(Adafruit_BNO055& bno) {
    return bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE).z();
}
uint8_t read_gyro_calib(Adafruit_BNO055& bno) {
    uint8_t sys, gyro, accel, mag;
    bno.getCalibration(&sys, &gyro, &accel, &mag);
    return gyro;
}
bool i2c_present(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

float norm180(float h) {
    while (h > 180.0f) h -= 360.0f;
    while (h < -180.0f) h += 360.0f;
    return h;
}
float capture_offset(Adafruit_BNO055& bno) {
    float sum = 0.0f;
    for (int i = 0; i < HEADING_SAMPLES; ++i) { sum += read_raw_yaw(bno); delay(20); }
    return sum / HEADING_SAMPLES;
}
// Heading CCW+, relativo al cero, SIN mount_offset (eso lo aplica el módulo).
float heading_no_mount(int i, float raw_yaw) {
    return norm180(HEADING_SIGN * (raw_yaw - g_offset[i]));
}

bool init_one_bno(Adafruit_BNO055& bno) {
    const uint32_t start = millis();
    while (!bno.begin(OPERATION_MODE_IMUPLUS)) {
        if (millis() - start > INIT_TIMEOUT_MS) return false;
        delay(100);
    }
    bno.setExtCrystalUse(true);
    delay(STABILIZE_MS);
    const uint32_t calib_start = millis();
    while (millis() - calib_start < GYRO_CALIB_MS) {
        if (read_gyro_calib(bno) >= 3) break;
        delay(100);
    }
    return true;
}

}  // namespace

bool sensors_imu_init() {
    Wire.begin();  // Wire1 ya NO se usa acá (recableado 2026-05-31)

    imu_fusion_init(g_fusion);
    g_fcfg = imu_fusion_default_cfg();
    for (int i = 0; i < IMU_FUSION_N; ++i) g_scfg[i] = imu_fusion_default_sensor_cfg();

    Serial.println("[IMU] Init BNO055 LEFT (Wire @ 0x28)...");
    g_ready[0] = init_one_bno(g_bno_left);
    Serial.println(g_ready[0] ? "[IMU] LEFT OK" : "[IMU] LEFT FAIL (continuando)");

    Serial.println("[IMU] Init BNO055 RIGHT (Wire @ 0x29)...");
    g_ready[1] = init_one_bno(g_bno_right);
    Serial.println(g_ready[1] ? "[IMU] RIGHT OK" : "[IMU] RIGHT FAIL (continuando)");

    // EEPROM (Capa 2): restaurar perfil de calibración de cada chip si existe.
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        if (g_ready[i] && ee_load_into_bno(i, *g_bno[i])) {
            Serial.print("[IMU] calib restaurada de EEPROM para sensor ");
            Serial.println(i);
        }
    }

    // Cero de cada sensor (robot debe apuntar al arco rival al boot).
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        if (g_ready[i]) g_offset[i] = capture_offset(*g_bno[i]);
    }

    g_last_tick_ms = millis();
    return g_ready[0] || g_ready[1];
}

void sensors_imu_tick() {
    const uint32_t now = millis();
    float dt_s = (now - g_last_tick_ms) / 1000.0f;
    g_last_tick_ms = now;
    if (dt_s <= 0.0f || dt_s > 1.0f) dt_s = 0.01f;  // clamp arranque/saltos

    ImuSample in[IMU_FUSION_N];
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        const bool present = g_ready[i] && i2c_present(g_addr[i]);
        if (present) {
            in[i].present     = true;
            in[i].heading_deg = heading_no_mount(i, read_raw_yaw(*g_bno[i]));
            in[i].gyro_z_dps  = HEADING_SIGN * read_gyro_z(*g_bno[i]);
            in[i].calib_gyro  = read_gyro_calib(*g_bno[i]);
        } else {
            in[i].present = false; in[i].heading_deg = 0.0f;
            in[i].gyro_z_dps = 0.0f; in[i].calib_gyro = 0;
        }
    }

    imu_fusion_update(g_fusion, g_fcfg, g_scfg, in, dt_s);

    // Pedidos de RESET del módulo: SOFT-RESYNC no bloqueante (re-cero del sensor
    // que driftó, alineado al más estable). NO hace begin() en el loop.
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        if (g_fusion.s[i].request_reset && g_ready[i]) {
            const float reseed = g_fusion.s[i].reseed_heading;
            const float raw    = read_raw_yaw(*g_bno[i]);
            const float target_no_mount = reseed - g_scfg[i].mount_offset_deg;
            // heading = SIGN*(raw - offset) = target  =>  offset = raw - target/SIGN
            g_offset[i] = raw - target_no_mount / HEADING_SIGN;
            imu_fusion_clear_reset(g_fusion, i);
            Serial.print("[IMU] soft-resync sensor ");
            Serial.print(i);
            Serial.println(" (drifto; re-alineado con el estable)");
        }
    }

    // Auto-guardar calib a EEPROM al llegar a fully-calibrated (chequeo barato
    // cada ~200 ticks para no saturar el bus).
    if (++g_calib_check_ctr >= 200) {
        g_calib_check_ctr = 0;
        for (int i = 0; i < IMU_FUSION_N; ++i) {
            if (g_ready[i] && !g_calib_saved[i] && ee_save_from_bno(i, *g_bno[i])) {
                g_calib_saved[i] = true;
                Serial.print("[IMU] calib guardada en EEPROM para sensor ");
                Serial.println(i);
            }
        }
    }
}

float sensors_imu_get_heading_deg()       { return g_fusion.fused_heading_deg; }
float sensors_imu_get_left_heading_deg()  { return g_fusion.s[0].heading_deg; }
float sensors_imu_get_right_heading_deg() { return g_fusion.s[1].heading_deg; }

int16_t sensors_imu_get_heading_centideg() {
    return static_cast<int16_t>(sensors_imu_get_heading_deg() * 100.0f);
}

float sensors_imu_get_disagreement_deg() { return g_fusion.disagreement_deg; }

void sensors_imu_recalibrate_zero() {
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        if (g_ready[i]) g_offset[i] = capture_offset(*g_bno[i]);
    }
    imu_fusion_init(g_fusion);  // limpia estado de fusión (drift, glitch, etc)
    g_last_tick_ms = millis();
}

bool sensors_imu_save_calibration() {
    bool any = false;
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        if (g_ready[i] && ee_save_from_bno(i, *g_bno[i])) { g_calib_saved[i] = true; any = true; }
    }
    return any;
}

bool sensors_imu_left_ready()  { return g_ready[0]; }
bool sensors_imu_right_ready() { return g_ready[1]; }

}  // namespace iitasoccer
