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

// ------------------------------------------------------------
// Primitiva HOLONÓMICA — moverse en CUALQUIER dirección + girar a la vez.
//
// La necesita el rodeo estilo Edge (mix_fsm_edge): traslación en un ángulo arbitrario
// (la curva de rodeo) MIENTRAS el frente se orienta al arco. Las primitivas 2025
// (avanzar/girar/centrar) son fijas y no alcanzan para esto.
//
// CINEMÁTICA (reconstruida desde el banco de Elías — avanzar/girar/retroceder — y
// VERIFICADA con él el 2026-06-23; geometría R1 M1=delIZQ, M2=delDER, M3=trasera):
//     w_M1 = +0.5·vx + 0.866·vy + omega
//     w_M2 = +0.5·vx − 0.866·vy + omega
//     w_M3 = −1.0·vx +    0     + omega
// con vx = componente DERECHA, vy = componente FRENTE (vx=speed·sin, vy=speed·cos), y
// omega = MISMO PWM sumado a las 3 ruedas (giro puro; signo = sentido). Trabaja DIRECTO
// en PWM (sin mm/s ni max_speed → no hereda la calibración del kinematics.cpp del repo).
//
//   go_ang_deg : dirección de traslación. 0 = frente, + = derecha (MISMA convención que
//                g_io.angulo_pelota_deg). Es hacia DÓNDE se mueve el robot, no hacia
//                dónde mira.
//   speed      : PWM de traslación (0..~240). Pico de rueda ≈ speed.
//   omega      : PWM de giro, sumado igual a las 3 ruedas. + / − = un sentido u otro
//                (perilla de banco; ver MIX_EDGE_FACE_KP). 0 = no girar.
//
// Satura por ESCALADO (si el pico de rueda pasa MIX_MAX_PWM, escala las 3 por el mismo
// factor) → preserva la dirección del movimiento, igual que avanzar_patear / saturate_wheels.
// ⚠️ Sentido físico anclado a avanzar() (banco); el signo de omega se confirma en banco.
void mix_mover_vector(float go_ang_deg, int speed, int omega);

}  // namespace mix
}  // namespace iitasoccer
