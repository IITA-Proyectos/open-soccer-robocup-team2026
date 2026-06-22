// amix_motors.cpp — PRIMITIVAS DE MOTOR DIRECTAS del ARQUERO 2025.
//
// Port 1:1 de definitivo-arquero_6-9-2026 (L140-233 + inline PATEANDO_*_arquero).
// Mapeo POR ÍNDICE de motor (M1=idx0, M2=idx1, M3=idx2): cada primitiva conserva la
// magnitud de PWM y el patrón de sentido (INA,INB) del 2025, ruteado al pin 2026 del
// mismo índice. El SENTIDO lo da SIEMPRE la pareja INA/INB; el PWM va siempre |mag|.
//
// Equivalencia con el 2025: "analogWrite(PWMn,mag); INAn=1; INBn=0;" → amix_set_motor(idx,+mag);
//                           "analogWrite(PWMn,mag); INAn=0; INBn=1;" → amix_set_motor(idx,-mag).
//
// ⚠️ NO TESTEADO EN HARDWARE. El sentido físico se re-verifica en banco.

#include <Arduino.h>
#include <math.h>

#include "amix_motors.h"
#include "amix_config.h"

namespace iitasoccer {
namespace arqmix {

static const int kINA[3] = { AMIX_PIN_INA1, AMIX_PIN_INA2, AMIX_PIN_INA3 };
static const int kINB[3] = { AMIX_PIN_INB1, AMIX_PIN_INB2, AMIX_PIN_INB3 };
static const int kPWM[3] = { AMIX_PIN_PWM1, AMIX_PIN_PWM2, AMIX_PIN_PWM3 };

// Estado de la RAMPA del golpe de despeje (port de la del delantero centralmix). Estáticos de
// archivo. s_kick_active se resetea en parar()/init → cada despeje arranca de 0 y rampa completa.
static int           s_kick_vel     = 0;
static unsigned long s_kick_prev_ms = 0;
static bool          s_kick_active  = false;

void amix_motors_init() {
    for (int idx = 0; idx < 3; ++idx) {
        pinMode(kINA[idx], OUTPUT);
        pinMode(kINB[idx], OUTPUT);
        pinMode(kPWM[idx], OUTPUT);
    }
    for (int idx = 0; idx < 3; ++idx) {  // estado inicial seguro: frenados
        digitalWrite(kINA[idx], LOW);
        digitalWrite(kINB[idx], LOW);
        analogWrite(kPWM[idx], 0);
    }
    s_kick_vel = 0; s_kick_prev_ms = 0; s_kick_active = false;
}

void amix_set_motor(int idx, int pwm_signed) {
    if (idx < 0 || idx > 2) return;
    const int signed_eff = pwm_signed * AMIX_MOTOR_INVERT[idx];
    int mag = signed_eff < 0 ? -signed_eff : signed_eff;
    if (mag > AMIX_MAX_PWM) mag = AMIX_MAX_PWM;
    if (signed_eff > 0) {
        digitalWrite(kINA[idx], HIGH);
        digitalWrite(kINB[idx], LOW);
    } else if (signed_eff < 0) {
        digitalWrite(kINA[idx], LOW);
        digitalWrite(kINB[idx], HIGH);
    } else {
        digitalWrite(kINA[idx], LOW);
        digitalWrite(kINB[idx], LOW);
    }
    analogWrite(kPWM[idx], mag);
}

void parar() {
    s_kick_active = false;   // cierra la rampa del golpe → el próximo despeje arranca de 0
    amix_set_motor(0, 0);
    amix_set_motor(1, 0);
    amix_set_motor(2, 0);
}

// aiproporcional() 2025 (L186-209): fronts (M1,M2) sentido NEGATIVO (INA=0/INB=1),
// rear (M3) POSITIVO (INA=1/INB=0). Magnitudes por banda de `error`.
void aiproporcional(float pd, float error) {
    const float e = error * AMIX_HEADING_CORRECT_SIGN;  // signo de corrección flippable (banco Virginia)
    int m0, m1, m2;
    if (e > -1.0f && e < 1.0f) {                     // banda centrada
        m0 = -(int)(pd * AMIX_PROP_FRONT_CENTER);
        m1 = -(int)(pd * AMIX_PROP_FRONT_CENTER);
        m2 = +(int)(pd * AMIX_PROP_REAR_CENTER);
    } else if (e > 0.0f) {                           // error > 0
        m0 = -(int)(pd * AMIX_PROP_FRONT_EPOS);
        m1 = -(int)(pd * AMIX_PROP_FRONT_EPOS);
        m2 = +(int)(pd * AMIX_AI_REAR_EPOS);
    } else {                                         // error < 0
        m0 = -(int)(pd * AMIX_PROP_M1_ENEG);         // "motor derecho" (idx0) 2025
        m1 = -(int)(pd * AMIX_PROP_M2_ENEG);         // "motor izquierdo" (idx1) 2025
        m2 = +(int)(pd * AMIX_AI_REAR_ENEG);
    }
    // SESGO FORWARD (banco Virginia 2026-06-21): micro-empuje RECTO al frente para no derivar atrás.
    // Signos FIJOS (M1 al +, M2 al - = patrón avanzar), NO sigue el signo del strafe. 0 = off.
    m0 += AMIX_FORWARD_BIAS_PWM;
    m1 -= AMIX_FORWARD_BIAS_PWM;
    amix_set_motor(0, m0);
    amix_set_motor(1, m1);
    amix_set_motor(2, m2);
}

// adproporcional() 2025 (L210-233): fronts (M1,M2) sentido POSITIVO (INA=1/INB=0),
// rear (M3) NEGATIVO (INA=0/INB=1). Espejo de aiproporcional salvo magnitud del rear.
void adproporcional(float pd, float error) {
    const float e = error * AMIX_HEADING_CORRECT_SIGN;  // signo de corrección flippable (banco Virginia)
    int m0, m1, m2;
    if (e > -1.0f && e < 1.0f) {                     // banda centrada
        m0 = +(int)(pd * AMIX_PROP_FRONT_CENTER);
        m1 = +(int)(pd * AMIX_PROP_FRONT_CENTER);
        m2 = -(int)(pd * AMIX_PROP_REAR_CENTER);
    } else if (e > 0.0f) {                           // error > 0
        m0 = +(int)(pd * AMIX_PROP_FRONT_EPOS);
        m1 = +(int)(pd * AMIX_PROP_FRONT_EPOS);
        m2 = -(int)(pd * AMIX_AD_REAR_EPOS);
    } else {                                         // error < 0
        m0 = +(int)(pd * AMIX_PROP_M1_ENEG);
        m1 = +(int)(pd * AMIX_PROP_M2_ENEG);
        m2 = -(int)(pd * AMIX_AD_REAR_ENEG);
    }
    // SESGO FORWARD (banco Virginia 2026-06-21): igual que aiproporcional — signos FIJOS, M1 al +, M2 al -.
    m0 += AMIX_FORWARD_BIAS_PWM;
    m1 -= AMIX_FORWARD_BIAS_PWM;
    amix_set_motor(0, m0);
    amix_set_motor(1, m1);
    amix_set_motor(2, m2);
}

// impulso_inicial — hoy M1=M2=+AMIX_IMP_INI_FRONT(70), M3=-AMIX_IMP_INI_REAR(110) (2025 era 90/153;
// bajado banco Virginia). ⚠️ DEAD CODE: el arranque actual es el homing (inicio_retroceder), NO esto.
void impulso_inicial_mov() {
    amix_set_motor(0, +AMIX_IMP_INI_FRONT);
    amix_set_motor(1, +AMIX_IMP_INI_FRONT);
    amix_set_motor(2, -AMIX_IMP_INI_REAR);
}

// avanzar() 2025 (L152-154): M1=+100, M2=-100, M3=0.
void avanzar() {
    amix_set_motor(0, +AMIX_AVANZAR);
    amix_set_motor(1, -AMIX_AVANZAR);
    amix_set_motor(2, 0);
}

// avanzar_inicio() — avance del HOMING (a ciegas tras detectar la línea). IDÉNTICO a avanzar()
// salvo la VELOCIDAD: usa AMIX_INICIO_AVANCE_PWM (más baja) en vez de AMIX_AVANZAR=100. MISMO sentido
// (M1=+, M2=-, M3=0), mismos 400 ms. Banco Virginia 2026-06-21: "anda de golpe" → SOLO baja la
// velocidad de ESTE avance, sin tocar el avanzar() del despeje (que sigue a 100).
void avanzar_inicio() {
    amix_set_motor(0, +AMIX_INICIO_AVANCE_PWM);
    amix_set_motor(1, -AMIX_INICIO_AVANCE_PWM);
    amix_set_motor(2, 0);
}

// avanzar_patear() — RAMPA DE ACELERACIÓN (port de la del delantero centralmix), pedido Virginia
// 2026-06-21. NO bloqueante: cada AMIX_KICK_INTERVALO_MS sube s_kick_vel en AMIX_KICK_PASO hasta
// AMIX_KICK_VEL_FINAL y aplica M1=+vel, M2=-vel (SIMÉTRICO = recto al frente, así "apunta" bien),
// M3=0. Se llama repetidamente desde PATEANDO_adelante. La rampa se resetea en parar()/init →
// cada golpe arranca de 0 y hace la rampa completa (no como el 2025, que iba a tope inmediato).
void avanzar_patear() {
    if (!s_kick_active) {           // (re)entrada a la rampa → arrancar de 0
        s_kick_active  = true;
        s_kick_vel     = 0;
        s_kick_prev_ms = millis();
    }
    const unsigned long now = millis();
    if (now - s_kick_prev_ms >= (unsigned long)AMIX_KICK_INTERVALO_MS) {
        s_kick_prev_ms = now;
        if (s_kick_vel < AMIX_KICK_VEL_FINAL) {
            s_kick_vel += AMIX_KICK_PASO;
            if (s_kick_vel > AMIX_KICK_VEL_FINAL) s_kick_vel = AMIX_KICK_VEL_FINAL;
        }
        amix_set_motor(0, +s_kick_vel);  // M1
        amix_set_motor(1, -s_kick_vel);  // M2 (sentido opuesto = avance RECTO)
        amix_set_motor(2, 0);            // M3
    }
    // Entre escalones NO se reescriben los motores (mantienen el último PWM) — igual que el delantero.
}

// girar() — ROTACIÓN PURA en el lugar. En un omni-3 a 120°, manejar las 3 ruedas con la MISMA
// velocidad tangencial gira el robot sin trasladarlo (vx=vy=0, ω≠0). Se usa para ALINEAR el frente
// al ARCO RIVAL antes del despeje. ⚠️ El SENTIDO (qué lado gira con +) se RE-VERIFICA en banco como
// el resto de las primitivas (el cableado 2026 puede invertirlo → perilla AMIX_GIRO_ALINEAR_SIGN).
void girar(int pwm_signed) {
    amix_set_motor(0, pwm_signed);
    amix_set_motor(1, pwm_signed);
    amix_set_motor(2, pwm_signed);
}

// patear_atras() — M1=-AMIX_ATRAS, M2=+AMIX_ATRAS, M3=0 (recto atrás). Hoy AMIX_ATRAS=120 (2025 era 150).
void patear_atras() {
    amix_set_motor(0, -AMIX_ATRAS);
    amix_set_motor(1, +AMIX_ATRAS);
    amix_set_motor(2, 0);
}

// retroceder_rumbo_opp() — retroceso (como patear_atras) PERO con control PROPORCIONAL de rumbo para
// MANTENER el frente apuntando al ARCO RIVAL (goal_opp). Pedido Virginia 2026-06-21 (la inercia tras la
// alineación lo desalineaba). El término de rotación `rot` (∝ ángulo al arco, con BANDA MUERTA y TOPE —
// guía skill control-pid-zona-muerta) se SUMA a las 3 ruedas (patrón girar); la autoridad de rotación
// sale del desbalance M1/M2 (el M3 queda bajo su piso). Mismo signo que ALINEAR_arco_opp.
void retroceder_rumbo_opp(float goal_opp_angle, bool goal_opp_visible) {
    int rot = 0;
    if (goal_opp_visible && fabsf(goal_opp_angle) > AMIX_DEADBAND_OPP_DEG) {  // banda muerta: no perseguir ruido
        rot = (int)(AMIX_KP_RUMBO_OPP * goal_opp_angle) * AMIX_GIRO_ALINEAR_SIGN;
        if (rot >  AMIX_ROT_MAX) rot =  AMIX_ROT_MAX;   // tope (skill: no subir el clamp para "ganarle")
        if (rot < -AMIX_ROT_MAX) rot = -AMIX_ROT_MAX;
    }
    amix_set_motor(0, -AMIX_ATRAS + rot);   // M1 (atrás + rotación)
    amix_set_motor(1, +AMIX_ATRAS + rot);   // M2 (atrás + rotación)
    amix_set_motor(2,  0          + rot);   // M3 (rotación)
}

// retroceder_inicio() — retroceso del HOMING de inicio. Mismo patrón que patear_atras
// (M1=-, M2=+ = hacia ATRÁS) pero con PWM propio y signo flippable. Si al GO el robot va hacia
// ADELANTE en vez de atrás, -DARQMIX_FLIP_INICIO_RETRO invierte el sentido.
void retroceder_inicio() {
    const int p = AMIX_INICIO_RETRO_PWM * AMIX_INICIO_RETRO_SIGN;
    amix_set_motor(0, -p);
    amix_set_motor(1, +p);
    amix_set_motor(2, 0);
}

}  // namespace arqmix
}  // namespace iitasoccer
