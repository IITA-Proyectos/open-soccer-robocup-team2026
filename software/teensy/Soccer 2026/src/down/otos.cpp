#include "otos.h"
#include "config_down.h"

#include <Arduino.h>
#include <Wire.h>
#include <cmath>

// ============================================================================
// La librería oficial de SparkFun es "SparkFun_Qwiic_OTOS_Arduino_Library".
// Hasta confirmar instalación en platformio.ini, dejamos un stub que reporta
// "no disponible" y todos los datos retornan 0. Cuando la lib esté instalada,
// reemplazar el bloque ENTRE las marcas TODO_OTOS_LIB.
// ============================================================================

// TODO_OTOS_LIB_BEGIN
//
// #include <SparkFun_Qwiic_OTOS_Arduino_Library.h>
// static QwiicOTOS g_otos_left;   // I2C0 (Wire)
// static QwiicOTOS g_otos_right;  // I2C2 (Wire2)
//
// TODO_OTOS_LIB_END

namespace iitasoccer {

namespace {

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

}  // namespace

bool otos_init() {
    Wire.begin();
    Wire1.begin();

    // STUB: hasta que SparkFun OTOS lib esté instalada, asumir que respondieron.
    // Reemplazar por las llamadas reales a la lib SparkFun cuando esté instalada.
    g_left_ready = (NUM_OTOS >= 1);
    g_right_ready = (NUM_OTOS >= 2);

    // TODO_OTOS_LIB_BEGIN
    // if (NUM_OTOS >= 1) {
    //     g_otos_left.begin(Wire);
    //     g_otos_left.calibrateImu();
    //     g_otos_left.resetTracking();
    //     g_left_ready = g_otos_left.isConnected();
    // }
    // if (NUM_OTOS >= 2) {
    //     g_otos_right.begin(Wire1);
    //     g_otos_right.calibrateImu();
    //     g_otos_right.resetTracking();
    //     g_right_ready = g_otos_right.isConnected();
    // }
    // TODO_OTOS_LIB_END

    return g_left_ready || g_right_ready;
}

void otos_tick() {
    g_tick_count++;

    // TODO_OTOS_LIB_BEGIN
    // sfe_otos_pose2d_t pl{}, pr{};
    // sfe_otos_velocity2d_t vl{}, vr{};
    // if (g_left_ready)  { g_otos_left.getPose(pl);  g_otos_left.getVelocity(vl); }
    // if (g_right_ready) { g_otos_right.getPose(pr); g_otos_right.getVelocity(vr); }
    // g_left_x = pl.x * 25.4f; g_left_y = pl.y * 25.4f; g_left_h = pl.h * 180.0f / M_PI;
    // g_right_x = pr.x * 25.4f; g_right_y = pr.y * 25.4f; g_right_h = pr.h * 180.0f / M_PI;
    // TODO_OTOS_LIB_END

    // === Fusión ===
    if (g_left_ready && g_right_ready) {
        // Pose del centro = promedio de los 2.
        g_x_mm = (g_left_x + g_right_x) * 0.5f;
        g_y_mm = (g_left_y + g_right_y) * 0.5f;

        // Heading inferido de la diferencia Y entre OTOS izq y der separados por
        // OTOS_SEPARATION_MM (configurable).
        const float dy = g_right_y - g_left_y;
        g_heading_deg = std::atan2(dy, OTOS_SEPARATION_MM) * (180.0f / M_PI);

        // Slip estimate: diferencia X esperada por la rotación es
        // |omega| * radio_a_otos. Si la diferencia X observada es mayor, hay slip.
        // Para una estimación simple sin omega instantáneo: usar diff_x / dt como
        // proxy en el firmware más completo. Por ahora, slip = |diff_x|.
        g_slip = std::abs(g_right_x - g_left_x);
    } else if (g_left_ready) {
        g_x_mm = g_left_x; g_y_mm = g_left_y; g_heading_deg = g_left_h;
        g_slip = 0.0f;
    } else if (g_right_ready) {
        g_x_mm = g_right_x; g_y_mm = g_right_y; g_heading_deg = g_right_h;
        g_slip = 0.0f;
    } else {
        // Ningún OTOS — degradación: pose no se actualiza.
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
    // TODO_OTOS_LIB_BEGIN
    // if (g_left_ready)  g_otos_left.resetTracking();
    // if (g_right_ready) g_otos_right.resetTracking();
    // TODO_OTOS_LIB_END
}

bool     otos_is_left_ready()   { return g_left_ready; }
bool     otos_is_right_ready()  { return g_right_ready; }
uint32_t otos_get_tick_count()  { return g_tick_count; }

}  // namespace iitasoccer
