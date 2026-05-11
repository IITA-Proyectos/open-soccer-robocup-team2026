// strategy.h — FSM dual (delantero / arquero) para el robot.
//
// El role se selecciona en setup() por dipswitch (Q7 del coach: roles fijos
// inicialmente). PIN_ROLE_DIPSWITCH en config_top.h:
//   LOW  = arquero (defender)
//   HIGH = delantero (atacar)
//
// La FSM aplica decisiones tácticas usando world_model.h como entrada y
// genera un MotorCommand como salida. NO toca hardware directamente — eso
// queda en motors.h (envía al Zircon).
//
// Stub inicial (Hito 4): FSM mínima. Las estrategias completas (delantero
// con behind-the-ball, arquero con predicción) llegan en Hito 6.

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
