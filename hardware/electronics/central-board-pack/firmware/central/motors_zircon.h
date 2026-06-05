// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
// motors_zircon.h — Driver de los 3 motores omni en el Zircon
//
// Aplica cinemática inversa para convertir comandos del robot (vx, vy, omega)
// en PWM por motor, y los aplica a los H-bridges de la placa Zircon Rev v15.

#pragma once
#include <stdint.h>
#include "types.h"

namespace iitasoccer {

// Inicializa los pines de motor según la macro ROBOT1/ROBOT2 (config_central.h).
void motors_init();

// Aplica un MotorCommand del protocolo (vx, vy, omega + kicker_fire).
// El kicker se ignora si el robot es arquero (sin kicker físico — futuro).
void motors_apply_command(const MotorCommand& cmd);

// Frena los 3 motores en modo libre (PWM = 0, INA/INB = 0).
// Motor queda libre — frena por fricción mecánica + back-EMF mínimo.
// Usar para estado default / watchdog (no urgente).
void motors_stop();

// Freno activo (PWM = 0, INA = INB = 1 → corto en H-bridge).
// El motor frena más rápido que motors_stop() pero estresa los drivers.
// Usar SOLO en EMERGENCY_LINE u otra emergencia real.
void motors_brake();

// Aplica PWM individual signed a un motor (0, 1, 2). Util para debug.
//   pwm positivo → adelante (INA=1, INB=0)
//   pwm negativo → reversa  (INA=0, INB=1)
//   pwm == 0     → libre
void motors_set_one(int motor_idx, int pwm_signed);

}  // namespace iitasoccer
