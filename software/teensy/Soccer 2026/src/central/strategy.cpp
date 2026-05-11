// strategy.cpp — FSM dual delantero/arquero con PIDs reales (Nivel 1 spec).
//
// Implementa el Nivel 1 de la spec FIRMWARE-PLACA-CENTRAL §18:
//   • DELANTERO: WAIT_START → SEARCH → APPROACH → SEARCH (sin behind-the-ball).
//   • ARQUERO:   WAIT_START → PATROL → INTERCEPT → CLEAR → PATROL.
//   • PID heading siempre activo en APPROACH e INTERCEPT.
//   • PID lateral activo SOLO en modo arquero + match_running.
//   • Modo EMERGENCY_LINE bypassa la FSM (se maneja en main_central.cpp directo,
//     antes de llamar a strategy_tick).
//   • Si LINE_FLAG_LIFTED set, world_model_imminent_exit() retorna false
//     (filtrado en line_ring DOWN antes de enviar), así no entramos a LINE_AVOID
//     con datos basura.
//
// La cinemática inversa NO vive acá — los outputs (vx, vy, omega) viajan al
// motors_zircon que aplica la kinematics. Eso permite tunear strategy y kinematics
// por separado.

#include "strategy.h"
#include "world_model.h"
#include "pids.h"

#include <Arduino.h>
#include <cmath>

namespace iitasoccer {

namespace {

// === Configuración táctica ===

RobotRole   g_role = RobotRole::ATTACKER;
AttackColor g_attack_color = AttackColor::MAGENTA;

const char* g_state_name = "INIT";

// Estados internos enumerados (no expuestos en el header — debug por nombre).
enum class AtkState : uint8_t {
    WAIT_START, SEARCH, APPROACH, LINE_AVOID
};
enum class GkState : uint8_t {
    WAIT_START, PATROL, INTERCEPT, LINE_AVOID
};

AtkState g_atk_state = AtkState::WAIT_START;
GkState  g_gk_state  = GkState::WAIT_START;

// === PIDs ===
HeadingPID g_heading_pid;
LateralPID g_lateral_pid_gk;

// === Tuning constants ===

// Delantero
constexpr float ATK_SEARCH_VY_MM_S       = 200.0f;
constexpr float ATK_SEARCH_OMEGA_DEG_S   = 60.0f;
constexpr float ATK_APPROACH_MAX_SPEED   = 600.0f;
constexpr float ATK_APPROACH_MIN_SPEED   = 200.0f;
constexpr float ATK_APPROACH_CLOSE_MM    = 50.0f;
constexpr float ATK_APPROACH_FAR_MM      = 500.0f;
constexpr float ATK_LINE_RETREAT_SPEED   = 400.0f;

// Arquero
constexpr float GK_PATROL_OSCILLATE_PERIOD_MS = 2000.0f;
constexpr float GK_PATROL_SPEED_MM_S          = 150.0f;
constexpr float GK_INTERCEPT_KP_VS_BALL_X     = 4.0f;
constexpr float GK_LATERAL_SETPOINT_DEPTH     = 1.0f;
constexpr float GK_LINE_RETREAT_SPEED         = 250.0f;

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

    // Transiciones globales prioritarias.
    if (!world_model_match_running()) {
        transition_atk(AtkState::WAIT_START);
    } else if (world_model_imminent_exit() && line_data_fresh()) {
        // imminent_exit en DOWN ya respeta lifted (no se setea si lifted=true).
        transition_atk(AtkState::LINE_AVOID);
    }

    switch (g_atk_state) {
        case AtkState::WAIT_START: {
            g_state_name = "ATK_WAIT_START";
            if (world_model_match_running()) {
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

            float vx_lateral_pid = 0.0f;
            if (line_data_fresh()) {
                const float depth = static_cast<float>(world_model_get_line_depth());
                lateral_pid_set_target(g_lateral_pid_gk, GK_LATERAL_SETPOINT_DEPTH);
                vx_lateral_pid = lateral_pid_tick(g_lateral_pid_gk, depth, now_ms);
            }

            const float vx_intercept = bx * GK_INTERCEPT_KP_VS_BALL_X;
            cmd.vx_mm_s = static_cast<int16_t>(vx_intercept + vx_lateral_pid * 0.3f);

            if (!world_model_ball_visible()) {
                transition_gk(GkState::PATROL);
            }
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
