// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
// strategy.cpp — FSM dual delantero/arquero con PIDs reales (Niveles 1 + 2).
//
// Niveles spec FIRMWARE-PLACA-CENTRAL §18:
//
// Nivel 1 (Incheon mínimo):
//   • DELANTERO: WAIT_START → SEARCH → APPROACH → SEARCH (sin behind-the-ball).
//   • ARQUERO:   WAIT_START → PATROL → INTERCEPT → PATROL.
//
// Nivel 2 (este archivo agrega):
//   • DELANTERO: KICKOFF (set play inicial) + POSITION (behind-the-ball) +
//                disparo de kicker cuando alineado a arco rival.
//   • ARQUERO:   CLEAR (despeje cuando pelota llega cerca).
//
// Modo EMERGENCY_LINE bypassa la FSM (se maneja en main_central.cpp directo,
// antes de llamar a strategy_tick).
//
// Si LINE_FLAG_LIFTED set, world_model_imminent_exit() retorna false (filtrado
// en line_ring DOWN antes de enviar), así no entramos a LINE_AVOID con datos
// basura.
//
// La cinemática inversa NO vive acá — los outputs (vx, vy, omega) viajan al
// motors_zircon que aplica la kinematics. Eso permite tunear strategy y
// kinematics por separado.

#include "strategy.h"
#include "world_model.h"
#include "pids.h"
#include "behind_ball.h"

#include <Arduino.h>
#include <cmath>

