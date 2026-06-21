// amix_fsm.cpp — Máquina de estados del ARQUERO 2025 (port fiel) para arqueromix.
//
// PORT del ciclo ARQUERO de definitivo-arquero_6-9-2026 (FIEL §2). Sólo cambian las
// fuentes (g_aio en vez de sensores locales), las salidas (primitivas amix_motors) y
// se AGREGAN: el gate del árbitro y un timeout de seguridad al retroceso.
//
// MAPEOS QUE REQUIRIERON DECISIÓN (marcados <RE-TUNE> / <RE-VERIFY SIGN>):
//   - "Xp <= tolerancia_cercania(140)" (cerca)  → ball_y_mm <= AMIX_TOL_CERCANIA_MM (mm). <RE-TUNE>
//   - "abs(Yp) <= 3" (centrada lateral)         → |ball_x_mm| <= AMIX_TOL_CENTRADO_MM.  <RE-TUNE>
//   - "abs(Yp) >= 5" (desviada)                 → |ball_x_mm| >= AMIX_TOL_DESVIO_MM.     <RE-TUNE>
//   - "Yp < 0 → derecha, Yp >= 0 → izquierda"   → ball_x_mm > 0 → derecha. <RE-VERIFY SIGN>
//   - "s1>=blanco1 || s2>=blanco2" (borde) y "s1||s2||s3" (volví a la línea)
//        → ambos = line_present (DOWN agrega los 32 sensores en una señal). <SIMPLIFICACIÓN>
//
// ⚠️ NO TESTEADO EN HARDWARE.

#include <Arduino.h>
#include <math.h>

#include "amix_config.h"
#include "amix_io.h"
#include "amix_motors.h"
#include "amix_fsm.h"

