// motors_zircon.h — Driver de los 3 motores omni en el Zircon
//
// Aplica cinemática inversa para convertir comandos del robot (vx, vy, omega)
// en PWM por motor, y los aplica a los H-bridges de la placa Zircon Rev v15.

#pragma once
#include <stdint.h>
#include "types.h"

namespace iitasoccer {

// Inicializa los pines de motor según la macro ROBOT1/ROBOT2 (config_zircon.h).
void motors_init();

// Aplica un MotorCommand del protocolo (vx, vy, omega + kicker_fire).
// El kicker se ignora si el robot es arquero (sin kicker físico — futuro).
void motors_apply_command(const MotorCommand& cmd);

// Frena los 3 motores (PWM = 0, INA/INB = 0). Llamar desde el watchdog.
void motors_stop();

// Aplica PWM individual signed a un motor (0, 1, 2). Util para debug.
//   pwm positivo → adelante (INA=1, INB=0)
//   pwm negativo → reversa  (INA=0, INB=1)
//   pwm == 0     → libre
void motors_set_one(int motor_idx, int pwm_signed);

}  // namespace iitasoccer
