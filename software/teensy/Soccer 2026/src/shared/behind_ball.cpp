#include "behind_ball.h"
#include <cmath>

namespace iitasoccer {

namespace {
constexpr float PI_F = 3.14159265358979323846f;

inline float deg_to_rad(float d) { return d * (PI_F / 180.0f); }
inline float rad_to_deg(float r) { return r * (180.0f / PI_F); }

// Normaliza ángulo a [-180, +180].
float wrap_180(float a_deg) {
    while (a_deg > 180.0f)  a_deg -= 360.0f;
    while (a_deg < -180.0f) a_deg += 360.0f;
    return a_deg;
}
}  // namespace

BehindBallTarget compute_behind_ball_target(float ball_x_mm,
                                            float ball_y_mm,
                                            float goal_angle_deg,
                                            float gap_mm) {
    // Vector unitario "del robot hacia el arco" (en coords del robot, con
    // atan2(x, y) → x = sin(θ), y = cos(θ)). Asumimos que la dirección al
    // arco vista desde la pelota ≈ vista desde el robot (válido cuando la
    // distancia al arco >> distancia robot–pelota, que es el caso típico
    // durante toda la cancha).
    const float theta_rad = deg_to_rad(goal_angle_deg);
    const float ux = std::sin(theta_rad);
    const float uy = std::cos(theta_rad);

    // El target está en el lado OPUESTO al arco respecto a la pelota, a `gap`
    // mm de la pelota → target = pelota - gap * u.
    BehindBallTarget out{};
    out.target_x_mm = ball_x_mm - gap_mm * ux;
    out.target_y_mm = ball_y_mm - gap_mm * uy;
    return out;
}

bool is_aligned_to_shoot(float ball_x_mm,
                         float ball_y_mm,
                         float goal_angle_deg,
                         float kick_dist_mm,
                         float kick_angle_deg) {
    const float dist = std::sqrt(ball_x_mm * ball_x_mm + ball_y_mm * ball_y_mm);
    if (dist > kick_dist_mm) return false;
    const float angle_to_goal = std::fabs(wrap_180(goal_angle_deg));
    return angle_to_goal <= kick_angle_deg;
}

bool ball_is_in_attack_line(float ball_x_mm,
                            float ball_y_mm,
                            float goal_angle_deg,
                            float angle_tolerance_deg) {
    // Ángulo robot→pelota.
    const float ball_angle_deg = rad_to_deg(std::atan2(ball_x_mm, ball_y_mm));
    // Diferencia respecto al ángulo robot→arco (wrap [-180, 180]).
    const float diff = wrap_180(ball_angle_deg - goal_angle_deg);
    return std::fabs(diff) <= angle_tolerance_deg;
}

KickoffVelocity compute_kickoff_velocity(float kickoff_speed_mm_s) {
    KickoffVelocity out{};
    out.vx_mm_s = 0.0f;
    out.vy_mm_s = kickoff_speed_mm_s;   // boost recto al frente
    return out;
}

}  // namespace iitasoccer
