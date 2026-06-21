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
    amix_set_motor(0, m0);
    amix_set_motor(1, m1);
    amix_set_motor(2, m2);
}

// impulso_inicial 2025 (L1018-1020): M1=+90, M2=+90, M3=-153 (strafe fuerte = patrón
// adproporcional). El SENTIDO físico (a qué lado) se confirma en banco.
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

// avanzar_patear() 2025 (L174-178), arquero = PWM FIJO inmediato (sin rampa):
//   M1=+patadM1(250), M2=-patadM2(150), M3=0.
void avanzar_patear() {
    amix_set_motor(0, +AMIX_PATAD_M1);
    amix_set_motor(1, -AMIX_PATAD_M2);
    amix_set_motor(2, 0);
}

// PATEANDO_atras_arquero inline 2025 (L1186-1188): M1=-150, M2=+150, M3=0 (recto atrás).
void patear_atras() {
    amix_set_motor(0, -AMIX_ATRAS);
    amix_set_motor(1, +AMIX_ATRAS);
    amix_set_motor(2, 0);
}

}  // namespace arqmix
}  // namespace iitasoccer
