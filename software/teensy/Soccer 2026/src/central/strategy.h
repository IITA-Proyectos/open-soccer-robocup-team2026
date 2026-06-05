// strategy.h — FSM dual (delantero / arquero) para el robot.
//
// El role se fija en build por -DROBOT1 (arquero) / -DROBOT2 (delantero); se
// aplica en setup() via apply_role_from_dipswitch() (nombre historico). No hay
// dipswitch fisico: el rol es una constante de compilacion, no se lee en runtime.
//
// La FSM aplica decisiones tácticas usando world_model.h como entrada y
// genera un MotorCommand como salida. NO toca hardware directamente — eso
// queda en motors.h (envía al Zircon).
//
// Estrategias implementadas: Nivel 1 (FSM base por rol) + Nivel 2 (delantero con
// behind-the-ball, arquero con predicción ball_predict). Ya no es un stub.

#pragma once
#include <stdint.h>
#include "types.h"

namespace iitasoccer {

enum class RobotRole : uint8_t {
    GOALKEEPER = 0,
    ATTACKER   = 1,
};

enum class AttackColor : uint8_t {
    CYAN    = 0,
    MAGENTA = 1,
};

void strategy_init();

// Llamado a 100 Hz desde el loop principal. Lee world_model y produce
// el siguiente MotorCommand que se enviará al Zircon.
MotorCommand strategy_tick();

// Setters / consultas:
void        strategy_set_role(RobotRole role);
RobotRole   strategy_get_role();

// Polaridad de campo configurable — resuelve T1 de la auditoría del striker
// (ARCO_CONTRINCANTE hardcoded a amarillo). Llamar antes del partido.
void        strategy_set_attack_color(AttackColor color);
AttackColor strategy_get_attack_color();

// Para debug: qué estado interno está la FSM.
const char* strategy_get_state_name();

}  // namespace iitasoccer
