// tof_distance_hold.cpp — controlador P de mantenimiento de distancia a pared (ToF).
// Ver contrato/convención de signo en tof_distance_hold.h. Función PURA, sin estado.
#include "tof_distance_hold.h"

namespace iitasoccer {

namespace {
// clamp local (módulo PURO: no dependemos de <algorithm> ni de Arduino).
inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
inline float absf(float v) { return v < 0.0f ? -v : v; }
}  // namespace

TofHoldParams tof_hold_default_params() {
    TofHoldParams p;
    p.kp = 2.0f;              // mm/s por mm de error.
    p.max_speed_mm_s = 400.0f;
    p.deadband_mm = 15.0f;
    return p;
}

TofHoldCmd tof_hold_update(float cur_dist_mm, float target_dist_mm,
                           bool dist_valid, const TofHoldParams& p) {
    TofHoldCmd c;
    c.v_mm_s = 0.0f;
    c.arrived = false;
    c.valid = false;

    // Sin lectura ToF confiable: c ya está en {0, false, false} → fail-safe.
    if (!dist_valid) {
        return c;
    }

    // error > 0 = estoy más lejos que el objetivo -> avanzar (v>0) hacia la pared.
    const float error = cur_dist_mm - target_dist_mm;

    if (absf(error) <= p.deadband_mm) {
        c.v_mm_s = 0.0f;
        c.arrived = true;
        c.valid = true;
        return c;
    }

    c.v_mm_s = clampf(p.kp * error, -p.max_speed_mm_s, p.max_speed_mm_s);
    c.arrived = false;
    c.valid = true;
    return c;
}

}  // namespace iitasoccer
