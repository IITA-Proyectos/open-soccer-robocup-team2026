// pose_targeting.cpp — Implementación de los helpers de targeting absoluto.
// Ver pose_targeting.h para la API, las convenciones de ejes y los casos
// degenerados. Módulo PURO: solo <cmath>, sin Arduino.

#include "pose_targeting.h"

namespace iitasoccer {

namespace {
constexpr float PI_F = 3.14159265358979323846f;

inline float rad_to_deg(float r) { return r * (180.0f / PI_F); }

inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Normaliza un heading en centigrados a [0, 36000).
int16_t normalize_centideg(float centideg) {
    // Llevar a [0, 36000) con fmod (maneja negativos y múltiples vueltas).
    float c = std::fmod(centideg, 36000.0f);
    if (c < 0.0f) c += 36000.0f;
    // Redondeo al entero más cercano. 36000 cabe en int16? No: int16 max=32767.
    // El heading [0,36000) no entra en int16 — pero localization usa el mismo
    // tipo int16_t para heading_centideg con rango 0..36000 (lo trata como
    // unsigned lógico, ver localization.h:54). Mantenemos ese contrato: devolvemos
    // el valor envuelto a [0,36000) en un int16_t (los valores >32767 quedan
    // negativos en complemento a dos, igual que en localization). El consumidor
    // interpreta el bit-pattern como 0..36000.
    int rounded = static_cast<int>(c + 0.5f);
    if (rounded >= 36000) rounded -= 36000;  // por si el redondeo empujó a 36000
    return static_cast<int16_t>(rounded);
}
}  // namespace

float dist_mm(float ax, float ay, float bx, float by) {
    const float dx = bx - ax;
    const float dy = by - ay;
    return std::sqrt(dx * dx + dy * dy);
}

int16_t aim_heading_to_point_centideg(float robot_x, float robot_y,
                                      float target_x, float target_y) {
    const float dx = target_x - robot_x;
    const float dy = target_y - robot_y;

    // Caso degenerado: target == robot → sin dirección definida. Fallback
    // seguro: mirar al arco rival (heading 0 = +Y).
    if (dx == 0.0f && dy == 0.0f) {
        return 0;
    }

    // El frente del robot para heading θ apunta (−sin θ, cos θ) en (X, Y).
    // Queremos (−sin θ, cos θ) ∝ (dx, dy) → θ = atan2(−dx, dy).
    // (Consistente con localization.cpp::classify_wall: heading 0=+Y, +90=−X.)
    const float heading_deg = rad_to_deg(std::atan2(-dx, dy));
    return normalize_centideg(heading_deg * 100.0f);
}

FieldPoint gk_defend_point(float ball_x, float ball_y, const FieldGeometry& field) {
    const float goal_cx     = field.goal_center_x;
    const float goal_line_y = field.own_goal_line_y;
    const float half_w      = field.goal_width_mm * 0.5f;
    const float post_left   = goal_cx - half_w;   // poste -X
    const float post_right  = goal_cx + half_w;   // poste +X

    // Centro del arco — fallback para todos los casos degenerados.
    FieldPoint center{ goal_cx, goal_line_y };

    // Si la pelota está DETRÁS o SOBRE la línea de gol propia (ball_y ≤ línea),
    // no hay un tiro entrante claro hacia el arco → quedarse en el centro.
    // (También evita la división por cero del modelo de intercepción de abajo,
    // que necesita ball_y estrictamente por delante de la línea.)
    if (ball_y <= goal_line_y) {
        return center;
    }

    // Modelo de intercepción del tiro:
    // El tiro más peligroso viaja en línea recta desde la pelota hacia la boca
    // del arco. Modelamos la amenaza como el rayo pelota→centro-del-arco y
    // devolvemos dónde cruza la LÍNEA DE GOL la prolongación de la dirección
    // lateral de la pelota: el arquero se para en la x que "cubre" la pelota
    // sobre la línea. Con la pelota delante del arco (ball_y > línea), el punto
    // de cruce es simplemente la x de la pelota proyectada a la línea de gol
    // (la pelota entraría por esa x si fuera recto al arco). Se clampa a la
    // boca del arco para no salirse de los postes.
    //
    // Geométricamente: sobre el segmento pelota→centro-del-arco, la x decrece de
    // ball_x a goal_cx a medida que y va de ball_y a goal_line_y. El arquero NO
    // se para en el centro sino donde MEJOR bloquea el ángulo de tiro: cubrimos
    // la x de la pelota (clamp a postes), que es la posición canónica de un
    // arquero que "sigue la pelota" sobre su línea.
    const float keeper_x = clampf(ball_x, post_left, post_right);

    return FieldPoint{ keeper_x, goal_line_y };
}

bool in_shooting_lane(float robot_x, float robot_y,
                      float ball_x, float ball_y,
                      float goal_x, float goal_y,
                      float tol_mm) {
    // Vector del segmento pelota→arco.
    const float sx = goal_x - ball_x;
    const float sy = goal_y - ball_y;
    const float seg_len2 = sx * sx + sy * sy;

    // Caso degenerado: pelota y arco coinciden (segmento de longitud 0).
    // No hay recta definida → medir distancia punto-a-punto del robot al punto.
    if (seg_len2 == 0.0f) {
        return dist_mm(robot_x, robot_y, ball_x, ball_y) <= tol_mm;
    }

    // Proyección escalar t del robot sobre el segmento, clampeada a [0,1] para
    // que un robot "más allá" del arco o "más atrás" de la pelota mida su
    // distancia al EXTREMO, no a la recta infinita.
    float t = ((robot_x - ball_x) * sx + (robot_y - ball_y) * sy) / seg_len2;
    t = clampf(t, 0.0f, 1.0f);

    // Punto más cercano sobre el segmento y distancia perpendicular del robot.
    const float closest_x = ball_x + t * sx;
    const float closest_y = ball_y + t * sy;
    const float d = dist_mm(robot_x, robot_y, closest_x, closest_y);

    return d <= tol_mm;
}

}  // namespace iitasoccer
