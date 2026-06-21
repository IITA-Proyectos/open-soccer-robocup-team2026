// mix_motors.cpp — PRIMITIVAS DE MOTOR DIRECTAS estilo 2025 (analogWrite/digitalWrite).
//
// Port 1:1 de las funciones de movimiento del delantero 2025
// (software/_deprecated-2025/robot-delantero/delantero-sin-zirconLib.cpp, rama
// #define ROBOT2). Cada primitiva fija PWM + sentido por motor DIRECTAMENTE, sin
// cinemática inversa ni mixer, exactamente como el 2025.
//
// MAPEO DE PINES: se usan los pines R1 (Zircon 2026) de mix_config.h, NO los del
// .cpp 2025. La correspondencia es POR ÍNDICE DE MOTOR (M1/M2/M3): cada primitiva
// conserva la magnitud de PWM y el patrón de sentido (INA,INB) de su motor 2025, y
// se rutea al pin R1 del mismo índice. El SENTIDO lo da SIEMPRE la pareja INA/INB,
// nunca el signo del PWM (el PWM va siempre |mag|, 0..255 a analogWrite).
//
// CONVENCIÓN DE mix_set_motor(idx, pwm_signed) (primitiva atómica):
//   pwm_signed > 0  → INA=1, INB=0  (sentido "positivo")
//   pwm_signed < 0  → INA=0, INB=1  (sentido "negativo")
//   pwm_signed == 0 → INA=0, INB=0  (frenado/parado, igual que parar() 2025)
// Aplica MIX_MOTOR_INVERT[idx] (invierte el sentido sin tocar la magnitud) y clampea
// |pwm| a MIX_MAX_PWM. Todas las primitivas de abajo se reescriben sobre esta base:
// una llamada 2025 "analogWrite(PWMn, mag); INAn=d; INBn=!d;" equivale a
// mix_set_motor(idx, d ? +mag : -mag).
//
// ⚠️ El 2025 estaba escrito para los pines ROBOT2 2025 (otro mapeo físico). Con el
// pinout R1, el SENTIDO FÍSICO de cada primitiva (avanzar/girar/centrar/patear) DEBE
// re-verificarse en banco: una primitiva puede salir lateral o al revés. Compila != anda.
//
// ⚠️ NO TESTEADO EN HARDWARE.

#include <Arduino.h>

#include "mix_motors.h"
#include "mix_config.h"

