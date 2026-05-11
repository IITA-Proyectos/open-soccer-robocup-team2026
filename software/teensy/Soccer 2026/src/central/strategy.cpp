// strategy.cpp — STUB inicial de la FSM dual.
//
// Versión actual: FSM mínima que devuelve STOP cuando no hay match running.
// Cuando el match está en curso:
//   • ATTACKER: avanza al frente lento mientras no vea pelota; cuando ve pelota
//     gira para apuntarle (placeholder — falta toda la lógica behind-the-ball).
//   • GOALKEEPER: queda quieto en el centro del arco (placeholder).
//
// Esto es suficiente para verificar end-to-end de comm + motores en cancha
// con un scrimmage. La lógica real de behind-the-ball, orbit, etc. se agrega
// en Hito 6, una vez que el firmware base esté validado.

#include "strategy.h"
#include "world_model.h"

#include <Arduino.h>
#include <cmath>

namespace iitasoccer {

namespace {

RobotRole   g_role = RobotRole::ATTACKER;
AttackColor g_attack_color = AttackColor::MAGENTA;

const char* g_state_name = "INIT";

constexpr int16_t SLOW_FORWARD_MM_S = 200;     // velocidad de búsqueda lenta
constexpr int16_t MEDIUM_FORWARD_MM_S = 400;

MotorCommand attacker_tick() {
    MotorCommand cmd{};

    if (!world_model_match_running()) {
        g_state_name = "ATK_WAIT_START";
        return cmd;  // todo en 0
    }

    if (world_model_imminent_exit()) {
        g_state_name = "ATK_LINE_AVOID";
        const float line_angle = world_model_get_line_angle_deg();
        // Retroceder en dirección opuesta a la línea (la línea está hacia adelante
        // del robot, hay que ir hacia atrás).
        const float retreat_angle = line_angle + 180.0f;
        const float rad = retreat_angle * (M_PI / 180.0f);
        cmd.vx_mm_s = static_cast<int16_t>(std::sin(rad) * MEDIUM_FORWARD_MM_S);
        cmd.vy_mm_s = static_cast<int16_t>(std::cos(rad) * MEDIUM_FORWARD_MM_S);
        return cmd;
    }

    if (world_model_ball_visible()) {
        g_state_name = "ATK_APPROACH";
        // Apuntar a la pelota: usar el ángulo de la pelota relativo al frente.
        // Por ahora, ball_x es relativo al robot. Avanzamos en su dirección.
        const float bx = world_model_get_ball_x_mm();
        const float by = world_model_get_ball_y_mm();
        const float mag = std::sqrt(bx * bx + by * by);
        if (mag > 1.0f) {
            cmd.vx_mm_s = static_cast<int16_t>(bx / mag * MEDIUM_FORWARD_MM_S);
            cmd.vy_mm_s = static_cast<int16_t>(by / mag * MEDIUM_FORWARD_MM_S);
        }
        return cmd;
    }

    g_state_name = "ATK_SEARCH";
    // Búsqueda — gira lento mientras avanza un poco.
    cmd.vy_mm_s = SLOW_FORWARD_MM_S;
    cmd.omega_centideg_s = 6000;  // 60°/s de rotación
    return cmd;
}

MotorCommand goalkeeper_tick() {
    MotorCommand cmd{};

    if (!world_model_match_running()) {
        g_state_name = "GK_WAIT_START";
        return cmd;
    }

    if (world_model_imminent_exit()) {
        g_state_name = "GK_LINE_AVOID";
        const float line_angle = world_model_get_line_angle_deg();
        const float retreat_angle = line_angle + 180.0f;
        const float rad = retreat_angle * (M_PI / 180.0f);
        cmd.vx_mm_s = static_cast<int16_t>(std::sin(rad) * SLOW_FORWARD_MM_S);
        cmd.vy_mm_s = static_cast<int16_t>(std::cos(rad) * SLOW_FORWARD_MM_S);
        return cmd;
    }

    if (world_model_ball_visible()) {
        g_state_name = "GK_INTERCEPT";
        // Moverse lateralmente para alinearse con la X de la pelota.
        const float bx = world_model_get_ball_x_mm();
        cmd.vx_mm_s = static_cast<int16_t>(bx > 0 ? SLOW_FORWARD_MM_S : -SLOW_FORWARD_MM_S);
        return cmd;
    }

    g_state_name = "GK_PATROL";
    // Patrullar lateralmente — placeholder con velocidad lateral oscilante.
    // Real strategy va en Hito 6.
    static int direction = 1;
    static uint32_t last_change = 0;
    if (millis() - last_change > 2000) {
        direction = -direction;
        last_change = millis();
    }
    cmd.vx_mm_s = direction * SLOW_FORWARD_MM_S / 2;
    return cmd;
}

}  // namespace

void strategy_init() {
    g_state_name = "INIT";
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
