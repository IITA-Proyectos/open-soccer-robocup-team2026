// mix_edge.cpp — Implementación PURA del rodeo estilo Edge. Ver mix_edge.h.
//
// Sin Arduino: usa <cmath> nomás → compila y se testea en host (mix_edge_test.cpp).

#include "mix_edge.h"
#include <cmath>

namespace iitasoccer {
namespace mix {

// ============================================================
// LA CURVA: |ángulo de pelota| → ángulo de avance amplificado.
//
// Tres tramos lineales, ENCADENADOS para que sea CONTINUA (el valor al final de un tramo
// es el inicio del siguiente — no hay saltos como en el port a tramos crudo de Edge):
//
//   go(0)        = 0
//   go(b1)       = k_near * b1                              (fin tramo NEAR)
//   go(b2)       = go(b1) + k_side * (b2 - b1)              (fin tramo SIDE)
//   go(a>b2)     = go(b2) + k_wide * (a - b2)               (tramo WIDE)
//
// Con los defaults (k_near 1.2, b1 20, k_side 2.0, b2 75, k_wide 1.0):
//   pelota a   0° → avanzo a   0°  (derecho a ella)
//   pelota a  20° → avanzo a  24°  (apenas amplificado)
//   pelota a  60° → avanzo a 104°  (¡me voy MÁS al costado que la pelota → la rodeo!)
//   pelota a  90° → avanzo a 149°  (barrido ancho para meterme detrás)
// El signo (pelota a izquierda/derecha) se restaura al final. Tope go_max para no apuntar
// 100% hacia atrás (quedaría inestable).
// ============================================================
float mix_edge_wrap_angle(float ball_angle_deg, const EdgeParams& p) {
    const float a = std::fabs(ball_angle_deg);

    float go;
    if (a < p.b1_deg) {
        // Zona CERCANA: casi derecho a la pelota.
        go = p.k_near * a;
    } else if (a < p.b2_deg) {
        // Zona LATERAL: rodeo fuerte (continúa desde el fin de la zona cercana).
        const float go_b1 = p.k_near * p.b1_deg;
        go = go_b1 + p.k_side * (a - p.b1_deg);
    } else {
        // Zona ANCHA: barrido para meterse detrás (continúa desde el fin de la lateral).
        const float go_b1 = p.k_near * p.b1_deg;
        const float go_b2 = go_b1 + p.k_side * (p.b2_deg - p.b1_deg);
        go = go_b2 + p.k_wide * (a - p.b2_deg);
    }

    if (go > p.go_max_deg) go = p.go_max_deg;

    // Restaurar el signo: pelota a la izquierda (ángulo<0) → avanzar a la izquierda.
    return (ball_angle_deg < 0.0f) ? -go : go;
}

// ============================================================
// El delantero de un tick.
// ============================================================
EdgeOut mix_edge_attack(const EdgeIn& in, const EdgeParams& p) {
    EdgeOut out{};

    // Dirección de avance = la curva de rodeo.
    out.go_ang_deg = mix_edge_wrap_angle(in.ball_angle_deg, p);

    // ¿Comprometerse al empuje (gol por inercia)? Pelota cerca + bien al frente.
    bool push = in.ball_visible &&
                (in.ball_dist_cm < p.push_dist_cm) &&
                (std::fabs(in.ball_angle_deg) < p.push_align_deg);

    // Si SE VE el arco rival, exigir además que esté alineado al frente (no empujar para
    // cualquier lado). Si NO se ve el arco, empujar igual (se confía en el rumbo/heading,
    // como hacía Edge cuando no veía el arco con la cámara). Esto EVITA el cuelgue de
    // "nunca empuja porque justo el arco se salió del FOV", pero BLOQUEA empujar hacia un
    // arco visible pero desalineado.
    if (push && in.goal_visible) {
        push = (std::fabs(in.goal_angle_deg) < p.push_goal_deg);
    }
    out.push_ready = push;

    return out;
}

}  // namespace mix
}  // namespace iitasoccer
