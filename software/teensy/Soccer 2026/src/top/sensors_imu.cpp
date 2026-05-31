#include "sensors_imu.h"
#include "config_top.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <cmath>

namespace iitasoccer {

namespace {

// Dos instancias del BNO055 en el MISMO bus Wire (recableado 2026-05-31).
// LEFT  en 0x28 (ADR a GND/flotante), RIGHT en 0x29 (ADR puenteado a 3V3).
// Confirmado en banco con diag_bno_addr_check (chip-id 0xA0 en ambas dirs).
// Antes RIGHT vivía en Wire1; el recableado lo movió a Wire para LIBERAR
// Wire1 (24/25) para la placa DOWN. Adafruit_BNO055 ctor: (id, addr, &bus).
Adafruit_BNO055 g_imu_left (55, BNO055_LEFT_I2C_ADDR,  &Wire);
Adafruit_BNO055 g_imu_right(56, BNO055_RIGHT_I2C_ADDR, &Wire);

bool  g_left_ready = false;
bool  g_right_ready = false;

float g_left_offset = 0.0f;
float g_right_offset = 0.0f;

float g_left_heading = 0.0f;
float g_right_heading = 0.0f;
float g_fused_heading = 0.0f;   // resultado de la fusión circular (CCW+)

constexpr uint32_t INIT_TIMEOUT_MS    = 3000;
constexpr uint32_t STABILIZE_MS       = 1000;
constexpr uint32_t GYRO_CALIB_MS      = 2000;
constexpr int      HEADING_SAMPLES    = 10;

// Si los 2 BNO difieren más que esto, NO fusionamos a ciegas: uno está
// fallando o hubo un impacto. En ese caso preferimos el LEFT (referencia
// histórica) y dejamos el desacuerdo visible vía get_disagreement_deg().
constexpr float    DISAGREE_MAX_DEG   = 30.0f;

// Signo del heading. MEDIDO EN BANCO 2026-05-31 (diag_top_bno): el BNO055 de
// esta placa entrega Euler yaw CRECIENTE al girar a la DERECHA (horario / CW):
// girar 90° a la derecha -> +90, a la izquierda -> -90.
// Pero la convención CANÓNICA del firmware (localization.cpp::classify_wall +
// TOF_MOUNT_ANGLE_DEG en pinout_common.h) es CCW-positiva: girar a la IZQUIERDA
// SUBE el heading. localization.cpp ya advertía esto en su comentario ("si el
// BNO da CW positivo, invertir el signo"). Lo invertimos ACÁ, en la FUENTE, para
// que TODO el firmware (snapshot a CENTRAL, HeadingPID del CENTRAL, localización)
// reciba heading CCW-positivo y consistente. NO volver a invertir aguas abajo.
// Si en el futuro se monta el chip al revés (boca abajo), revisar este signo.
constexpr float HEADING_SIGN = -1.0f;

bool init_one_bno(Adafruit_BNO055& bno) {
    const uint32_t start = millis();
    // OPERATION_MODE_IMUPLUS es enum global de adafruit_bno055_opmode_t,
    // NO member estático de la clase (fix de build 2026-05-19).
    while (!bno.begin(OPERATION_MODE_IMUPLUS)) {
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

// Diferencia circular a-b en [-180,180]. NO restar crudo: rompe en ±180.
float heading_diff(float a, float b) { return normalize_heading(a - b); }

// Promedio circular de dos headings (maneja el wraparound -179/+179 con atan2;
// el promedio aritmético daría 0 para 179 y -179, que es justo lo contrario).
float fuse_circular(float a_deg, float b_deg) {
    const float ar = a_deg * static_cast<float>(M_PI) / 180.0f;
    const float br = b_deg * static_cast<float>(M_PI) / 180.0f;
    const float s = std::sin(ar) + std::sin(br);
    const float c = std::cos(ar) + std::cos(br);
    return std::atan2(s, c) * 180.0f / static_cast<float>(M_PI);
}

}  // namespace

bool sensors_imu_init() {
    // Recableado 2026-05-31: AMBOS BNO en el bus Wire (18/19), direcciones
    // distintas (LEFT 0x28, RIGHT 0x29). Wire1 quedó LIBRE para la placa DOWN,
    // así que ya NO lo inicializamos acá.
    Wire.begin();

    Serial.println("[IMU] Init BNO055 LEFT (Wire @ 0x28)...");
    g_left_ready = init_one_bno(g_imu_left);
    Serial.println(g_left_ready ? "[IMU] LEFT OK" : "[IMU] LEFT FAIL (continuando)");

    Serial.println("[IMU] Init BNO055 RIGHT (Wire @ 0x29)...");
    g_right_ready = init_one_bno(g_imu_right);
    Serial.println(g_right_ready ? "[IMU] RIGHT OK" : "[IMU] RIGHT FAIL (continuando)");

    if (g_left_ready)  g_left_offset  = capture_offset(g_imu_left);
    if (g_right_ready) g_right_offset = capture_offset(g_imu_right);

    return g_left_ready || g_right_ready;
}

void sensors_imu_tick() {
    // HEADING_SIGN invierte el yaw crudo (CW-positivo del chip) a la convención
    // CCW-positiva del firmware. Ver nota en la sección de constantes.
    if (g_left_ready) {
        g_left_heading = normalize_heading(
            HEADING_SIGN * (read_raw_yaw(g_imu_left) - g_left_offset));
    }
    if (g_right_ready) {
        g_right_heading = normalize_heading(
            HEADING_SIGN * (read_raw_yaw(g_imu_right) - g_right_offset));
    }

    // --- Fusión del heading ---
    // 2 BNO OK y de acuerdo  -> promedio circular (más estable, menos ruido).
    // 2 BNO OK pero discrepan -> hay falla/impacto: NO promediar (daría un valor
    //                            intermedio falso); preferir LEFT como referencia.
    // 1 BNO                   -> el que esté listo.
    // ninguno                 -> 0.
    if (g_left_ready && g_right_ready) {
        if (std::abs(heading_diff(g_left_heading, g_right_heading)) <= DISAGREE_MAX_DEG) {
            g_fused_heading = fuse_circular(g_left_heading, g_right_heading);
        } else {
            g_fused_heading = g_left_heading;  // desacuerdo: referencia LEFT
        }
    } else if (g_left_ready) {
        g_fused_heading = g_left_heading;
    } else if (g_right_ready) {
        g_fused_heading = g_right_heading;
    } else {
        g_fused_heading = 0.0f;
    }
}

float sensors_imu_get_heading_deg() {
    // Heading principal del robot = heading FUSIONADO (ver sensors_imu_tick).
    // Con 2 BNO sanos es el promedio circular; degrada solo a 1 BNO; 0 si fallan.
    return g_fused_heading;
}

float sensors_imu_get_left_heading_deg()  { return g_left_heading; }
float sensors_imu_get_right_heading_deg() { return g_right_heading; }

int16_t sensors_imu_get_heading_centideg() {
    // Wrapper: misma logica que get_heading_deg() pero en centidegrees.
    // El heading ya viene normalizado a [-180, 180] desde el tick.
    float heading_deg = sensors_imu_get_heading_deg();
    return static_cast<int16_t>(heading_deg * 100.0f);
}

float sensors_imu_get_disagreement_deg() {
    // Diferencia circular |L - R| en [0,180]. > ~5° en reposo = sospecha
    // (impacto/interferencia/falla). El tick deja de promediar si supera
    // DISAGREE_MAX_DEG. 0 si no hay 2 BNO para comparar.
    if (!g_left_ready || !g_right_ready) return 0.0f;
    return std::abs(heading_diff(g_left_heading, g_right_heading));
}

void sensors_imu_recalibrate_zero() {
    if (g_left_ready)  g_left_offset  = capture_offset(g_imu_left);
    if (g_right_ready) g_right_offset = capture_offset(g_imu_right);
}

bool sensors_imu_left_ready()  { return g_left_ready; }
bool sensors_imu_right_ready() { return g_right_ready; }

}  // namespace iitasoccer