namespace iitasoccer {
namespace mix {

// ============================================================
// Tabla de pines por índice de motor (0=M1, 1=M2, 2=M3) — espejo de mix_config.h.
// ============================================================
static const int kINA[3] = { MIX_PIN_INA1, MIX_PIN_INA2, MIX_PIN_INA3 };
static const int kINB[3] = { MIX_PIN_INB1, MIX_PIN_INB2, MIX_PIN_INB3 };
static const int kPWM[3] = { MIX_PIN_PWM1, MIX_PIN_PWM2, MIX_PIN_PWM3 };

// ============================================================
// Estado del kicker (port de las globales 2025 velocidadActualPateo /
// tiempoAnteriorPateo). Estáticas de archivo: no se exponen fuera de este .cpp.
// ============================================================
static int           s_kick_vel      = 0;  // velocidadActualPateo
static unsigned long s_kick_prev_ms   = 0;  // tiempoAnteriorPateo
static bool          s_kick_active     = false; // ¿ya estamos en una rampa de patada?
                                               // (para resetear s_kick_vel AL ENTRAR;
                                               //  el 2025 NO reseteaba entre patadas)

// ============================================================
// mix_motors_init — pinMode de los 9 pines de motor (port del setup() 2025).
// ============================================================
void mix_motors_init() {
    for (int idx = 0; idx < 3; ++idx) {
        pinMode(kINA[idx], OUTPUT);
        pinMode(kINB[idx], OUTPUT);
        pinMode(kPWM[idx], OUTPUT);
    }
    // Estado inicial seguro: los 3 motores frenados.
    for (int idx = 0; idx < 3; ++idx) {
        digitalWrite(kINA[idx], LOW);
        digitalWrite(kINB[idx], LOW);
        analogWrite(kPWM[idx], 0);
    }
    s_kick_vel    = 0;
    s_kick_prev_ms = millis();
    s_kick_active  = false;
}

// ============================================================
// mix_set_motor — primitiva atómica. Aplica MIX_MOTOR_INVERT y clampea.
// ============================================================
void mix_set_motor(int idx, int pwm_signed) {
    if (idx < 0 || idx > 2) return;

    // Sentido efectivo tras aplicar el invert de HW de este motor.
    const int signed_eff = pwm_signed * MIX_MOTOR_INVERT[idx];

    // Magnitud para analogWrite (siempre positiva, clampeada a MIX_MAX_PWM).
    int mag = signed_eff;
    if (mag < 0) mag = -mag;
    if (mag > MIX_MAX_PWM) mag = MIX_MAX_PWM;

    // Dirección por la pareja INA/INB (NUNCA por el signo del PWM).
    if (signed_eff > 0) {
        digitalWrite(kINA[idx], HIGH);
        digitalWrite(kINB[idx], LOW);
    } else if (signed_eff < 0) {
        digitalWrite(kINA[idx], LOW);
        digitalWrite(kINB[idx], HIGH);
    } else {
        // 0 → frenado (ambas bajas, como parar() 2025).
        digitalWrite(kINA[idx], LOW);
        digitalWrite(kINB[idx], LOW);
    }

    analogWrite(kPWM[idx], mag);
}

// ============================================================
// Primitivas 2025 (port 1:1). Cada llamada documenta el original
// "analogWrite(PWMn, mag); INAn=d; INBn=!d;" → mix_set_motor(idx, d ? +mag : -mag).
// ============================================================

// parar() 2025: los 3 motores con PWM=0, INA=0, INB=0 → frenado.
void parar() {
    mix_set_motor(0, 0);
    mix_set_motor(1, 0);
    mix_set_motor(2, 0);
}

// girar() 2025: 3 ruedas a 100*g = 100*0.3 = 30, todas con INA=0/INB=1 (sentido neg).
void girar() {
    const int mag = (int)(100.0f * MIX_G);  // 30
    mix_set_motor(0, -mag);  // M1: INA1=0, INB1=1
    mix_set_motor(1, -mag);  // M2: INA2=0, INB2=1
    mix_set_motor(2, -mag);  // M3: INA3=0, INB3=1
}

// avanzar() 2025: M1=100 (INA1=1), M2=100 (INB2=1), M3=0.
void avanzar() {
    mix_set_motor(0, +100);  // M1: INA1=1, INB1=0
    mix_set_motor(1, -100);  // M2: INA2=0, INB2=1
    mix_set_motor(2, 0);     // M3: PWM=0, INA3=1, INB3=0 → magnitud 0 = frenado
}

// ------------------------------------------------------------
// Retrocesos / escape de línea blanca (los llama el FSM: DETECTA_LINEA_1/2/3).
// Valores RE-TUNEADOS por Elías en banco (2026-06-19, commit e83d43c): con el
// pinout/geometría R1 el mapeo línea→escape del port 2025 quedaba cruzado;
// Elías reasignó los patrones probando el robot. Decode (geometría {330,210,90},
// +X=derecha, +Y=adelante) verificado contra las transiciones del FSM:
//   retroceder1 ← DETECTA_LINEA_1 (línea a la IZQUIERDA) → escapa DERECHA+adelante
//   retroceder2 ← DETECTA_LINEA_2 (línea al FRENTE)       → escapa hacia ATRÁS
//   retroceder3 ← DETECTA_LINEA_3 (línea a la DERECHA)    → escapa IZQUIERDA+adelante
// (Convención de sentido por rueda: ver mix_set_motor arriba.)
// ------------------------------------------------------------

// retroceder1 — línea a la izquierda → escapa a la derecha+adelante. (banco Elías)
void retroceder1() {
    mix_set_motor(0, +100);
    mix_set_motor(1, 0);
    mix_set_motor(2, -100);
}

// retroceder2 — línea al frente → escapa hacia atrás. (banco Elías)
void retroceder2() {
    mix_set_motor(0, -100);
    mix_set_motor(1, +100);
    mix_set_motor(2, 0);
}

// retroceder3 — línea a la derecha → escapa a la izquierda+adelante. (banco Elías)
void retroceder3() {
    mix_set_motor(0, 0);
    mix_set_motor(1, -100);
    mix_set_motor(2, +100);
}

// ============================================================
// Kicker — empuje por inercia con rampa NO BLOQUEANTE (port 1:1 del 2025).
//
// 2025 avanzar_patear(): cada intervaloPateo(20ms) sube velocidadActualPateo en
// pasoPateo(5) hasta velocidadFinalPateo(240), y aplica:
//   M1 = vel (INA1=1, INB1=0)
//   M2 = vel (INA2=0, INB2=1)
//   M3 = 0   (INA3=0, INB3=0)
//
// DIFERENCIA INTENCIONAL vs 2025: el 2025 NO reseteaba velocidadActualPateo entre
// patadas, así que una segunda patada arrancaba ya a tope. Acá SÍ se resetea AL
// ENTRAR a una rampa nueva (s_kick_active pasa de false→true), para que cada patada
// arranque desde 0 y haga la rampa completa. La FSM debe llamar a kick_reset()
// (vía parar()/otra primitiva) o simplemente dejar de llamar avanzar_patear() entre
// patadas: la próxima vez que vuelva a llamarla, detecta la transición y resetea.
// ============================================================
void avanzar_patear() {
    // Al (re)entrar a la rampa: resetear la velocidad a 0 (NO como el 2025).
    if (!s_kick_active) {
        s_kick_active  = true;
        s_kick_vel     = 0;
        s_kick_prev_ms = millis();
    }

    const unsigned long now = millis();

    // Subir la velocidad cada MIX_KICK_INTERVALO_MS (no bloqueante, con millis()).
    if (now - s_kick_prev_ms >= (unsigned long)MIX_KICK_INTERVALO_MS) {
        s_kick_prev_ms = now;

        if (s_kick_vel < MIX_KICK_VEL_FINAL) {
            s_kick_vel += MIX_KICK_PASO;
            if (s_kick_vel > MIX_KICK_VEL_FINAL) {
                s_kick_vel = MIX_KICK_VEL_FINAL;
            }
        }

        // Aplicar la velocidad actual (igual patrón de sentido que el 2025).
        mix_set_motor(0, +s_kick_vel);  // M1: INA1=1, INB1=0
        mix_set_motor(1, -s_kick_vel);  // M2: INA2=0, INB2=1
        mix_set_motor(2, 0);            // M3: parado (INA3=0, INB3=0)
    }
    // Entre intervalos NO se reescriben los motores (idéntico al 2025: los pines
    // mantienen el último PWM aplicado por hardware).
}

// retroceder_patear() 2025: M1=patadM1(250, INB1=1), M2=patadM2(170, INA2=1), M3=0(INA3=1).
// Además marca el fin de la rampa de patada: la próxima avanzar_patear() reseteará.
void retroceder_patear() {
    s_kick_active = false;  // cierra la rampa actual → próxima patada arranca de 0
    mix_set_motor(0, -MIX_PATAD_M1);  // M1: INA1=0, INB1=1  (250)
    mix_set_motor(1, +MIX_PATAD_M2);  // M2: INA2=1, INB2=0  (170)
    mix_set_motor(2, 0);              // M3: PWM=0, INA3=1, INB3=0 → magnitud 0 = frenado
}

// ============================================================
// Centrado / orbitado de la pelota (port de CENTRANDO_horario / _antihorario 2025).
// Velocidades a c (MIX_C): M1/M2 = 60*c = 24, M3 = 180*c = 72.
// ============================================================

// CENTRANDO_horario: orbita la pelota en sentido horario (strafe-IZQ + giro CW).
// DEFAULT (sin flag) = port 2025 [-24,-24,+72]: casi puro strafe → orbita mal en R1.
// Con -DMIX_CENTRAR_ORBIT_2026 = candidato rebalanceado a rotación-dominante
// [+FRONT,+FRONT,+REAR] (ver mix_config.h MIX_CENTRAR_FRONT/REAR + el trade-off).
// Detrás del flag para que Elías lo valide en banco antes de dar el OK.
void centrar_horario() {
#ifdef MIX_CENTRAR_ORBIT_2026
    mix_set_motor(0, +MIX_CENTRAR_FRONT);  // M1 delantera-IZQ
    mix_set_motor(1, +MIX_CENTRAR_FRONT);  // M2 delantera-DER
    mix_set_motor(2, +MIX_CENTRAR_REAR);   // M3 trasera (rotación-dominante)
#else
    const int mag_fr = (int)(60.0f  * MIX_C);  // 24 (ruedas delanteras M1/M2)
    const int mag_re = (int)(180.0f * MIX_C);  // 72 (rueda trasera M3)
    mix_set_motor(0, -mag_fr);  // M1: INA1=0, INB1=1
    mix_set_motor(1, -mag_fr);  // M2: INA2=0, INB2=1
    mix_set_motor(2, +mag_re);  // M3: INA3=1, INB3=0
#endif
}

// CENTRANDO_antihorario: espejo exacto del horario (strafe-DER + giro CCW).
void centrar_antihorario() {
#ifdef MIX_CENTRAR_ORBIT_2026
    mix_set_motor(0, -MIX_CENTRAR_FRONT);  // espejo: niega las 3 ruedas
    mix_set_motor(1, -MIX_CENTRAR_FRONT);
    mix_set_motor(2, -MIX_CENTRAR_REAR);
#else
    const int mag_fr = (int)(60.0f  * MIX_C);  // 24
    const int mag_re = (int)(180.0f * MIX_C);  // 72
    mix_set_motor(0, +mag_fr);  // M1: INA1=1, INB1=0
    mix_set_motor(1, +mag_fr);  // M2: INA2=1, INB2=0
    mix_set_motor(2, -mag_re);  // M3: INA3=0, INB3=1
#endif
}

// ============================================================
// Medialuna de arranque (KICKOFF_SEEK). Combinación DIRECTA de PWM por rueda, estilo
// impulso_inicial_girando(): cada motor con su valor de mix_config (el signo da el
// sentido). mix_set_motor clampea cada rueda a ±MIX_MAX_PWM.
// ⚠️ Sentido/curvatura A CONFIRMAR EN BANCO (igual que el resto de las primitivas; si
// curva al revés, invertí los signos de MIX_KICKOFF_M1/M2/M3).
// ============================================================
void kickoff_medialuna() {
    mix_set_motor(0, MIX_KICKOFF_ARC_PWD*80);   // M1 delantera IZQ
    mix_set_motor(1, MIX_KICKOFF_ARC_PWD*80);   // M2 delantera DER
    mix_set_motor(2, MIX_KICKOFF_ARC_PWD*MIX_KICKOFF_ARC_CURV*170);   // M3 trasera
}

}  // namespace mix
}  // namespace iitasoccer
