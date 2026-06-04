#include "behind_ball_abs.h"
#include <cmath>

namespace iitasoccer {

namespace {

constexpr float PI_F = 3.14159265358979323846f;
constexpr float RAD_TO_CENTIDEG = 18000.0f / PI_F;  // rad → grados*100

// Normaliza un ángulo en centideg al rango (-18000, +18000].
float wrap_centideg(float c) {
    while (c >  18000.0f) c -= 36000.0f;
    while (c <= -18000.0f) c += 36000.0f;
    return c;
}

// Heading (centideg) que deja al robot mirando hacia el vector de cancha
// (dx, dy). En el frame de cancha (+Y=frente del heading 0, +X=derecha) y con el
// heading creciendo CCW, el heading deseado es -atan2(dx, dy):
//   atan2(dx,dy) mide desde +Y creciendo hacia +X (CW); el heading crece CCW
//   (signo opuesto) → se niega. Ver behind_ball_abs.h y CONVENCION-EJES §1/§2.
float heading_to_centideg(float dx, float dy) {
    return wrap_centideg(-std::atan2(dx, dy) * RAD_TO_CENTIDEG);
}

// Clampea un valor float a [lo, hi] y lo devuelve como int16 (redondeado).
int16_t clamp_to_int16(float v, float lo, float hi) {
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return static_cast<int16_t>(std::lround(v));
}

}  // namespace

BehindBallAbsOut behind_ball_abs(const BehindBallAbsIn& in,
                                 int16_t align_tol_centideg) {
    BehindBallAbsOut out{};

    const float ball_x = static_cast<float>(in.ball_x_mm);
    const float ball_y = static_cast<float>(in.ball_y_mm);
    const float goal_x = static_cast<float>(in.opp_goal_x_mm);
    const float goal_y = static_cast<float>(in.opp_goal_y_mm);
    const float robot_x = static_cast<float>(in.robot_x_mm);
    const float robot_y = static_cast<float>(in.robot_y_mm);
    const float gap = static_cast<float>(in.gap_mm);

    // Límites de cancha (pared-a-pared) para clampear el target.
    const float xmax = static_cast<float>(in.field_width_mm);
    const float ymax = static_cast<float>(in.field_height_mm);

    // ── Vector pelota → arco rival ───────────────────────────────────────────
    const float dx_bg = goal_x - ball_x;
    const float dy_bg = goal_y - ball_y;
    const float d_bg = std::hypot(dx_bg, dy_bg);

    // ── Target = pelota - gap * û  (lado opuesto al arco) ────────────────────
    // Degenerado: pelota ≈ arco → recta indefinida; target = pelota (clampeada).
    float tx = ball_x;
    float ty = ball_y;
    const bool goal_degenerate = (d_bg < 1.0f);
    if (!goal_degenerate) {
        const float ux = dx_bg / d_bg;
        const float uy = dy_bg / d_bg;
        tx = ball_x - gap * ux;
        ty = ball_y - gap * uy;
    }
    out.target_x_mm = clamp_to_int16(tx, 0.0f, xmax);
    out.target_y_mm = clamp_to_int16(ty, 0.0f, ymax);

    // ── Heading deseado: robot → ARCO rival (para empujar derecho al arco) ────
    // Degenerado robot==arco → mejor esfuerzo: apuntar a la pelota.
    float hdx = goal_x - robot_x;
    float hdy = goal_y - robot_y;
    if (std::hypot(hdx, hdy) < 1.0f) {
        hdx = ball_x - robot_x;
        hdy = ball_y - robot_y;
    }
    out.desired_heading_centideg =
        static_cast<int16_t>(std::lround(heading_to_centideg(hdx, hdy)));

    // ── ¿Alineado para empujar? ──────────────────────────────────────────────
    // El robot está detrás de la pelota y colineal con el arco si la dirección
    // robot→pelota coincide (mismo sentido) con la dirección pelota→arco, dentro
    // de la tolerancia. Si están alineadas, empujar la pelota la manda al arco.
    out.is_aligned_to_shoot = false;
    if (!goal_degenerate) {
        const float dx_rb = ball_x - robot_x;
        const float dy_rb = ball_y - robot_y;
        if (std::hypot(dx_rb, dy_rb) >= 1.0f) {
            // Ambos ángulos medidos en el mismo sistema (atan2(x,y) desde +Y).
            const float ang_rb = std::atan2(dx_rb, dy_rb);   // robot→pelota
            const float ang_bg = std::atan2(dx_bg, dy_bg);   // pelota→arco
            float diff_cdeg =
                wrap_centideg((ang_rb - ang_bg) * RAD_TO_CENTIDEG);
            const float tol = static_cast<float>(align_tol_centideg);
            out.is_aligned_to_shoot = (std::fabs(diff_cdeg) <= tol);
        }
    }

    return out;
}

}  // namespace iitasoccer
