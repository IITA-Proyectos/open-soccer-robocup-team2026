#include "otos.h"
#include "config_down.h"

#include <Arduino.h>
#include <Wire.h>
#include <cmath>
#include <SparkFun_Qwiic_OTOS_Arduino_Library.h>

// ============================================================================
// SparkFun Qwiic OTOS lib activada 2026-05-24 (TASK-012).
// API: getPosition/getVelocity con sfe_otos_pose2d_t {x, y, h}.
// Unidades seteadas en init: linear=meters, angular=degrees. Convertidos a
// mm y rad/s en otos_tick() para mantener el contrato del resto del firmware.
// Bus físico: U5 → Wire (I²C0, SDA=18 SCL=19); U6 → Wire1 (I²C1, SDA=17 SCL=16).
// (Antes este comentario decía "Wire2 / I2C2" — typo viejo corregido 2026-05-24.)
// ============================================================================

namespace iitasoccer {

namespace {

QwiicOTOS g_otos_left;   // U5 → Wire  (I²C0)
QwiicOTOS g_otos_right;  // U6 → Wire1 (I²C1)

bool  g_left_ready = false;
bool  g_right_ready = false;

float g_x_mm = 0.0f;
float g_y_mm = 0.0f;
float g_heading_deg = 0.0f;
float g_vx = 0.0f, g_vy = 0.0f, g_omega = 0.0f;
float g_slip = 0.0f;

uint32_t g_tick_count = 0;

// Última pose de cada OTOS (usadas para análisis diferencial).
float g_left_x = 0.0f,  g_left_y = 0.0f,  g_left_h = 0.0f;
float g_right_x = 0.0f, g_right_y = 0.0f, g_right_h = 0.0f;

// Última velocidad de cada OTOS.
float g_left_vx = 0.0f,  g_left_vy = 0.0f,  g_left_w = 0.0f;
float g_right_vx = 0.0f, g_right_vy = 0.0f, g_right_w = 0.0f;

}  // namespace

bool otos_init() {
    Wire.begin();
    Wire1.begin();

    g_left_ready = false;
    g_right_ready = false;

    // QwiicOTOS::begin() retorna bool: true si respondió I²C, false si no.
    // Si responde: seteamos unidades a m + deg, calibramos IMU (~0.5 s bloqueante
    // con default numSamples=255), y reseteamos tracking a (0, 0, 0).
    if (NUM_OTOS >= 1) {
        g_left_ready = g_otos_left.begin(Wire);
        if (g_left_ready) {
            g_otos_left.setLinearUnit(kSfeOtosLinearUnitMeters);
            g_otos_left.setAngularUnit(kSfeOtosAngularUnitDegrees);
            g_otos_left.calibrateImu();
            g_otos_left.resetTracking();
        }
    }
    if (NUM_OTOS >= 2) {
        g_right_ready = g_otos_right.begin(Wire1);
        if (g_right_ready) {
            g_otos_right.setLinearUnit(kSfeOtosLinearUnitMeters);
            g_otos_right.setAngularUnit(kSfeOtosAngularUnitDegrees);
            g_otos_right.calibrateImu();
            g_otos_right.resetTracking();
        }
    }

    return g_left_ready || g_right_ready;
}

void otos_tick() {
    g_tick_count++;

    // Lectura I²C de los 2 OTOS. sfe_otos_pose2d_t = {float x, y, h}.
    // Position en metros (m), Velocity en m/s, Heading en grados, ω en deg/s
    // (porque seteamos linear=meters + angular=degrees en otos_init).
    sfe_otos_pose2d_t pl{}, pr{}, vl{}, vr{};
    if (g_left_ready)  { g_otos_left.getPosition(pl);  g_otos_left.getVelocity(vl); }
    if (g_right_ready) { g_otos_right.getPosition(pr); g_otos_right.getVelocity(vr); }
    // Conversión al contrato del firmware (mm, deg, rad/s):
    g_left_x  = pl.x * 1000.0f;  g_left_y  = pl.y * 1000.0f;  g_left_h  = pl.h;
    g_right_x = pr.x * 1000.0f;  g_right_y = pr.y * 1000.0f;  g_right_h = pr.h;
    g_left_vx  = vl.x * 1000.0f; g_left_vy  = vl.y * 1000.0f; g_left_w  = vl.h * (M_PI / 180.0f);
    g_right_vx = vr.x * 1000.0f; g_right_vy = vr.y * 1000.0f; g_right_w = vr.h * (M_PI / 180.0f);

    // === Fusión ===
    if (g_left_ready && g_right_ready) {
        g_x_mm = (g_left_x + g_right_x) * 0.5f;
        g_y_mm = (g_left_y + g_right_y) * 0.5f;
        const float dy = g_right_y - g_left_y;
        g_heading_deg = std::atan2(dy, OTOS_SEPARATION_MM) * (180.0f / M_PI);
        g_vx = (g_left_vx + g_right_vx) * 0.5f;
        g_vy = (g_left_vy + g_right_vy) * 0.5f;
        g_omega = (g_left_w + g_right_w) * 0.5f;
        g_slip = std::abs(g_right_x - g_left_x);
    } else if (g_left_ready) {
        g_x_mm = g_left_x; g_y_mm = g_left_y; g_heading_deg = g_left_h;
        g_vx = g_left_vx; g_vy = g_left_vy; g_omega = g_left_w;
        g_slip = 0.0f;
    } else if (g_right_ready) {
        g_x_mm = g_right_x; g_y_mm = g_right_y; g_heading_deg = g_right_h;
        g_vx = g_right_vx; g_vy = g_right_vy; g_omega = g_right_w;
        g_slip = 0.0f;
    } else {
        g_slip = 0.0f;
    }
}

float otos_get_x_mm()           { return g_x_mm; }
float otos_get_y_mm()           { return g_y_mm; }
float otos_get_heading_deg()    { return g_heading_deg; }
float otos_get_vx_mm_s()        { return g_vx; }
float otos_get_vy_mm_s()        { return g_vy; }
float otos_get_omega_rad_s()    { return g_omega; }
float otos_get_slip_estimate()  { return g_slip; }

void otos_reset() {
    g_x_mm = g_y_mm = g_heading_deg = 0.0f;
    g_vx = g_vy = g_omega = g_slip = 0.0f;
    if (g_left_ready)  g_otos_left.resetTracking();
    if (g_right_ready) g_otos_right.resetTracking();
}

bool     otos_is_left_ready()   { return g_left_ready; }
bool     otos_is_right_ready()  { return g_right_ready; }
uint32_t otos_get_tick_count()  { return g_tick_count; }

}  // namespace iitasoccer
