#include "strategy_transitions.h"
#include "behind_ball.h"
#include <cmath>

namespace iitasoccer {

AtkTuning atk_tuning_default() {
    // Espejo de strategy.cpp commit b10c66d (constexpr líneas ~76-83).
    AtkTuning t{};
    t.attack_line_tol_deg = 30.0f;
    t.position_reached_mm = 80.0f;
    t.behind_ball_gap_mm  = 120.0f;
    t.kick_dist_mm        = 80.0f;
    t.kick_angle_deg      = 12.0f;
    t.kickoff_duration_ms = 250;
    return t;
}

GkTuning gk_tuning_default() {
    // Espejo de strategy.cpp commit b10c66d (constexpr líneas ~93-95).
    GkTuning t{};
    t.clear_trigger_mm = 250.0f;
    t.clear_release_mm = 400.0f;
    return t;
}

AtkDecision atk_decide_transition(AtkPhase current,
                                  bool prev_match_running,
                                  const AtkWorldView& w,
                                  const AtkTuning& t) {
    AtkDecision d{};
    d.kicker_fire = false;
    d.start_kickoff_timer = false;

    // --- Flanco STOP→RUN (igual que strategy.cpp:135) ---
    const bool kickoff_edge = w.match_running && !prev_match_running;

    // --- Transiciones globales prioritarias (strategy.cpp:139-148) ---
    // El orden importa: !match_running gana sobre línea, que gana sobre kickoff.
    AtkPhase phase = current;
    if (!w.match_running) {
        phase = AtkPhase::WAIT_START;
    } else if (w.imminent_exit && w.line_fresh) {
        phase = AtkPhase::LINE_AVOID;
    } else if (kickoff_edge) {
        phase = AtkPhase::KICKOFF;
        d.start_kickoff_timer = true;
    }

    // Si el global recién entró a KICKOFF, el caller usará `now` como
    // kickoff_started. Replicamos eso para evaluar el timer del case KICKOFF
    // en el MISMO tick (now - now = 0 < duration → se queda en KICKOFF).
    const uint32_t kickoff_start = d.start_kickoff_timer ? w.now_ms
                                                         : w.kickoff_started_ms;

    // --- Transición por-estado sobre el estado intermedio (strategy.cpp:150-313) ---
    switch (phase) {
        case AtkPhase::WAIT_START:
            // El case no hace nada — el flanco lo maneja el bloque global.
            break;

        case AtkPhase::KICKOFF:
            if (w.now_ms - kickoff_start >= t.kickoff_duration_ms) {
                phase = AtkPhase::SEARCH;
            }
            break;

        case AtkPhase::LINE_AVOID:
            if (!w.imminent_exit) {
                phase = AtkPhase::SEARCH;
            }
            break;

        case AtkPhase::SEARCH:
            if (w.ball_visible) {
                if (w.goal_visible) {
                    const bool aligned = ball_is_in_attack_line(
                        w.ball_x_mm, w.ball_y_mm, w.goal_angle_deg,
                        t.attack_line_tol_deg);
                    phase = aligned ? AtkPhase::APPROACH : AtkPhase::POSITION;
                } else {
                    phase = AtkPhase::APPROACH;
                }
            }
            break;

        case AtkPhase::POSITION: {
            if (!w.ball_visible) {
                phase = AtkPhase::SEARCH;
                break;
            }
            if (!w.goal_visible) {
                phase = AtkPhase::APPROACH;
                break;
            }
            const BehindBallTarget tgt = compute_behind_ball_target(
                w.ball_x_mm, w.ball_y_mm, w.goal_angle_deg, t.behind_ball_gap_mm);
            const float tdist = std::sqrt(tgt.target_x_mm * tgt.target_x_mm
                                        + tgt.target_y_mm * tgt.target_y_mm);
            const bool reached = (tdist < t.position_reached_mm);
            const bool aligned = ball_is_in_attack_line(
                w.ball_x_mm, w.ball_y_mm, w.goal_angle_deg,
                t.attack_line_tol_deg);
            if (reached && aligned) {
                phase = AtkPhase::APPROACH;
            }
            break;
        }

        case AtkPhase::APPROACH: {
            const float dist = std::sqrt(w.ball_x_mm * w.ball_x_mm
                                       + w.ball_y_mm * w.ball_y_mm);
            if (!w.ball_visible || dist < 1.0f) {
                phase = AtkPhase::SEARCH;
                break;
            }
            if (w.goal_visible) {
                // Histéresis +10° para no oscilar APPROACH↔POSITION.
                if (!ball_is_in_attack_line(w.ball_x_mm, w.ball_y_mm,
                                            w.goal_angle_deg,
                                            t.attack_line_tol_deg + 10.0f)) {
                    phase = AtkPhase::POSITION;
                    break;
                }
                // Alineados al arco — punto de empuje (sin kicker físico).
                if (is_aligned_to_push(w.ball_x_mm, w.ball_y_mm,
                                       w.goal_angle_deg,
                                       t.kick_dist_mm, t.kick_angle_deg)) {
                    d.kicker_fire = true;   // mirror de caracterización: "alineado para empujar"
                }
            }
            break;
        }
    }

    d.next_phase = phase;
    return d;
}

GkDecision gk_decide_transition(GkPhase current,
                                const GkWorldView& w,
                                const GkTuning& t) {
    GkDecision d{};

    // --- Transiciones globales prioritarias (strategy.cpp:324-328) ---
    GkPhase phase = current;
    if (!w.match_running) {
        phase = GkPhase::WAIT_START;
    } else if (w.imminent_exit && w.line_fresh) {
        phase = GkPhase::LINE_AVOID;
    }

    const float dist = std::sqrt(w.ball_x_mm * w.ball_x_mm
                               + w.ball_y_mm * w.ball_y_mm);

    // --- Transición por-estado (strategy.cpp:330-438) ---
    switch (phase) {
        case GkPhase::WAIT_START:
            if (w.match_running) {
                phase = GkPhase::PATROL;
            }
            break;

        case GkPhase::LINE_AVOID:
            if (!w.imminent_exit) {
                phase = GkPhase::PATROL;
            }
            break;

        case GkPhase::PATROL:
            if (w.ball_visible) {
                phase = GkPhase::INTERCEPT;
            }
            break;

        case GkPhase::INTERCEPT:
            // Orden EXACTO de strategy.cpp:396-400: CLEAR gana sobre PATROL.
            if (dist < t.clear_trigger_mm) {
                phase = GkPhase::CLEAR;
            } else if (!w.ball_visible) {
                phase = GkPhase::PATROL;
            }
            break;

        case GkPhase::CLEAR:
            // Orden EXACTO de strategy.cpp:410-422: !visible gana sobre release.
            if (!w.ball_visible) {
                phase = GkPhase::PATROL;
            } else if (dist > t.clear_release_mm) {
                phase = GkPhase::INTERCEPT;
            }
            break;
    }

    d.next_phase = phase;
    return d;
}

}  // namespace iitasoccer