namespace iitasoccer {
namespace arqmix {

// Estado + timers (eran globales sueltas en el 2025).
static Estado estado = Estado::impulso_inicial;
static unsigned long millis_inicio_estado = 0;
// `pd` (factor proporcional): el 2025 lo seteaba en moverce_* y lo usaba adproporcional/
// aiproporcional en el MISMO tick (con valor del tick anterior). Se replica el lag.
static float pd = AMIX_PD_BASE;

// ---- Helpers de lectura (reemplazan las globales seriales/analógicas 2025) ----

// Línea presente (== OR de los 3 sensores 2025; el DOWN ya agrega los 32).
static inline bool linea() {
    return g_aio.line_present && (g_aio.line_depth >= AMIX_LINE_DEPTH_TRIGGER);
}
// Pelota cerca Y centrada → dispara el despeje. (2025: Xp<=140 && abs(Yp)<=3) <RE-TUNE>
static inline bool ball_cerca_centrada() {
    return g_aio.ball_visible &&
           (g_aio.ball_y_mm <= AMIX_TOL_CERCANIA_MM) &&
           (fabsf(g_aio.ball_x_mm) <= AMIX_TOL_CENTRADO_MM);
}
// Pelota desviada lateralmente. (2025: abs(Yp)>=5) <RE-TUNE>
static inline bool ball_desviada() {
    return g_aio.ball_visible && (fabsf(g_aio.ball_x_mm) >= AMIX_TOL_DESVIO_MM);
}
// ¿La pelota está a la DERECHA? (2025: Yp<0→der). ball_x_mm>0 = derecha. <RE-VERIFY SIGN>
static inline bool ball_a_la_derecha() {
    return g_aio.ball_x_mm > 0.0f;
}

void amix_fsm_init() {
    estado = Estado::impulso_inicial;
    millis_inicio_estado = millis();
    pd = AMIX_PD_BASE;
}

void amix_fsm_tick() {
    // --- AGREGADO 2026: GO/STOP del árbitro RCJ (el 2025 arrancaba solo). ---
    if (!g_aio.match_running) {
        parar();
        return;
    }

    const float error = g_aio.heading_error_deg;   // == 'error' 2025 (ya wrapeado)
    const bool  haypelota = g_aio.ball_visible;     // == 'haypelota' 2025

    switch (estado) {
        // ----------------------------------------------------
        case Estado::impulso_inicial:               // L1016-1028
            impulso_inicial_mov();                  // strafe fuerte (M1=M2=90, M3=153)
            if (millis() - millis_inicio_estado >= AMIX_T_IMP_INICIAL) {  // 40 ms
                millis_inicio_estado = millis();
                estado = Estado::moverce_derecha;
            }
            break;

        // ----------------------------------------------------
        case Estado::moverce_derecha:               // L1030-1076
            adproporcional(pd, error);              // strafe derecha + corrección rumbo
            if (haypelota) {
                if (ball_cerca_centrada()) {        // Xp<=140 && abs(Yp)<=3 → PATEA
                    parar();
                    millis_inicio_estado = millis();
                    estado = Estado::PATEANDO_pausa_inicial;
                } else if (ball_desviada()) {       // abs(Yp)>=5 → elige lado
                    pd = AMIX_PD_BALL;              // 1.5
                    estado = ball_a_la_derecha() ? Estado::moverce_derecha
                                                  : Estado::moverce_izquierda;
                } else {
                    parar();                         // banda muerta (3<abs<5 o centrada lejos)
                }
            } else {
                pd = AMIX_PD_BASE;                   // sin pelota: patrulla base
            }
            if (linea()) {                           // borde → rebote al lado opuesto
                parar();
                millis_inicio_estado = millis();
                estado = Estado::impulso_izquierda;
            }
            break;

        // ----------------------------------------------------
        case Estado::moverce_izquierda:             // L1078-1124 (espejo)
            aiproporcional(pd, error);
            if (haypelota) {
                if (ball_cerca_centrada()) {
                    parar();
                    millis_inicio_estado = millis();
                    estado = Estado::PATEANDO_pausa_inicial;
                } else if (ball_desviada()) {
                    pd = AMIX_PD_BALL;
                    estado = ball_a_la_derecha() ? Estado::moverce_derecha
                                                  : Estado::moverce_izquierda;
                } else {
                    parar();
                }
            } else {
                pd = AMIX_PD_BASE;
            }
            if (linea()) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::impulso_derecha;
            }
            break;

        // ----------------------------------------------------
        case Estado::impulso_derecha:               // L1126-1136 anti-traba
            adproporcional(pd, error);
            if (millis() - millis_inicio_estado >= AMIX_T_IMP_LATERAL) {  // 350 ms
                millis_inicio_estado = millis();
                estado = Estado::moverce_derecha;
            }
            break;

        // ----------------------------------------------------
        case Estado::impulso_izquierda:             // L1138-1148 anti-traba (espejo)
            aiproporcional(pd, error);
            if (millis() - millis_inicio_estado >= AMIX_T_IMP_LATERAL) {  // 350 ms
                millis_inicio_estado = millis();
                estado = Estado::moverce_izquierda;
            }
            break;

        // --- SECUENCIA DE DESPEJE ---
        // ----------------------------------------------------
        case Estado::PATEANDO_pausa_inicial:        // L1151-1160
            parar();                                 // deja pasar la inercia lateral
            if (millis() - millis_inicio_estado >= AMIX_T_PAT_PAUSA_INI) {  // 200 ms
                millis_inicio_estado = millis();
                estado = Estado::PATEANDO_adelante;
            }
            break;

        // ----------------------------------------------------
        case Estado::PATEANDO_adelante:             // L1162-1172
            avanzar_patear();                        // golpe de avance (M1=250, M2=150)
            if (millis() - millis_inicio_estado >= AMIX_T_PAT_ADELANTE) {  // 450 ms
                parar();
                millis_inicio_estado = millis();
                estado = Estado::PATEANDO_pausa;
            }
            break;

        // ----------------------------------------------------
        case Estado::PATEANDO_pausa:                // L1174-1182
            parar();
            if (millis() - millis_inicio_estado >= AMIX_T_PAT_PAUSA) {  // 1000 ms
                millis_inicio_estado = millis();
                estado = Estado::PATEANDO_atras;
            }
            break;

        // ----------------------------------------------------
        case Estado::PATEANDO_atras:                // L1184-1195 (retroceso recto)
            patear_atras();                          // M1=-150, M2=+150, M3=0
            // 2025: SIN timeout, sale sólo al ver blanco. AGREGADO 2026: timeout de
            // seguridad para no colgarse si nunca llega a la línea. <MEJORA 2026>
            if (linea() ||
                (millis() - millis_inicio_estado >= AMIX_T_ATRAS_SAFETY)) {  // 4000 ms safety
                parar();
                millis_inicio_estado = millis();
                estado = Estado::avanzar_despues_de_patear;
            }
            break;

        // ----------------------------------------------------
        case Estado::avanzar_despues_de_patear:     // L1197-1205
            avanzar();
            if (millis() - millis_inicio_estado >= AMIX_T_AVANCE_POST) {  // 1000 ms
                millis_inicio_estado = millis();
                estado = Estado::moverce_derecha;    // cierra el ciclo → patrulla
            }
            break;
    }
}

}  // namespace arqmix
}  // namespace iitasoccer
