#include "kinematics.h"
#include <cmath>

namespace iitasoccer {

WheelSpeeds inverse_kinematics(float vx_mm_s, float vy_mm_s, float omega_rad_s,
                                const WheelConfig wheels[3]) {
    WheelSpeeds ws{};
    for (int i = 0; i < 3; ++i) {
        const float theta = wheels[i].angle_rad;
        const float R = wheels[i].radius_mm;
        ws.wheel[i] = -vx_mm_s * std::sin(theta)
                    +  vy_mm_s * std::cos(theta)
                    +  omega_rad_s * R;
    }
    return ws;
}

int wheel_speed_to_pwm(float speed_mm_s, float max_speed_mm_s, int max_pwm) {
    if (max_speed_mm_s <= 0.0f) return 0;
    float pwm_f = (speed_mm_s / max_speed_mm_s) * static_cast<float>(max_pwm);
    if (pwm_f > static_cast<float>(max_pwm)) pwm_f = static_cast<float>(max_pwm);
    if (pwm_f < -static_cast<float>(max_pwm)) pwm_f = -static_cast<float>(max_pwm);
    return static_cast<int>(pwm_f);
}

void saturate_wheels(WheelSpeeds& ws, float max_speed_mm_s) {
    if (max_speed_mm_s <= 0.0f) return;
    float max_abs = 0.0f;
    for (int i = 0; i < 3; ++i) {
        const float a = std::abs(ws.wheel[i]);
        if (a > max_abs) max_abs = a;
    }
    if (max_abs <= max_speed_mm_s) return;
    const float scale = max_speed_mm_s / max_abs;
    for (int i = 0; i < 3; ++i) {
        ws.wheel[i] *= scale;
    }
}

}  // namespace iitasoccer
