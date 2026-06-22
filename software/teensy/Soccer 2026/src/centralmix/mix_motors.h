// mix_motors.h — PRIMITIVAS DE MOTOR DIRECTAS estilo 2025 (analogWrite/digitalWrite).
//
// CONTRATO. Port 1:1 de las funciones de movimiento del delantero 2025
// (delantero-sin-zirconLib.cpp). NO usa cinemática inversa ni mixer: cada primitiva
// fija PWM + sentido por motor DIRECTAMENTE, exactamente como el 2025.
//
// PINES: los de mix_config.h (R1 Zircon: M1=2/5/3, M2=8/7/6, M3=11/12/4).
// SENTIDO: mix_set_motor() aplica MIX_MOTOR_INVERT[idx] ({+1,+1,+1} en R1).
//
// ⚠️ El 2025 estaba escrito para los pines ROBOT2 2025 (otro mapeo). Al portar a
// los pines R1, el SENTIDO FÍSICO de cada primitiva (avanzar/girar/centrar) DEBE
// re-verificarse en banco: una primitiva "avanzar" 2025 puede salir lateral o al
// revés con el pinout nuevo. El implementador NO debe asumir que compila == anda.
//
// ⚠️ NO TESTEADO EN HARDWARE.

#pragma once

namespace iitasoccer {
namespace mix {

// Inicializa pinMode de los 9 pines de motor (3× INA/INB/PWM). Llamar en setup().
void mix_motors_init();

// --- Helper de bajo nivel ---
// Aplica un PWM CON SIGNO a un motor por índice (0=M1, 1=M2, 2=M3):
//   pwm_signed > 0  → sentido "positivo" del motor
//   pwm_signed < 0  → sentido "negativo"
//   pwm_signed == 0 → frenado/parado de ese motor
// Aplica MIX_MOTOR_INVERT[idx] y clampea |pwm| a MIX_MAX_PWM. Es la base sobre la
// que se pueden reescribir las primitivas de abajo (o usarse suelto para diagnóstico).
void mix_set_motor(int idx, int pwm_signed);

// --- Primitivas 2025 (port 1:1) ---
void parar();              // frena los 3 motores
void girar();              // gira en el lugar a 100*MIX_G (sentido 2025)
void avanzar();            // avanza (combinación de las 3 ruedas del 2025)
void retroceder1();        // retroceso variante 1 (salida de DETECTA_LINEA_1)
void retroceder2();        // retroceso variante 2 (salida de DETECTA_LINEA_2)
void retroceder3();        // retroceso variante 3 (salida de DETECTA_LINEA_3)

// Kicker (empuje por inercia) — RECTO y FUERTE con heading-hold del OTOS. Usa MIX_KICK_*
// de mix_config. NO BLOQUEANTE: arranca en MIX_KICK_VEL_START y rampa AGRESIVA hasta
// MIX_KICK_VEL_FINAL; en cada tick agrega un término de giro (clampeado a MIX_KICK_CORR_MAX)
// proporcional al error de rumbo del OTOS (otos_heading_deg) para ir derecho. Si el OTOS no
// está fresco/sano, va recto a ciegas. El rumbo objetivo se ancla al iniciar la patada (parar()
// cierra la rampa). Debe llamarse repetidamente desde el estado de patada. ⚠️ Signo del Kp a
// confirmar en banco (si curva más, invertir MIX_KICK_HEADING_KP).
void avanzar_patear();
void retroceder_patear();  // retroceso de patada con PWM crudo MIX_PATAD_M1 / MIX_PATAD_M2

// Centrado / orbitado de la pelota (port de CENTRANDO_horario / _antihorario 2025).
// Velocidades a MIX_C (las primitivas de impulso usan MIX_IC; se dejan implícitas en
// la FSM como en el 2025, o el implementador puede agregar centrar_*_impulso()).
void centrar_horario();      // orbita en sentido horario   (CENTRANDO_horario)
void centrar_antihorario();  // orbita en sentido antihorario (CENTRANDO_antihorario)

// Medialuna de arranque (KICKOFF): impulso FUERTE y CORTO = combinación DIRECTA de PWM por
// rueda (estilo impulso_inicial_girando), valores MIX_KICKOFF_M1/M2/M3 de mix_config.
// Lo llama el estado KICKOFF_SEEK del FSM (primer estado / arranque).
void kickoff_medialuna();

}  // namespace mix
}  // namespace iitasoccer
