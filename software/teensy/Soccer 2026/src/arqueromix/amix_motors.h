// amix_motors.h — PRIMITIVAS DE MOTOR DIRECTAS del ARQUERO 2025 (analogWrite/digitalWrite).
//
// CONTRATO. Port 1:1 de las funciones de movimiento del arquero 2025
// (definitivo-arquero_6-9-2026, L140-233 + inline de los PATEANDO_*_arquero). NO usa
// cinemática inversa ni mixer: cada primitiva fija PWM + sentido por motor DIRECTAMENTE,
// exactamente como el 2025. Mismo patrón que centralmix/mix_motors.h (delantero).
//
// PINES: los de amix_config.h (Zircon 2026: M1=2/5/3, M2=8/7/6, M3=11/12/4).
// SENTIDO: amix_set_motor() aplica AMIX_MOTOR_INVERT[idx] ({+1,+1,+1}).
//
// ⚠️ El sentido FÍSICO de cada primitiva (strafe der/izq, avanzar, retroceder) DEBE
// re-verificarse en banco con las ruedas al aire: una primitiva puede salir al revés
// o lateral con el pinout 2026. Compila != anda.
//
// ⚠️ NO TESTEADO EN HARDWARE.

#pragma once

namespace iitasoccer {
namespace arqmix {

// Inicializa los 9 pines de motor (3× INA/INB/PWM). Llamar en setup().
void amix_motors_init();

// Helper atómico: PWM CON SIGNO a un motor (0=M1, 1=M2, 2=M3).
//   >0 → INA=1,INB=0 ; <0 → INA=0,INB=1 ; 0 → INA=0,INB=0 (coast/parado).
// Aplica AMIX_MOTOR_INVERT[idx] y clampea |pwm| a AMIX_MAX_PWM.
void amix_set_motor(int idx, int pwm_signed);

// --- Primitivas del ARQUERO 2025 (port 1:1) ---

void parar();   // frena los 3 motores (INA=INB=0).

// PATRULLA con corrección de rumbo en 3 bandas según `error` (= heading - heading_inicial).
// `pd` = factor proporcional (1.0 base / 1.5 con pelota desviada). Port de
// adproporcional()/aiproporcional() del 2025 (L186-233). Las delanteras (M1,M2) van al
// mismo sentido; la trasera (M3) al opuesto y su magnitud cambia por banda para enderezar.
void adproporcional(float pd, float error);  // strafe "derecha" (2025)
void aiproporcional(float pd, float error);  // strafe "izquierda" (2025)

// Impulso inicial / anti-traba: strafe fuerte (M1=M2=90, M3=-153). Mismo patrón de
// signo que adproporcional (strafe derecha). Los impulso_der/izq del FSM reusan
// adproporcional/aiproporcional con timing (como el 2025), no una primitiva aparte.
void impulso_inicial_mov();

// Avance recto (M1=+100, M2=-100, M3=0). (L152-154)
void avanzar();

// Avance del HOMING de inicio (despegarse de la línea del área antes de patrullar). Primitiva
// DEDICADA con PWM propio (AMIX_INICIO_AVANCE_PWM), separada de avanzar() para que sea un IMPULSO
// breve y suave (no acumula deriva de yaw → no se va chueco) sin tocar el avance del despeje.
void avanzar_inicio();

// Patada del arquero: RAMPA DE ACELERACIÓN (como el delantero centralmix, pedido Virginia
// 2026-06-21): sube de 0 a AMIX_KICK_VEL_FINAL de a pasos. M1=+vel, M2=-vel (SIMÉTRICO = recto
// al frente → "apunta" bien). NO bloqueante; se resetea en parar()/init.
void avanzar_patear();

// Giro PURO en el lugar (rotación del omni-3 sin trasladar): los 3 motores al MISMO sentido.
// Se usa para ALINEAR el frente del robot al ARCO RIVAL antes de despejar (el despeje sale recto
// al frente, así que "apuntar al arco" = girar hasta tenerlo al frente). pwm_signed>0 = un sentido,
// <0 = el otro. ⚠️ El SENTIDO físico se RE-VERIFICA en banco como el resto de las primitivas.
void girar(int pwm_signed);

// Retroceso recto de la secuencia de despeje (inline 2025, L1186-1188):
//   M1=-150, M2=+150, M3=0 (= opuesto de avanzar).
void patear_atras();

// Retroceso del HOMING de inicio (ir hacia el arco propio a buscar la línea del área). Primitiva
// DEDICADA con PWM propio (AMIX_INICIO_RETRO_PWM) y dirección flippable (AMIX_INICIO_RETRO_SIGN),
// para no acoplarse al retroceso del despeje y poder darle vuelta el sentido sin tocar el resto.
void retroceder_inicio();

}  // namespace arqmix
}  // namespace iitasoccer
