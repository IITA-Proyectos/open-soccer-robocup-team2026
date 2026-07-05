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
#include "mix_io.h"      // g_io: otos_heading_deg / otos_confidence / frescura (patada recta)

namespace iitasoccer {
namespace mix {

// Normaliza un ángulo a [-180, 180]. Local a este .cpp (la patada recta lo usa para
// el error de rumbo del OTOS). Misma semántica que el wrap180 de mix_comm / mix_fsm.
static inline float wrap180(float deg) {
    while (deg >  180.0f) deg -= 360.0f;
    while (deg < -180.0f) deg += 360.0f;
    return deg;
}

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
static float         s_kick_heading_target = 0.0f; // rumbo OTOS anclado al iniciar la patada
                                                    // (objetivo del heading-hold = ir DERECHO)
static bool          s_kick_use_otos       = false; // ¿el OTOS estaba fresco/sano al entrar?
                                                     // (si no, la patada va recta "a ciegas")

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
    s_kick_heading_target = 0.0f;
    s_kick_use_otos       = false;
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
// + Cierra la rampa de patada (s_kick_active=false): la FSM llama parar() en TODA pausa y
//   en cada transición; así la próxima avanzar_patear() detecta el flanco de entrada y
//   RE-ANCLA el rumbo objetivo del OTOS, aunque la patada anterior se haya abortado por línea.
void parar() {
    mix_set_motor(0, 0);
    mix_set_motor(1, 0);
    mix_set_motor(2, 0);
    s_kick_active = false;
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
    mix_set_motor(0, +175);  // M1: INA1=1, INB1=0
    mix_set_motor(1, -175);  // M2: INA2=0, INB2=1
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
// Kicker — empuje RECTO y FUERTE con corrección de rumbo por OTOS (2026-06-21, pedido Elías).
//
// REEMPLAZA el avanzar_patear() lazo-abierto del 2025 (rampa lenta 0→240 sin
// realimentación, que CURVA si las 2 ruedas delanteras no están parejas) por:
//
//   1) EMPUJE FUERTE Y RÁPIDO. Arranca en MIX_KICK_VEL_START (sobre el piso del motor,
//      bite inmediato) y rampa AGRESIVA (MIX_KICK_PASO grande cada MIX_KICK_INTERVALO_MS
//      corto) hasta MIX_KICK_VEL_FINAL (alto). La rampa NO desaparece (evita brownout del
//      regulador por el pico de arranque), pero llega a tope en ~24 ms (era ~120 ms).
//      Patrón de avance del 2025:  M1=+vel (INA1=1) · M2=-vel (INB2=1) · M3=0.
//
//   2) DERECHO (heading-hold con el OTOS). Al ENTRAR a la patada (flanco false→true) se
//      ANCLA otos_heading_deg como objetivo y se decide si el OTOS es confiable AHORA
//      (confidence>0 + pose fresca). En cada tick se mide el error de rumbo y se agrega un
//      término de GIRO (mismo signo en las 3 ruedas, EXACTAMENTE como girar()) proporcional
//      al error → cancela la curvatura y la patada sale recta. El término está CLAMPEADO
//      (MIX_KICK_CORR_MAX) y el empuje siempre lo domina: aunque el signo del Kp esté mal,
//      no descontrola (a lo sumo curva, no gira en el lugar) → se corrige con el signo en banco.
//      Si el OTOS NO está fresco/sano → corr=0 (patada recta "a ciegas", como el 2025 pero rápida).
//
// IMPORTANTE: ahora se ESCRIBEN los motores en CADA tick (no solo en el borde del intervalo),
// porque la corrección de rumbo cambia entre intervalos aunque la velocidad de rampa no. Es
// barato (3 analogWrite + 6 digitalWrite) y necesario para que el heading-hold reaccione.
//
// El RESETEO entre patadas lo da parar() (lo llama la FSM en cada pausa/transición): cierra
// s_kick_active → la próxima avanzar_patear() re-ancla el rumbo y rampa desde el inicio.
// ============================================================
void avanzar_patear() {
    const unsigned long now = millis();

    // (Re)entrada a una patada nueva (flanco false→true): empezar SOBRE el piso, anclar el
    // rumbo objetivo del OTOS y fijar si el OTOS es confiable AHORA (se congela para toda
    // esta patada: si el enlace se cae a mitad igual dejamos de corregir por el chequeo de
    // frescura de abajo).
    if (!s_kick_active) {
        s_kick_active  = true;
        s_kick_vel     = MIX_KICK_VEL_START;
        s_kick_prev_ms = now;
        s_kick_heading_target = g_io.otos_heading_deg;   // dirección al iniciar la patada
        s_kick_use_otos = (g_io.otos_confidence > 0) &&
                          (g_io.t_last_otos_pose_ms > 0) &&
                          ((now - g_io.t_last_otos_pose_ms) < MIX_KICK_OTOS_FRESH_MS);
    } 
 
    // Rampa RÁPIDA hasta el PWM final (no bloqueante, con millis()).
    if (now - s_kick_prev_ms >= (unsigned long)MIX_KICK_INTERVALO_MS) {
        s_kick_prev_ms = now;
        if (s_kick_vel < MIX_KICK_VEL_FINAL) {
            s_kick_vel += MIX_KICK_PASO;
            if (s_kick_vel > MIX_KICK_VEL_FINAL) s_kick_vel = MIX_KICK_VEL_FINAL;
        }
    }

    // Corrección de rumbo con el OTOS (heading-hold). Se corrige SOLO si el OTOS está:
    //   (a) fresco AL ENTRAR (s_kick_use_otos, congelado en el flanco), Y
    //   (b) fresco AHORA (la pose no envejeció a mitad de la patada), Y
    //   (c) SANO AHORA (g_io.otos_confidence>0). El tercer chequeo (red-team) tapa el caso "OTOS
    //       vivo pero ciego": DOWN puede seguir mandando Pose2D a 100 Hz (timestamp fresco) con
    //       confidence=0 (ningún OTOS sano) → sin (c) corregiríamos contra un heading basura. Con
    //       (c), corr=0 → patada RECTA A CIEGAS (degradación segura). (c) re-activa si confidence sube.
    float err = 0.0f;
    int corr = 0;
    if (s_kick_use_otos &&
        g_io.otos_confidence > 0 &&
        (now - g_io.t_last_otos_pose_ms) < MIX_KICK_OTOS_FRESH_MS) {
        err = wrap180(g_io.otos_heading_deg - s_kick_heading_target);
        int c = static_cast<int>(MIX_KICK_HEADING_KP * err);
        if (c >  MIX_KICK_CORR_MAX) c =  MIX_KICK_CORR_MAX;
        if (c < -MIX_KICK_CORR_MAX) c = -MIX_KICK_CORR_MAX;
        corr = c;
    }
    g_io.kick_err_deg = err;   // diagnóstico para el debug por USB (titular en banco)
    g_io.kick_corr    = corr;

    // MEZCLA: empuje (M1=+vel, M2=-vel, M3=0) + giro PURO (la MISMA corr en las 3 = el término
    // ω·R de la cinemática, idéntico para las 3 porque están al mismo radio; es lo que hace
    // girar()). + FEEDFORWARD de balance MIX_KICK_FWD_TRIM (default 0): trasvasa PWM de M1 a M2
    // por si queda una deriva CONSTANTE residual tras el escalado (sesgo fijo de hardware). El
    // veer "solo al patear" suele ser la SATURACIÓN de abajo, no un motor flojo → probar primero
    // con TRIM=0; subirlo solo si persiste.
    int w[3];
    w[0] = +(s_kick_vel - MIX_KICK_FWD_TRIM) + corr;   // M1 delantera IZQ
    w[1] = -(s_kick_vel + MIX_KICK_FWD_TRIM) + corr;   // M2 delantera DER
    w[2] =                              0    + corr;    // M3 trasera (solo gira para corregir)

    // SATURACIÓN QUE PRESERVA LA DIRECCIÓN (el arreglo del veer "solo al patear"). Si dejáramos
    // que mix_set_motor recorte CADA rueda a ±255 por separado, a PWM alto (patada=240) una
    // delantera satura y la otra no → el vector se TUERCE e inyecta strafe lateral parásito (la
    // patada sale de costado). En avanzar() (PWM 100) no satura → por eso NO derivaba ahí. En vez
    // del clamp por-rueda, si el pico pasa MIX_MAX_PWM escalamos las 3 por el MISMO factor →
    // la mezcla preserva su dirección. Mismo patrón que saturate_wheels() (src/shared/kinematics.cpp).
    int peak = 0;
    for (int i = 0; i < 3; ++i) { int a = (w[i] < 0) ? -w[i] : w[i]; if (a > peak) peak = a; }
    if (peak > MIX_MAX_PWM) {
        for (int i = 0; i < 3; ++i)
            w[i] = (int)((long)w[i] * MIX_MAX_PWM / peak);  // entero, preserva signo y proporciones
    }

    // (OPCIONAL, default OFF) Piso de la rueda TRASERA. M3 solo hace giro; por debajo de su piso
    // físico (~107) NO gira (zumba) → el giro queda solo en el diferencial M1/M2. Si en banco el
    // giro es flojo, MIX_KICK_REAR_FLOOR>0 eleva M3 a su piso (conserva signo), DESPUÉS del
    // escalado. ⚠️ A ganancias chicas vuelve M3 bang-bang → puede meter zigzag; por eso off.
    if (MIX_KICK_REAR_FLOOR > 0) {
        int a3 = (w[2] < 0) ? -w[2] : w[2];
        if (a3 > 0 && a3 < MIX_KICK_REAR_FLOOR)
            w[2] = (w[2] < 0) ? -MIX_KICK_REAR_FLOOR : MIX_KICK_REAR_FLOOR;
    }

    // ESCRIBIR. El vector ya cabe en ±255; el clamp por-rueda de mix_set_motor queda solo como
    // red de seguridad (nunca se activa). Se reescribe en CADA tick (la corr cambia entre rampas).
    mix_set_motor(0, w[0]);
    mix_set_motor(1, w[1]);
    mix_set_motor(2, w[2]);
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
    mix_set_motor(0, MIX_KICKOFF_ARC_PWD*120);   // M1 delantera IZQ
    mix_set_motor(1, MIX_KICKOFF_ARC_PWD*(-80));   // M2 delantera DER
    mix_set_motor(2, MIX_KICKOFF_ARC_PWD*0);   // M3 trasera
}

// ============================================================
// APUNTAR a la pelota PROPORCIONAL. Gira en el lugar hacia la pelota con potencia PROPORCIONAL al
// ángulo: ángulo GRANDE → más PWM (gira rápido); ángulo CHICO → menos PWM (suave, no se pasa).
// Lee g_io.angulo_pelota_deg (0 = pelota al frente, + = a la DERECHA). Giro PURO (las 3 ruedas al
// mismo signo, como girar()). El signo del giro sale del LADO de la pelota (mismo mapeo que el
// apuntar_pelota_motores del 2025: ang>0 → +, ang<0 → -).
//   pwm = clamp(|ang| * AP_KP, AP_PWM_MIN, AP_PWM_MAX)
// AP_PWM_MIN es el PISO para vencer la zona muerta del motor (por debajo NO gira, solo zumba);
// AP_PWM_MAX es el techo. Subí AP_KP para que reaccione más fuerte a ángulos chicos.
// ⚠️ NO TESTEADO EN HW: confirmá en banco el sentido (si apunta al lado contrario, invertí el signo).
// ============================================================
static constexpr float AP_KP      = 2.5f;   // PWM por GRADO de ángulo de la pelota
static constexpr int   AP_PWM_MIN = 90;     // piso (vence la zona muerta; el motor no mueve por debajo)
static constexpr int   AP_PWM_MAX = 180;    // techo del giro
void apuntar_pelota_proporcional() {
    const float ang = g_io.angulo_pelota_deg;         // 0=frente, +=derecha
    const float a   = (ang < 0.0f) ? -ang : ang;      // |ángulo|
    int pwm = (int)(a * AP_KP);                       // proporcional al ángulo
    if (pwm > AP_PWM_MAX) pwm = AP_PWM_MAX;
    if (pwm < AP_PWM_MIN) pwm = AP_PWM_MIN;           // piso para que efectivamente gire
    const int w = (ang > 0.0f) ? +pwm : -pwm;         // signo = de qué lado está la pelota
    mix_set_motor(0, w);
    mix_set_motor(1, w);
    mix_set_motor(2, w);
}

}  // namespace mix
}  // namespace iitasoccer
