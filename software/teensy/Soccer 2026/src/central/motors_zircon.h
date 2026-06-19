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

// Último PWM signed APLICADO al motor i (post-pisos/escala/kickstart — lo que de
// verdad llegó al H-bridge). Para telemetría/caja negra. Siempre disponible
// (costo: 3 stores de int16 por apply — despreciable).
int16_t motors_get_applied_pwm(int motor_idx);

#ifdef CENTRAL_REAR_BRAKE_LEAD
// FRENO ANTICIPADO DE LA TRASERA (técnica 2025; banco robot2 2026-06-09). Con cut=true,
// motors_apply_command fuerza PWM=0 en la rueda TRASERA (idx 2) mientras las delanteras
// siguen. El CALLER (diag/FSM, que es quien sabe cuándo termina el movimiento) lo activa
// en los últimos ms del tramo: la trasera rueda de más por inercia y desacomoda el robot
// al frenar. Gateado: sin -DCENTRAL_REAR_BRAKE_LEAD esta API no existe (binario idéntico).
void motors_set_rear_cut(bool cut);
#endif

#ifdef CENTRAL_REAR_TRIM
// TRIM DE RUMBO POR LA RUEDA TRASERA (Cambio 1, Gustavo 2026-06-17). El caller (FSM del
// arquero) computa una PEQUEÑA corrección de PWM con un PI sobre el heading del TOP y la
// pasa acá; motors_apply_command la SUMA a la trasera (idx 2) DESPUÉS del floor-scale —
// donde la trasera ya está sobre su piso (107→~150), con holgura — para corregir el rumbo
// sin desbordar las delanteras (que a baja velocidad están pegadas a su piso). delta=0 →
// no-op. Gateado: sin -DCENTRAL_REAR_TRIM esta API no existe (binario idéntico).
void motors_set_rear_trim(int delta_pwm);
// ESCALA DE LA TRASERA (Gustavo 2026-06-18): el caller la baja (<1.0) al ARRANCAR el escape para
// compensar que la trasera, alineada al movimiento, tiene menos rozamiento y acelera ANTES que las
// delanteras (oblicuas, mas rozamiento) -> sin esto el robot cruza adelante en el transitorio.
// Multiplica el PWM de la trasera: preserva el signo (reduce magnitud en ambos sentidos). 1.0 = sin cambio.
void motors_set_rear_scale(float scale);
// ESCALA DE LAS DELANTERAS (Gustavo 2026-06-18): el caller la SUBE (>1.0) durante el escape para que
// las delanteras (oblicuas, cerca de su piso a 0.5*vx) empujen mas y la huida salga RECTA, sumando
// potencia lateral por las ruedas con holgura (no la trasera, que satura/diagonaliza). 1.0 = sin cambio.
void motors_set_front_scale(float scale);
#endif

#ifdef GK_PINGPONG
// MANEJO DIRECTO POR RUEDA (Gustavo 2026-06-18) — patrulla/escape del arquero ping-pong.
// Pasa el PWM CRUDO de cada rueda (m1=delantera izq, m2=delantera der, m3=trasera; -150..150).
// El proximo motors_apply_command los escribe DIRECTO: sin mixer, sin pisos, sin coeficientes;
// solo kickstart (romper inercia) + (si -DCENTRAL_REAR_TRIM) el trim de rumbo en la trasera.
// One-shot por tick (si la FSM no lo llama, ese tick va por el mixer normal).
void motors_drive_raw3(int m1, int m2, int m3);
#endif

}  // namespace iitasoccer
