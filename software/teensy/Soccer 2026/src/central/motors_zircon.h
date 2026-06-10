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

// Aplica un MotorCommand del protocolo (vx, vy, omega).
// El robot no tiene kicker físico — el delantero empuja la pelota por inercia.
void motors_apply_command(const MotorCommand& cmd);

// Frena los 3 motores en modo libre (PWM = 0, INA/INB = 0).
// Motor queda libre — frena por fricción mecánica + back-EMF mínimo.
// Usar para estado default / watchdog (no urgente).
void motors_stop();

// "Freno" (PWM = 0, INA = INB = 1).  ⚠️ #41/#11 SIN CONFIRMAR EN HW: esto es freno
// activo (corto en el H-bridge) SOLO si el chip driver hace short-brake con esa
// combinación. En drivers tipo VNH (PWM = enable) PWM=0 DESHABILITA el puente → el
// motor queda en COAST (libre), NO frena. EMERGENCY_LINE depende de que esto frene de
// verdad (<15 ms). PENDIENTE: identificar el integrado del Zircon Rev v15 (Enzo/datasheet)
// y medir en banco tiempo de parada brake vs stop. Ver auditoría 2026-06-03 #11.
// Usar SOLO en EMERGENCY_LINE u otra emergencia real.
void motors_brake();

// Aplica PWM individual signed a un motor (0, 1, 2). Util para debug.
//   pwm positivo → adelante (INA=1, INB=0)
//   pwm negativo → reversa  (INA=0, INB=1)
//   pwm == 0     → libre
void motors_set_one(int motor_idx, int pwm_signed);

#ifdef CENTRAL_REAR_BRAKE_LEAD
// FRENO ANTICIPADO DE LA TRASERA (técnica 2025; banco robot2 2026-06-09). Con cut=true,
// motors_apply_command fuerza PWM=0 en la rueda TRASERA (idx 2) mientras las delanteras
// siguen. El CALLER (diag/FSM, que es quien sabe cuándo termina el movimiento) lo activa
// en los últimos ms del tramo: la trasera rueda de más por inercia y desacomoda el robot
// al frenar. Gateado: sin -DCENTRAL_REAR_BRAKE_LEAD esta API no existe (binario idéntico).
void motors_set_rear_cut(bool cut);
#endif

}  // namespace iitasoccer
