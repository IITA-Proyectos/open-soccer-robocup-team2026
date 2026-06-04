#include "clear_aim.h"
#include <cmath>

namespace iitasoccer {

namespace {
constexpr float PI_F = 3.14159265358979323846f;

inline float deg_to_rad(float d) { return d * (PI_F / 180.0f); }

// Normaliza a (-180, +180].
float wrap_180(float a_deg) {
    while (a_deg > 180.0f)  a_deg -= 360.0f;
    while (a_deg <= -180.0f) a_deg += 360.0f;
    return a_deg;
}

// Redondeo a int para centideg (evita truncado hacia cero asimétrico).
inline int16_t to_centideg(float deg) {
    float cd = deg * 100.0f;
    return static_cast<int16_t>(cd >= 0.0f ? cd + 0.5f : cd - 0.5f);
}
}  // namespace

ClearAimOut clear_aim(const ClearAimIn& in) {
    // Fallback EXACTO: sin arco propio visible, este módulo NO opina. El caller
    // usa su empuje derecho actual (byte-idéntico al de hoy).
    if (!in.goal_own_visible) {
        return ClearAimOut{0, false};
    }

    // Ángulo robot→arco propio (atan2(x,y): + = derecha del robot).
    const float goal_deg = wrap_180(in.goal_own_angle_centideg / 100.0f);
    const float goal_rad = deg_to_rad(goal_deg);

    // Las bandas laterales corren PERPENDICULARES al eje robot→arco. Hay DOS
    // perpendiculares: goal_deg+90 y goal_deg−90. Queremos el que apunta a la
    // banda MÁS LEJANA del centro del arco = el del lado lateral OPUESTO al arco.
    //
    // El "lado lateral" del arco lo da su componente x = sin(goal_deg) (marco
    // robot, +x = derecha). La componente x de cada perpendicular es:
    //   x(goal+90) = sin(goal+90) = +cos(goal)
    //   x(goal−90) = sin(goal−90) = −cos(goal)
    // Elegimos el perpendicular cuya x tenga signo OPUESTO al del arco (empujar
    // lateralmente hacia el lado contrario al arco). Esto vale para CUALQUIER
    // orientación del arco — al frente, al costado, o DETRÁS del robot (caso
    // normal del arquero mirando al campo: arco atrás-izquierda ≈ −170° → empuja
    // atrás-derecha; arco al costado-izquierda = −90° → empuja al frente/atrás
    // por el lado correcto).
    const float goal_lat = std::sin(goal_rad);   // lado lateral del arco.
    const float cos_goal = std::cos(goal_rad);   // = x del perpendicular +90.

    float perp_sign;                              // +1 → usar goal+90, −1 → goal−90.
    const float LAT_DEADZONE = 0.01745f;          // ~|sin(1°)|: arco casi al eje.
    if (std::fabs(goal_lat) > LAT_DEADZONE) {
        // Queremos x(perp) con signo opuesto a goal_lat.
        //   x(goal+90) = +cos_goal ; x(goal−90) = −cos_goal.
        // Elegir +90 si +cos_goal tiene signo opuesto a goal_lat, si no −90.
        const bool plus90_is_opposite =
            (cos_goal > 0.0f && goal_lat < 0.0f) ||
            (cos_goal < 0.0f && goal_lat > 0.0f);
        perp_sign = plus90_is_opposite ? +1.0f : -1.0f;
    } else {
        // Arco casi alineado con el eje frente/atrás (lado lateral ambiguo). Los
        // dos perpendiculares son ≈±90° (derecha/izquierda pura). Desempate por
        // la posición lateral de la pelota: empujar hacia el lado donde ya está
        // (su banda más cercana = la más lejana del centro del arco). Acá
        // cos(goal)≈±1, así que goal+perp_sign*90 cae en ±90° (lateral puro).
        float want_right;             // +1 = queremos push a la derecha (+90 robot).
        if (in.ball_x_mm > 0)       want_right = +1.0f;  // pelota derecha.
        else if (in.ball_x_mm < 0)  want_right = -1.0f;  // pelota izquierda.
        else                        want_right = +1.0f;  // centrada → derecha.
        // x(goal+90)=cos_goal. Si su signo coincide con el lado querido, usar +90.
        const bool plus90_matches =
            (cos_goal > 0.0f && want_right > 0.0f) ||
            (cos_goal < 0.0f && want_right < 0.0f);
        perp_sign = plus90_matches ? +1.0f : -1.0f;
    }

    const float push_deg = wrap_180(goal_deg + perp_sign * 90.0f);

    ClearAimOut out{};
    out.push_heading_centideg = to_centideg(push_deg);
    out.valid = true;
    return out;
}

}  // namespace iitasoccer
