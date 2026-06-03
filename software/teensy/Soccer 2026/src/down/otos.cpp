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
// Heading inferido de la diferencia Y entre OTOS (atan2(dy, sep)). SOLO diagnóstico:
// saturado a (-90,+90); NO se usa como heading autoritativo. Ver M2 en otos_tick().
float g_heading_diag_deg = 0.0f;
float g_vx = 0.0f, g_vy = 0.0f, g_omega = 0.0f;
float g_slip = 0.0f;

uint32_t g_tick_count = 0;

// Última pose de cada OTOS (usadas para análisis diferencial).
float g_left_x = 0.0f,  g_left_y = 0.0f,  g_left_h = 0.0f;
float g_right_x = 0.0f, g_right_y = 0.0f, g_right_h = 0.0f;

// Última velocidad de cada OTOS. La lib SparkFun YA está activa (TASK-012,
// 2026-05-24): otos_tick() lee getVelocity() y llena estos campos, y la fusión
// de abajo computa g_vx/g_vy/g_omega con datos reales. (Antes, con la lib
// comentada, quedaban en 0 — bug latente ya cerrado.)
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
        // Pose del centro = promedio de los 2.
        g_x_mm = (g_left_x + g_right_x) * 0.5f;
        g_y_mm = (g_left_y + g_right_y) * 0.5f;

        // --- M2: Heading dual por PROMEDIO CIRCULAR de los headings ABSOLUTOS ---
        // Cada OTOS tiene su propio heading absoluto (g_left_h / g_right_h, en grados,
        // wrap ±180) medido por su IMU interna. El método viejo
        //   atan2(g_right_y - g_left_y, OTOS_SEPARATION_MM)
        // estaba MAL por dos razones:
        //   (1) saturaba a (-90°, +90°) → no podía representar heading completo ±180°,
        //   (2) descartaba el heading absoluto que cada OTOS ya entrega.
        // Fix: promedio circular de los dos headings absolutos. El promedio circular
        // maneja el wrap ±180° correctamente (p.ej. promediar +170° y -170° da ±180°,
        // no 0° como haría un promedio aritmético):
        //   h = atan2(sin(hl)+sin(hr), cos(hl)+cos(hr))
        const float hl_rad = g_left_h  * (M_PI / 180.0f);
        const float hr_rad = g_right_h * (M_PI / 180.0f);
        const float sin_sum = std::sin(hl_rad) + std::sin(hr_rad);
        const float cos_sum = std::cos(hl_rad) + std::cos(hr_rad);
        g_heading_deg = std::atan2(sin_sum, cos_sum) * (180.0f / M_PI);  // ∈ (-180, 180]

        // DIAGNÓSTICO ÚNICAMENTE (NO es el heading): el ángulo inferido de la
        // diferencia Y entre los OTOS separados por OTOS_SEPARATION_MM. Saturado a
        // (-90°, +90°). Útil solo para chequear coherencia con g_heading_deg en banco.
        g_heading_diag_deg =
            std::atan2(g_right_y - g_left_y, OTOS_SEPARATION_MM) * (180.0f / M_PI);

        // Velocidad del centro = promedio de los 2 (la lib SparkFun ya está
        // activa, así que estos son valores reales de getVelocity()).
        g_vx = (g_left_vx + g_right_vx) * 0.5f;
        g_vy = (g_left_vy + g_right_vy) * 0.5f;
        g_omega = (g_left_w + g_right_w) * 0.5f;

        // --- M3: Slip estimate desde VELOCIDADES (NO desde posición integrada) ---
        // El método viejo, g_slip = |g_right_x - g_left_x|, usaba POSICIÓN integrada:
        // esa diferencia crece monótona con la deriva de cada OTOS y se satura a 255
        // (clamp de abajo) → inservible.
        //
        // Físico: si el robot rota a ω (rad/s), los dos OTOS (separados D = OTOS_
        // SEPARATION_MM, uno a cada costado) ven velocidades laterales (eje X local)
        // que difieren por la rotación pura en:
        //   Δvx_esperado = ω * D            [rad/s * mm = mm/s]
        // (right está a +D/2 del centro y left a -D/2; la contribución relativa de la
        // rotación a vx_right - vx_left es ω·D). Lo que NO se explica por esa rotación
        // es slip lateral (ruedas patinando, p.ej. al patear):
        //   slip = | (g_right_vx - g_left_vx) - ω * D |     [mm/s]
        // Usamos g_omega (rad/s, ya promediado arriba) como estimador de ω del cuerpo.
        //
        // Propiedades: ACOTADO (depende de velocidades instantáneas, no integra) →
        // NO es monótono, vuelve a ~0 cuando cesa el patinaje. Unidad: mm/s.
        // Signo/eje: se asume eje X local = lateral y que +ω·D corresponde al término
        // de (right - left); CONFIRMAR EN BANCO el signo real (ver validación).
        const float dvx_rotation = g_omega * OTOS_SEPARATION_MM;  // mm/s esperados por rotación
        g_slip = std::abs((g_right_vx - g_left_vx) - dvx_rotation);
    } else if (g_left_ready) {
        g_x_mm = g_left_x; g_y_mm = g_left_y; g_heading_deg = g_left_h;
        g_vx = g_left_vx; g_vy = g_left_vy; g_omega = g_left_w;
        g_slip = 0.0f;
    } else if (g_right_ready) {
        g_x_mm = g_right_x; g_y_mm = g_right_y; g_heading_deg = g_right_h;
        g_vx = g_right_vx; g_vy = g_right_vy; g_omega = g_right_w;
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
    if (g_left_ready)  g_otos_left.resetTracking();
    if (g_right_ready) g_otos_right.resetTracking();
}

bool     otos_is_left_ready()   { return g_left_ready; }
bool     otos_is_right_ready()  { return g_right_ready; }
uint32_t otos_get_tick_count()  { return g_tick_count; }

// Diagnóstico: heading inferido de la diferencia Y (atan2(dy, sep)), saturado a
// (-90,+90). NO es el heading autoritativo (ese es otos_get_heading_deg, promedio
// circular). Útil en banco para contrastar contra el heading real con 2 OTOS.
float    otos_get_heading_diag_deg() { return g_heading_diag_deg; }

}  // namespace iitasoccer
