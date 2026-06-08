// gk_motion_speed.cpp — implementación pura (host-testeable) del helper de velocidad-objetivo.
#include "gk_motion_speed.h"

namespace iitasoccer {

namespace {
constexpr float FWD_FRONT_FACTOR = 0.866025f;  // |cos(330°)| = |cos(210°)| en avance
}

float gk_clamp_strafe_speed_to_cap(float vx_mm_s, float max_speed_mm_s, int max_pwm,
                                   int pwm_cap) {
    if (max_speed_mm_s <= 0.0f || max_pwm <= 0 || pwm_cap <= 0) return vx_mm_s;
    // PWM de la rueda dominante (trasera, factor 1.0) a esta vx.
    const float rear_pwm = (GK_STRAFE_REAR_FACTOR * vx_mm_s / max_speed_mm_s)
                         * static_cast<float>(max_pwm);
    const float mag = rear_pwm < 0.0f ? -rear_pwm : rear_pwm;
    if (mag <= static_cast<float>(pwm_cap)) return vx_mm_s;
    // Recortar: la vx que deja la trasera EXACTO en el cap (sign-preserving).
    const float vx_cap = static_cast<float>(pwm_cap) * max_speed_mm_s
                       / (GK_STRAFE_REAR_FACTOR * static_cast<float>(max_pwm));
    return vx_mm_s < 0.0f ? -vx_cap : vx_cap;
}

float gk_strafe_speed_from_front_pwm(int front_pwm, float max_speed_mm_s, int max_pwm,
                                     int pwm_cap) {
    if (max_speed_mm_s <= 0.0f || max_pwm <= 0) return 0.0f;
    const float vx = static_cast<float>(front_pwm) * max_speed_mm_s
                   / (GK_STRAFE_FRONT_FACTOR * static_cast<float>(max_pwm));
    return gk_clamp_strafe_speed_to_cap(vx, max_speed_mm_s, max_pwm, pwm_cap);
}

float gk_forward_speed_from_front_pwm(int front_pwm, float max_speed_mm_s, int max_pwm,
                                      int pwm_cap) {
    if (max_speed_mm_s <= 0.0f || max_pwm <= 0) return 0.0f;
    float vy = static_cast<float>(front_pwm) * max_speed_mm_s
             / (FWD_FRONT_FACTOR * static_cast<float>(max_pwm));
    // En avance la rueda dominante es la delantera (la trasera está en 0) → cap por delantera.
    const float front_pwm_at = (FWD_FRONT_FACTOR * vy / max_speed_mm_s)
                             * static_cast<float>(max_pwm);
    if (pwm_cap > 0 && front_pwm_at > static_cast<float>(pwm_cap)) {
        vy = static_cast<float>(pwm_cap) * max_speed_mm_s
           / (FWD_FRONT_FACTOR * static_cast<float>(max_pwm));
    }
    return vy;
}

}  // namespace iitasoccer
