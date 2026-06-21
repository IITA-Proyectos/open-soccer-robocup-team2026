// amix_fsm.cpp — Máquina de estados del ARQUERO 2025 (port fiel) para arqueromix.
//
// PORT del ciclo ARQUERO de definitivo-arquero_6-9-2026 (FIEL §2). Sólo cambian las
// fuentes (g_aio en vez de sensores locales), las salidas (primitivas amix_motors) y
// se AGREGAN: el gate del árbitro y un timeout de seguridad al retroceso.
//
// MAPEOS QUE REQUIRIERON DECISIÓN (marcados <RE-TUNE> / <RE-VERIFY SIGN>):
//   FIX 2026-06-21 (banco Virginia: la cámara veía la pelota pero el robot NO se movía):
//   el seguimiento de la pelota pasa a ser POR ÁNGULO (atan2), como centralmix usa la cámara,
//   en vez de mm crudos (que caían en banda muerta para la escala sin calibrar → parar()).
//   - "Xp<=140 && abs(Yp)<=3" (cerca+centrada → patea) → dist_pelota_mm()<=CERCANIA_MM &&
//        |angulo_pelota_deg|<=KICK_DEG (ángulo + distancia euclídea). <RE-TUNE distancia>
//   - "abs(Yp)>=5" (desviada → seguir) → |angulo_pelota_deg| > CENTRADO_DEG → strafe al lado.
//   - lado: "Yp<0→der" → angulo_pelota_deg>0 → derecha. <RE-VERIFY SIGN>
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
static Estado estado = Estado::inicio_retroceder;
static unsigned long millis_inicio_estado = 0;
// `pd` (factor proporcional): el 2025 lo seteaba en moverce_* y lo usaba adproporcional/
// aiproporcional en el MISMO tick (con valor del tick anterior). Se replica el lag.
static float pd = AMIX_PD_BASE;

// Flanco del árbitro: detecta STOP→GO para RE-HACER el homing del área chica en CADA GO
// (banco Virginia 2026-06-21: antes el homing corría una sola vez —el primer GO— porque el FSM
// no se reiniciaba entre STOP y GO sin apagar la batería).
static bool s_was_running = false;

// ---- Helpers de lectura (reemplazan las globales seriales/analógicas 2025) ----

// Línea presente (== OR de los 3 sensores 2025; el DOWN ya agrega los 32).
static inline bool linea() {
    return g_aio.line_present && (g_aio.line_depth >= AMIX_LINE_DEPTH_TRIGGER);
}
// Distancia euclídea robot→pelota (mm) — como dist_pelota_mm() de centralmix.
static inline float dist_pelota_mm() {
    return sqrtf(g_aio.ball_x_mm * g_aio.ball_x_mm + g_aio.ball_y_mm * g_aio.ball_y_mm);
}
// ¿Pelota cerca Y razonablemente al frente → DESPEJAR? (por ÁNGULO + distancia, NO mm crudos).
// FIX 2026-06-21: el 2025 usaba Xp<=140 && abs(Yp)<=3 en píxeles; con mm crudos caía en banda
// muerta y el robot se congelaba. Ahora: distancia euclídea + ángulo de despeje. <RE-TUNE distancia>
static inline bool ball_para_despejar() {
    return g_aio.ball_visible &&
           (dist_pelota_mm() <= AMIX_TOL_CERCANIA_MM) &&
           (fabsf(g_aio.angulo_pelota_deg) <= AMIX_TOL_KICK_DEG);
}
// ¿Pelota ALINEADA al frente (banda muerta ANGULAR angosta → mantener posición)?
static inline bool ball_alineada() {
    return fabsf(g_aio.angulo_pelota_deg) <= AMIX_TOL_CENTRADO_DEG;
}
// ¿La pelota está a la DERECHA? por el SIGNO del ÁNGULO (ang>0 = derecha). <RE-VERIFY SIGN>
static inline bool ball_a_la_derecha() {
    return g_aio.angulo_pelota_deg > 0.0f;
}

void amix_fsm_init() {
    estado = Estado::inicio_retroceder;   // homing al área chica antes de patrullar
    millis_inicio_estado = millis();
    pd = AMIX_PD_BASE;
}

void amix_fsm_tick() {
    // --- Árbitro RCJ + RE-HOMING en cada GO (banco Virginia 2026-06-21) ---
    if (!g_aio.match_running) {
        parar();
        s_was_running = false;   // quedó parado → el próximo GO re-arranca el homing
        return;
    }
    // Flanco STOP→GO (y el primer GO): reiniciar el FSM al homing del área chica, SIEMPRE.
    if (!s_was_running) {
        s_was_running = true;
        estado = Estado::inicio_retroceder;
        millis_inicio_estado = millis();
        pd = AMIX_PD_BASE;
    }

    const float error = g_aio.heading_error_deg;   // == 'error' 2025 (ya wrapeado)
    const bool  haypelota = g_aio.ball_visible;     // == 'haypelota' 2025

    switch (estado) {
        // ----------------------------------------------------
        // --- INICIO: homing al área chica (banco Virginia 2026-06-21) ---
        case Estado::inicio_retroceder:             // ir HACIA ATRÁS hasta detectar la línea del área
            retroceder_inicio();                     // retroceso dedicado (PWM propio, sentido flippable)
            // Sale al ver la línea (blanco del área chica). Safety: si NUNCA la ve, avanza igual
            // tras AMIX_T_INICIO_RETRO_SAFETY para no quedarse retrocediendo contra la pared.
            if (linea() || (millis() - millis_inicio_estado >= AMIX_T_INICIO_RETRO_SAFETY)) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::inicio_avanzar;
            }
            break;

        case Estado::inicio_avanzar:                // avanzar un poco SIN leer los sensores
            avanzar();                               // a ciegas: NO se chequea linea() acá a propósito
            if (millis() - millis_inicio_estado >= AMIX_T_INICIO_AVANCE) {  // ~400 ms
                millis_inicio_estado = millis();
                estado = Estado::moverce_derecha;    // recién ahora empieza a patrullar
            }
            break;

        // ----------------------------------------------------
        case Estado::moverce_derecha:               // L1030-1076 (FIX cámara por ÁNGULO 2026-06-21)
            adproporcional(pd, error);              // strafe derecha + corrección rumbo
            if (haypelota) {
                if (ball_para_despejar()) {         // cerca + al frente → DESPEJA
                    parar();
                    millis_inicio_estado = millis();
                    estado = Estado::PATEANDO_pausa_inicial;
                } else if (!ball_alineada()) {      // off-center → SIGUE la pelota (por ángulo)
                    pd = AMIX_PD_BALL;              // 1.5
                    estado = ball_a_la_derecha() ? Estado::moverce_derecha
                                                  : Estado::moverce_izquierda;
                } else {
                    parar();                         // alineada y lejos → mantener posición (banda angosta)
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
        case Estado::moverce_izquierda:             // L1078-1124 (espejo, FIX cámara por ÁNGULO)
            aiproporcional(pd, error);
            if (haypelota) {
                if (ball_para_despejar()) {
                    parar();
                    millis_inicio_estado = millis();
                    estado = Estado::PATEANDO_pausa_inicial;
                } else if (!ball_alineada()) {
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