namespace iitasoccer {

namespace {

// === Configuración táctica ===

RobotRole   g_role = RobotRole::ATTACKER;
AttackColor g_attack_color = AttackColor::MAGENTA;

const char* g_state_name = "INIT";

// Estados internos enumerados (no expuestos en el header — debug por nombre).
//
// Nivel 1 (Incheon mínimo):
//   ATK: WAIT_START / SEARCH / APPROACH / LINE_AVOID
//   GK:  WAIT_START / PATROL / INTERCEPT / LINE_AVOID
//
// Nivel 2 (este batch) agrega:
//   ATK: KICKOFF (set play inicial), POSITION (behind-the-ball orbit)
//   GK:  CLEAR (despeje cuando la pelota está cerca)
enum class AtkState : uint8_t {
    WAIT_START, KICKOFF, SEARCH, POSITION, APPROACH, LINE_AVOID
};
enum class GkState : uint8_t {
    WAIT_START, PATROL, INTERCEPT, CLEAR, LINE_AVOID
};

AtkState g_atk_state = AtkState::WAIT_START;
GkState  g_gk_state  = GkState::WAIT_START;

// Estado auxiliar del kickoff (timer) — válido solo cuando g_atk_state == KICKOFF.
uint32_t g_kickoff_started_ms = 0;
bool     g_match_was_running = false;   // detecta flanco "STOP → RUN" para KICKOFF

// === PIDs ===
HeadingPID g_heading_pid;
LateralPID g_lateral_pid_gk;

// === Tuning constants ===

// Delantero — Nivel 1
constexpr float ATK_SEARCH_VY_MM_S       = 200.0f;
constexpr float ATK_SEARCH_OMEGA_DEG_S   = 60.0f;
constexpr float ATK_APPROACH_MAX_SPEED   = 600.0f;
constexpr float ATK_APPROACH_MIN_SPEED   = 200.0f;
constexpr float ATK_APPROACH_CLOSE_MM    = 50.0f;
constexpr float ATK_APPROACH_FAR_MM      = 500.0f;
constexpr float ATK_LINE_RETREAT_SPEED   = 400.0f;

// Delantero — Nivel 2
constexpr float ATK_BEHIND_BALL_GAP_MM         = 120.0f;   // separación robot–pelota cuando POSITION
constexpr float ATK_ATTACK_LINE_TOL_DEG        = 30.0f;    // si ángulo ball ↔ goal < este → ya alineado
constexpr float ATK_POSITION_REACHED_MM        = 80.0f;    // distancia target POSITION para pasar a APPROACH
constexpr float ATK_KICK_DIST_MM               = 80.0f;    // cuando pelota más cerca que esto + alineado → kick
constexpr float ATK_KICK_ANGLE_DEG             = 12.0f;    // tolerancia angular para disparar
constexpr float ATK_POSITION_MAX_SPEED         = 500.0f;
constexpr float ATK_KICKOFF_SPEED_MM_S         = 500.0f;
constexpr uint32_t ATK_KICKOFF_DURATION_MS     = 250;      // boost inicial al frente al arrancar match

// Arquero — Nivel 1
constexpr float GK_PATROL_OSCILLATE_PERIOD_MS = 2000.0f;
constexpr float GK_PATROL_SPEED_MM_S          = 150.0f;
constexpr float GK_INTERCEPT_KP_VS_BALL_X     = 4.0f;
constexpr float GK_LATERAL_SETPOINT_DEPTH     = 1.0f;
constexpr float GK_LINE_RETREAT_SPEED         = 250.0f;

// Arquero — Nivel 2
constexpr float GK_CLEAR_TRIGGER_MM           = 250.0f;    // pelota más cerca que esto → CLEAR
constexpr float GK_CLEAR_RELEASE_MM           = 400.0f;    // histéresis: vuelve a PATROL al alejarse
constexpr float GK_CLEAR_SPEED_MM_S           = 500.0f;

// === Helpers ===

inline bool line_data_fresh() {
    return world_model_line_is_fresh();
}

void transition_atk(AtkState new_state) {
    if (new_state == g_atk_state) return;
    g_atk_state = new_state;
    // Reset PID en cada transición — evita arrastrar windup viejo.
    heading_pid_reset(g_heading_pid);
}

void transition_gk(GkState new_state) {
    if (new_state == g_gk_state) return;
    g_gk_state = new_state;
    heading_pid_reset(g_heading_pid);
    if (new_state != GkState::PATROL && new_state != GkState::INTERCEPT) {
        lateral_pid_reset(g_lateral_pid_gk);
    }
}

// === FSM Delantero ===

MotorCommand attacker_tick() {
    MotorCommand cmd{};
    const uint32_t now_ms = millis();
    const float heading = world_model_get_my_heading_deg();

    // Detección de flanco STOP→RUN para entrar a KICKOFF.
    const bool match_running = world_model_match_running();
    const bool kickoff_edge = match_running && !g_match_was_running;
    g_match_was_running = match_running;

    // Transiciones globales prioritarias.
    if (!match_running) {
        transition_atk(AtkState::WAIT_START);
    } else if (world_model_imminent_exit() && line_data_fresh()) {
        // imminent_exit en DOWN ya respeta lifted (no se setea si lifted=true).
        transition_atk(AtkState::LINE_AVOID);
    } else if (kickoff_edge) {
        // Flanco STOP→RUN: arrancar el set play KICKOFF.
        transition_atk(AtkState::KICKOFF);
        g_kickoff_started_ms = now_ms;
    }

    switch (g_atk_state) {
        case AtkState::WAIT_START: {
            g_state_name = "ATK_WAIT_START";
            // El flanco STOP→RUN lo maneja la sección global de arriba.
            return cmd;
        }

        case AtkState::KICKOFF: {
            g_state_name = "ATK_KICKOFF";
            // Set play: boost recto al frente con heading hacia 0° absoluto
            // durante ATK_KICKOFF_DURATION_MS. Después transitions a SEARCH.
            const KickoffVelocity kv = compute_kickoff_velocity(ATK_KICKOFF_SPEED_MM_S);
            cmd.vx_mm_s = static_cast<int16_t>(kv.vx_mm_s);
            cmd.vy_mm_s = static_cast<int16_t>(kv.vy_mm_s);
            // Mantener heading actual — no queremos rotar durante el boost.
            heading_pid_set_target(g_heading_pid, heading);
            cmd.omega_centideg_s = 0;

            if (now_ms - g_kickoff_started_ms >= ATK_KICKOFF_DURATION_MS) {
                transition_atk(AtkState::SEARCH);
            }
            return cmd;
        }

        case AtkState::LINE_AVOID: {
            g_state_name = "ATK_LINE_AVOID";
            // Retroceder en dirección opuesta a la línea detectada.
            const float line_angle = world_model_get_line_angle_deg();
            const float retreat = line_angle + 180.0f;
            const float rad = retreat * (M_PI / 180.0f);
            cmd.vx_mm_s = static_cast<int16_t>(std::sin(rad) * ATK_LINE_RETREAT_SPEED);
            cmd.vy_mm_s = static_cast<int16_t>(std::cos(rad) * ATK_LINE_RETREAT_SPEED);
            if (!world_model_imminent_exit()) {
                transition_atk(AtkState::SEARCH);
            }
            return cmd;
        }

        case AtkState::SEARCH: {
            g_state_name = "ATK_SEARCH";
            // Recorrer cancha con avance lento + rotación.
            cmd.vy_mm_s = static_cast<int16_t>(ATK_SEARCH_VY_MM_S);
            cmd.omega_centideg_s = static_cast<int16_t>(ATK_SEARCH_OMEGA_DEG_S * 100.0f);
            if (world_model_ball_visible()) {
                // Si vemos arco rival y la pelota NO está alineada, primero
                // POSITION (orbit). Si está alineada o no vemos arco, APPROACH
                // directo (fallback).
                const float bx = world_model_get_ball_x_mm();
                const float by = world_model_get_ball_y_mm();
                if (world_model_goal_opp_visible()) {
                    const float goal_angle = world_model_get_goal_opp_angle_deg();
                    const bool aligned = ball_is_in_attack_line(bx, by, goal_angle,
                                                                ATK_ATTACK_LINE_TOL_DEG);
                    transition_atk(aligned ? AtkState::APPROACH : AtkState::POSITION);
                } else {
                    transition_atk(AtkState::APPROACH);
                }
            }
            return cmd;
        }

        case AtkState::POSITION: {
            g_state_name = "ATK_POSITION";
            // Behind-the-ball: ir al target que queda DETRÁS de la pelota
            // mirando al arco. Cuando llego al target Y estoy mirando al arco,
            // transitions a APPROACH para empujar/patear.
            if (!world_model_ball_visible()) {
                transition_atk(AtkState::SEARCH);
                return cmd;
            }
            if (!world_model_goal_opp_visible()) {
                // Perdí el arco: degradar a APPROACH directo (Nivel 1).
                transition_atk(AtkState::APPROACH);
                return cmd;
            }

            const float bx = world_model_get_ball_x_mm();
            const float by = world_model_get_ball_y_mm();
            const float goal_angle = world_model_get_goal_opp_angle_deg();

            const BehindBallTarget tgt = compute_behind_ball_target(
                bx, by, goal_angle, ATK_BEHIND_BALL_GAP_MM);

            // Heading: siempre mirar al arco rival.
            heading_pid_set_target(g_heading_pid, heading + goal_angle);
            const float omega = heading_pid_tick(g_heading_pid, heading, now_ms);

            // Vector al target relativo al robot. Velocidad proporcional con
            // perfil suave (mismo approach_velocity reusado).
            const float tx = tgt.target_x_mm;
            const float ty = tgt.target_y_mm;
            const float tdist = std::sqrt(tx * tx + ty * ty);
            const float speed = approach_velocity(tdist,
                                                  ATK_POSITION_REACHED_MM,
                                                  ATK_APPROACH_FAR_MM,
                                                  ATK_POSITION_MAX_SPEED,
                                                  ATK_APPROACH_MIN_SPEED);
            if (tdist > 1.0f) {
                cmd.vx_mm_s = static_cast<int16_t>(tx / tdist * speed);
                cmd.vy_mm_s = static_cast<int16_t>(ty / tdist * speed);
            }
            cmd.omega_centideg_s = static_cast<int16_t>(omega * 100.0f);

            // Transición a APPROACH: llegué al target Y estoy alineado a la
            // línea pelota–arco.
            const bool reached = (tdist < ATK_POSITION_REACHED_MM);
            const bool aligned = ball_is_in_attack_line(bx, by, goal_angle,
                                                         ATK_ATTACK_LINE_TOL_DEG);
            if (reached && aligned) {
                transition_atk(AtkState::APPROACH);
            }
            return cmd;
        }

        case AtkState::APPROACH: {
            g_state_name = "ATK_APPROACH";
            const float bx = world_model_get_ball_x_mm();
            const float by = world_model_get_ball_y_mm();
            const float dist = std::sqrt(bx * bx + by * by);

            if (!world_model_ball_visible() || dist < 1.0f) {
                transition_atk(AtkState::SEARCH);
                return cmd;
            }

            // Si la pelota dejó de estar alineada con el arco (la perdimos al
            // costado), volver a POSITION para rodearla otra vez.
            if (world_model_goal_opp_visible()) {
                const float goal_angle = world_model_get_goal_opp_angle_deg();
                if (!ball_is_in_attack_line(bx, by, goal_angle,
                                             ATK_ATTACK_LINE_TOL_DEG + 10.0f)) {
                    // Histéresis +10° para no oscilar entre APPROACH↔POSITION.
                    transition_atk(AtkState::POSITION);
                    return cmd;
                }
                // Estamos alineados — ¿ya alcanza para patear?
                if (is_aligned_to_shoot(bx, by, goal_angle,
                                         ATK_KICK_DIST_MM, ATK_KICK_ANGLE_DEG)) {
                    cmd.kicker_fire = 1;   // motors_zircon hace el pulso + cooldown
                }
            }

            // Heading target: orientar el frente hacia la pelota.
            // ball_x relativo al robot: 0 = frente. Convertir a absoluto.
            const float ball_angle_rel = std::atan2(bx, by) * (180.0f / M_PI);
            const float ball_angle_abs = heading + ball_angle_rel;
            heading_pid_set_target(g_heading_pid, ball_angle_abs);
            const float omega = heading_pid_tick(g_heading_pid, heading, now_ms);

            // Velocidad de avance con perfil suave.
            const float speed = approach_velocity(dist,
                                                  ATK_APPROACH_CLOSE_MM,
                                                  ATK_APPROACH_FAR_MM,
                                                  ATK_APPROACH_MAX_SPEED,
                                                  ATK_APPROACH_MIN_SPEED);

            if (dist > 1.0f) {
                cmd.vx_mm_s = static_cast<int16_t>(bx / dist * speed);
                cmd.vy_mm_s = static_cast<int16_t>(by / dist * speed);
            }
            cmd.omega_centideg_s = static_cast<int16_t>(omega * 100.0f);
            return cmd;
        }
    }

    return cmd;
}

// === FSM Arquero ===

MotorCommand goalkeeper_tick() {
    MotorCommand cmd{};
    const uint32_t now_ms = millis();

    if (!world_model_match_running()) {
        transition_gk(GkState::WAIT_START);
    } else if (world_model_imminent_exit() && line_data_fresh()) {
        transition_gk(GkState::LINE_AVOID);
    }

    switch (g_gk_state) {
        case GkState::WAIT_START: {
            g_state_name = "GK_WAIT_START";
            if (world_model_match_running()) {
                transition_gk(GkState::PATROL);
            }
            return cmd;
        }

        case GkState::LINE_AVOID: {
            g_state_name = "GK_LINE_AVOID";
            const float line_angle = world_model_get_line_angle_deg();
            const float retreat = line_angle + 180.0f;
            const float rad = retreat * (M_PI / 180.0f);
            cmd.vx_mm_s = static_cast<int16_t>(std::sin(rad) * GK_LINE_RETREAT_SPEED);
            cmd.vy_mm_s = static_cast<int16_t>(std::cos(rad) * GK_LINE_RETREAT_SPEED);
            if (!world_model_imminent_exit()) {
                transition_gk(GkState::PATROL);
            }
            return cmd;
        }

        case GkState::PATROL: {
            g_state_name = "GK_PATROL";
            // PID lateral mantiene el robot pisando línea de fondo.
            float vx_lateral_pid = 0.0f;
            if (line_data_fresh()) {
                const float depth = static_cast<float>(world_model_get_line_depth());
                lateral_pid_set_target(g_lateral_pid_gk, GK_LATERAL_SETPOINT_DEPTH);
                vx_lateral_pid = lateral_pid_tick(g_lateral_pid_gk, depth, now_ms);
            }

            // Oscilación lateral encima del PID — patrulla el área chica.
            static int direction = 1;
            static uint32_t last_change = 0;
            if (now_ms - last_change > static_cast<uint32_t>(GK_PATROL_OSCILLATE_PERIOD_MS)) {
                direction = -direction;
                last_change = now_ms;
            }
            const float vx_patrol = direction * GK_PATROL_SPEED_MM_S;
            cmd.vx_mm_s = static_cast<int16_t>(vx_patrol + vx_lateral_pid * 0.5f);

            if (world_model_ball_visible()) {
                transition_gk(GkState::INTERCEPT);
            }
            return cmd;
        }

        case GkState::INTERCEPT: {
            g_state_name = "GK_INTERCEPT";
            const float bx = world_model_get_ball_x_mm();
            const float by = world_model_get_ball_y_mm();
            const float dist = std::sqrt(bx * bx + by * by);

            float vx_lateral_pid = 0.0f;
            if (line_data_fresh()) {
                const float depth = static_cast<float>(world_model_get_line_depth());
                lateral_pid_set_target(g_lateral_pid_gk, GK_LATERAL_SETPOINT_DEPTH);
                vx_lateral_pid = lateral_pid_tick(g_lateral_pid_gk, depth, now_ms);
            }

            const float vx_intercept = bx * GK_INTERCEPT_KP_VS_BALL_X;
            cmd.vx_mm_s = static_cast<int16_t>(vx_intercept + vx_lateral_pid * 0.3f);

            // Transición a CLEAR: la pelota llegó cerca → salir a despejar
            // en lugar de seguir solo el eje X.
            if (dist < GK_CLEAR_TRIGGER_MM) {
                transition_gk(GkState::CLEAR);
            } else if (!world_model_ball_visible()) {
                transition_gk(GkState::PATROL);
            }
            return cmd;
        }

        case GkState::CLEAR: {
            g_state_name = "GK_CLEAR";
            // Despeje: ir DERECHO a la pelota a velocidad alta (no PID lateral
            // — ya no estamos defendiendo el arco, estamos atacando la pelota).
            // El arquero NO tiene kicker en ROBOT1 → empuja la pelota fuera
            // por inercia mecánica.
            if (!world_model_ball_visible()) {
                transition_gk(GkState::PATROL);
                return cmd;
            }
            const float bx = world_model_get_ball_x_mm();
            const float by = world_model_get_ball_y_mm();
            const float dist = std::sqrt(bx * bx + by * by);

            if (dist > GK_CLEAR_RELEASE_MM) {
                // Histéresis: si la pelota se alejó, volver a defender.
                transition_gk(GkState::INTERCEPT);
                return cmd;
            }

            if (dist > 1.0f) {
                cmd.vx_mm_s = static_cast<int16_t>(bx / dist * GK_CLEAR_SPEED_MM_S);
                cmd.vy_mm_s = static_cast<int16_t>(by / dist * GK_CLEAR_SPEED_MM_S);
            }
            // Heading hacia la pelota para que el despeje sea con el frente.
            const float ball_angle_rel = std::atan2(bx, by) * (180.0f / M_PI);
            heading_pid_set_target(g_heading_pid,
                                   world_model_get_my_heading_deg() + ball_angle_rel);
            const float omega = heading_pid_tick(g_heading_pid,
                                                  world_model_get_my_heading_deg(),
                                                  now_ms);
            cmd.omega_centideg_s = static_cast<int16_t>(omega * 100.0f);
            return cmd;
        }
    }

    return cmd;
}

}  // namespace

void strategy_init() {
    g_state_name = "INIT";
    g_atk_state = AtkState::WAIT_START;
    g_gk_state = GkState::WAIT_START;
    g_match_was_running = false;
    g_kickoff_started_ms = 0;
    heading_pid_reset(g_heading_pid);
    lateral_pid_reset(g_lateral_pid_gk);
}

MotorCommand strategy_tick() {
    return (g_role == RobotRole::ATTACKER) ? attacker_tick() : goalkeeper_tick();
}

void        strategy_set_role(RobotRole role)         { g_role = role; }
RobotRole   strategy_get_role()                       { return g_role; }
void        strategy_set_attack_color(AttackColor c)  { g_attack_color = c; }
AttackColor strategy_get_attack_color()               { return g_attack_color; }
const char* strategy_get_state_name()                 { return g_state_name; }

}  // namespace iitasoccer
