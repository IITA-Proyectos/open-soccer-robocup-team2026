// goal_polarity.cpp — implementación pura de goal_polarity.h.

#include "goal_polarity.h"

namespace iitasoccer {

namespace {
// |ángulo| < 90° = hemisferio DELANTERO (el arco está al frente del robot).
bool in_front(float angle_deg) {
    float a = angle_deg < 0.0f ? -angle_deg : angle_deg;
    return a < 90.0f;
}
}  // namespace

GoalPolarity goal_polarity_infer(bool yellow_visible, float yellow_angle_deg,
                                 bool blue_visible,   float blue_angle_deg) {
    const bool y_front = yellow_visible && in_front(yellow_angle_deg);
    const bool b_front = blue_visible   && in_front(blue_angle_deg);
    const bool y_back  = yellow_visible && !in_front(yellow_angle_deg);
    const bool b_back  = blue_visible   && !in_front(blue_angle_deg);

    // Conflicto: los DOS parecen estar al frente → no se puede decidir.
    if (y_front && b_front) return GoalPolarity::UNKNOWN;

    // El arco al FRENTE es el rival.
    if (y_front) return GoalPolarity::YELLOW_IS_OPP;
    if (b_front) return GoalPolarity::BLUE_IS_OPP;

    // Ninguno al frente: usar el de ATRÁS (= propio) para deducir el rival.
    if (y_back) return GoalPolarity::BLUE_IS_OPP;    // amarillo atrás=propio → azul rival
    if (b_back) return GoalPolarity::YELLOW_IS_OPP;  // azul atrás=propio → amarillo rival

    return GoalPolarity::UNKNOWN;  // no se ve ningún arco
}

void goal_polarity_latch_init(GoalPolarityLatch& l) {
    l.value = GoalPolarity::UNKNOWN;
    l.candidate = GoalPolarity::UNKNOWN;
    l.count = 0;
}

GoalPolarity goal_polarity_latch_update(GoalPolarityLatch& l, GoalPolarity inferred) {
    if (l.value != GoalPolarity::UNKNOWN) {
        return l.value;  // ya fijada: estable para todo el partido
    }
    if (inferred == GoalPolarity::UNKNOWN) {
        l.candidate = GoalPolarity::UNKNOWN;
        l.count = 0;
        return l.value;
    }
    if (inferred == l.candidate) {
        if (l.count < 0xFFFFu) ++l.count;
    } else {
        l.candidate = inferred;
        l.count = 1;
    }
    if (l.count >= GOAL_POLARITY_CONFIRM) {
        l.value = l.candidate;  // FIJA (no vuelve a cambiar)
    }
    return l.value;
}

GoalPolarity goal_polarity_effective(const GoalPolarityLatch& l, GoalPolarity fallback) {
    return (l.value != GoalPolarity::UNKNOWN) ? l.value : fallback;
}

}  // namespace iitasoccer
