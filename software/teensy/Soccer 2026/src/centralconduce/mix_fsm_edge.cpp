// mix_fsm_edge.cpp — FSM "CONDUCIR la pelota al arco" (centralconduce). Ver mix_fsm_edge.h.
//
// Se activa con -DMIX_ATTACK_EDGE. NO es el rodeo de Edge: esta versión simplemente busca la
// pelota, la CENTRA al frente, la LLEVA al arco (o al heading 0 si no ve el arco) manteniéndola
// adelante, y cuando está a <40 cm del arco APUNTA y PATEA (empuje por inercia).
//
// FLUJO:  KICKOFF → BUSCAR → CENTRAR → CONDUCIR → (a 40cm) APUNTAR_ARCO → PATEAR → RETROCEDER → BUSCAR
//         (+ escape de línea DETECTA_LINEA_1/2/3 en cualquier estado que se mueve)
//
// ⚠️ NO TESTEADO EN HARDWARE. Compila != anda: el signo del giro (MIX_EDGE_FACE_KP), el sentido
// de mix_mover_vector y las distancias en cm/mm se titulan en banco.

#include <Arduino.h>
#include <math.h>

#include "mix_config.h"
#include "mix_io.h"
#include "mix_motors.h"
#include "mix_fsm_edge.h"

namespace iitasoccer {
namespace mix {

// ============================================================
// Estados.
// ============================================================
enum class EstadoEdge {
    KICKOFF,
    BUSCAR,
    CENTRAR,        // girar en el lugar hasta tener la pelota AL FRENTE (centrada)
    CONDUCIR,       // llevar la pelota: avanzar hacia ella, y al tenerla cerca escoltarla al arco
    APUNTAR_ARCO,   // a <40 cm del arco: girar para apuntar al arco
    PATEAR,         // empuje a fondo por inercia (gol)
    RETROCEDER,     // despegarse corto post-empuje
    DETECTA_LINEA_1,
    DETECTA_LINEA_2,
    DETECTA_LINEA_3,
};

static EstadoEdge    estado               = EstadoEdge::KICKOFF;
static unsigned long millis_inicio_estado = 0;

// ============================================================
// Helpers de línea (reconstrucción de los 3 "sensores" del 2025 desde DOWN; igual a mix_fsm.cpp).
// Sectores ±60° → RE-TUNEAR en banco.
// ============================================================
static inline bool linea_presente() {
    return g_io.line_present && (g_io.line_depth >= MIX_LINE_DEPTH_TRIGGER);
}
static inline bool linea_s1() { return linea_presente() && (g_io.line_angle_deg < -60.0f); } // izq
static inline bool linea_s2() { return linea_presente() && (fabsf(g_io.line_angle_deg) <= 60.0f); } // frente
static inline bool linea_s3() { return linea_presente() && (g_io.line_angle_deg >  60.0f); } // der

// ============================================================
// Giro para ORIENTAR el frente del robot hacia un ángulo (en el marco del robot: 0 = frente).
// target_rel_deg = a dónde quiero mirar (ángulo a la pelota, al arco, o a la dir. inicial).
// + / − = un sentido u otro; el SIGNO es perilla de banco (MIX_EDGE_FACE_KP).
// ============================================================
static int omega_hacia(float target_rel_deg) {
    float corr = MIX_EDGE_FACE_KP * target_rel_deg;
    if (corr >  (float)MIX_EDGE_OMEGA_MAX) corr =  (float)MIX_EDGE_OMEGA_MAX;
    if (corr < -(float)MIX_EDGE_OMEGA_MAX) corr = -(float)MIX_EDGE_OMEGA_MAX;
    return (int)corr;
}

// Distancia robot→pelota (cm, dato crudo de cámara).
static inline float dist_pelota_cm() {
    return sqrtf(g_io.ball_x_cm * g_io.ball_x_cm + g_io.ball_y_cm * g_io.ball_y_cm);
}
// Distancia robot→arco rival (cm). g_io.goal_opp_dist viene en mm → /10.
static inline float dist_arco_cm() {
    return g_io.goal_opp_dist / 10.0f;
}

// ============================================================
// init.
// ============================================================
void mix_fsm_edge_init() {
    estado = EstadoEdge::KICKOFF;
    millis_inicio_estado = millis();
}

// ============================================================
// tick.
// ============================================================
void mix_fsm_edge_tick() {
    // ---- Árbitro RCJ + KICKOFF una vez por encendido (mismo patrón probado de mix_fsm.cpp) ----
    static bool prev_go       = false;
    static bool seen_stop     = false;
    static bool kickoff_done  = false;
    static bool prev_top_link = false;

    if (g_io.top_link_fresh && !prev_top_link) {   // power-cycle detectado por el TOP → arranque
        estado = EstadoEdge::KICKOFF;
        millis_inicio_estado = millis();
        kickoff_done = false;
        prev_go      = false;
    }
    prev_top_link = g_io.top_link_fresh;

    if (!g_io.match_running) {   // STOP del árbitro
        seen_stop = true;
        prev_go   = false;
        parar();
        return;
    }
    if (!seen_stop) {            // GO sin STOP previo (boot): no moverse aún
        parar();
        return;
    }
    const bool go_edge = !prev_go;
    prev_go = true;
    if (go_edge && !kickoff_done) {
        estado = EstadoEdge::KICKOFF;
        millis_inicio_estado = millis();
        kickoff_done = true;
    }

    // ---- Lecturas del tick ----
    const bool  haypelota    = g_io.ball_visible;
    const float ball_ang     = g_io.angulo_pelota_deg;          // 0=frente, +=derecha
    const bool  arco_visible = g_io.goal_opp_visible;
    const float arco_ang     = g_io.goal_opp_angle;             // 0=frente, +=derecha
    const float heading_err  = g_io.heading_error_deg;          // rumbo - rumbo inicial

    switch (estado) {

        // ----------------------------------------------------
        case EstadoEdge::KICKOFF:
            if (haypelota) { estado = EstadoEdge::CENTRAR; millis_inicio_estado = millis(); break; }
            if (linea_s1()) { estado = EstadoEdge::DETECTA_LINEA_1; millis_inicio_estado = millis(); break; }
            if (linea_s2()) { estado = EstadoEdge::DETECTA_LINEA_2; millis_inicio_estado = millis(); break; }
            if (linea_s3()) { estado = EstadoEdge::DETECTA_LINEA_3; millis_inicio_estado = millis(); break; }
            kickoff_medialuna();
            if (millis() - millis_inicio_estado >= (unsigned long)MIX_KICKOFF_ARC_MS) {
                parar();
                estado = EstadoEdge::BUSCAR;
                millis_inicio_estado = millis();
            }
            break;

        // ----------------------------------------------------
        case EstadoEdge::BUSCAR:
            if (haypelota) {
                estado = EstadoEdge::CENTRAR;
                millis_inicio_estado = millis();
            } else {
                girar();   // gira en el lugar buscando (sentido a confirmar en banco)
            }
            if (linea_s1()) { estado = EstadoEdge::DETECTA_LINEA_1; millis_inicio_estado = millis(); }
            if (linea_s2()) { estado = EstadoEdge::DETECTA_LINEA_2; millis_inicio_estado = millis(); }
            if (linea_s3()) { estado = EstadoEdge::DETECTA_LINEA_3; millis_inicio_estado = millis(); }
            break;

        // ----------------------------------------------------
        // CENTRAR — girar EN EL LUGAR hasta tener la pelota al frente (para que al avanzar no se
        // le escape). No traslada (así no la empuja descentrada).
        // ----------------------------------------------------
        case EstadoEdge::CENTRAR:
            mix_mover_vector(0, 0, omega_hacia(ball_ang));   // giro puro hacia la pelota

            if (fabsf(ball_ang) < MIX_CONDUCE_CENTER_TOL_DEG) {   // ya está al frente → llevarla
                estado = EstadoEdge::CONDUCIR;
                millis_inicio_estado = millis();
            }
            if (millis() - g_io.t_last_ball_seen_ms >= MIX_EDGE_BALL_LOST_MS) {
                estado = EstadoEdge::BUSCAR;
                millis_inicio_estado = millis();
            }
            if (linea_s1()) { estado = EstadoEdge::DETECTA_LINEA_1; millis_inicio_estado = millis(); }
            if (linea_s2()) { estado = EstadoEdge::DETECTA_LINEA_2; millis_inicio_estado = millis(); }
            if (linea_s3()) { estado = EstadoEdge::DETECTA_LINEA_3; millis_inicio_estado = millis(); }
            break;

        // ----------------------------------------------------
        // CONDUCIR — llevar la pelota. Avanza HACIA la pelota (la mantiene al frente). Mientras
        // todavía va a buscarla (lejos) gira para CENTRARLA; cuando ya la tiene cerca, gira para
        // ESCOLTARLA al arco (o al heading 0 si no ve el arco). NO se posiciona detrás: solo lleva.
        // ----------------------------------------------------
        case EstadoEdge::CONDUCIR: {
            // ¿a dónde oriento el frente? Cerca de la pelota → al arco (o heading 0). Lejos → a la pelota.
            float target_rel;
            if (dist_pelota_cm() < MIX_CONDUCE_HAVE_DIST_CM) {
                target_rel = arco_visible ? arco_ang : (-heading_err);   // escoltar al arco / heading 0
            } else {
                target_rel = ball_ang;                                   // todavía centrando la pelota
            }
            mix_mover_vector(ball_ang, MIX_EDGE_SPEED, omega_hacia(target_rel));

            // a <40 cm del arco → apuntar y patear
            if (arco_visible && dist_arco_cm() < MIX_CONDUCE_KICK_GOAL_DIST_CM) {
                estado = EstadoEdge::APUNTAR_ARCO;
                millis_inicio_estado = millis();
            }
            // se fue al costado → recentrar
            if (fabsf(ball_ang) > MIX_CONDUCE_RECAPTURE_TOL_DEG) {
                estado = EstadoEdge::CENTRAR;
                millis_inicio_estado = millis();
            }
            // pelota perdida → buscar
            if (millis() - g_io.t_last_ball_seen_ms >= MIX_EDGE_BALL_LOST_MS) {
                estado = EstadoEdge::BUSCAR;
                millis_inicio_estado = millis();
            }
            if (linea_s1()) { estado = EstadoEdge::DETECTA_LINEA_1; millis_inicio_estado = millis(); }
            if (linea_s2()) { estado = EstadoEdge::DETECTA_LINEA_2; millis_inicio_estado = millis(); }
            if (linea_s3()) { estado = EstadoEdge::DETECTA_LINEA_3; millis_inicio_estado = millis(); }
            break;
        }

        // ----------------------------------------------------
        // APUNTAR_ARCO — a <40 cm del arco: girar EN EL LUGAR para apuntar al arco antes de patear.
        // ----------------------------------------------------
        case EstadoEdge::APUNTAR_ARCO:
            mix_mover_vector(0, 0, omega_hacia(arco_ang));   // giro puro hacia el arco

            // apuntado (o ya no veo el arco, o timeout de seguridad) → patear
            if (!arco_visible ||
                fabsf(arco_ang) < MIX_CONDUCE_AIM_TOL_DEG ||
                (millis() - millis_inicio_estado >= 1200)) {
                estado = EstadoEdge::PATEAR;
                millis_inicio_estado = millis();
            }
            if (linea_s1()) { estado = EstadoEdge::DETECTA_LINEA_1; millis_inicio_estado = millis(); }
            if (linea_s2()) { estado = EstadoEdge::DETECTA_LINEA_2; millis_inicio_estado = millis(); }
            if (linea_s3()) { estado = EstadoEdge::DETECTA_LINEA_3; millis_inicio_estado = millis(); }
            break;

        // ----------------------------------------------------
        // PATEAR — empuje recto a fondo (avanzar_patear: rampa fuerte + heading-hold del OTOS).
        // ----------------------------------------------------
        case EstadoEdge::PATEAR:
            avanzar_patear();
            if (millis() - millis_inicio_estado >= MIX_EDGE_PUSH_MS) {
                parar();
                estado = EstadoEdge::RETROCEDER;
                millis_inicio_estado = millis();
            }
            if (linea_s1()) { estado = EstadoEdge::DETECTA_LINEA_1; millis_inicio_estado = millis(); }
            if (linea_s2()) { estado = EstadoEdge::DETECTA_LINEA_2; millis_inicio_estado = millis(); }
            if (linea_s3()) { estado = EstadoEdge::DETECTA_LINEA_3; millis_inicio_estado = millis(); }
            break;

        // ----------------------------------------------------
        case EstadoEdge::RETROCEDER:
            retroceder_patear();
            if (millis() - millis_inicio_estado >= MIX_EDGE_BACK_MS) {
                parar();
                estado = EstadoEdge::BUSCAR;
                millis_inicio_estado = millis();
            }
            break;

        // ----------------------------------------------------
        // Escape de línea (retroceder1/2/3 bench-tuneados por Elías). Tras 400 ms → BUSCAR.
        // ----------------------------------------------------
        case EstadoEdge::DETECTA_LINEA_1:
            retroceder1();
            if (millis() - millis_inicio_estado >= 400) {
                parar(); millis_inicio_estado = millis(); estado = EstadoEdge::BUSCAR;
            }
            break;
        case EstadoEdge::DETECTA_LINEA_2:
            retroceder2();
            if (millis() - millis_inicio_estado >= 400) {
                parar(); millis_inicio_estado = millis(); estado = EstadoEdge::BUSCAR;
            }
            break;
        case EstadoEdge::DETECTA_LINEA_3:
            retroceder3();
            if (millis() - millis_inicio_estado >= 400) {
                parar(); millis_inicio_estado = millis(); estado = EstadoEdge::BUSCAR;
            }
            break;
    }
}

// ============================================================
// Nombre del estado (debug por USB).
// ============================================================
const char* mix_fsm_edge_estado_nombre() {
    switch (estado) {
        case EstadoEdge::KICKOFF:         return "KICKOFF";
        case EstadoEdge::BUSCAR:          return "BUSCAR";
        case EstadoEdge::CENTRAR:         return "CENTRAR";
        case EstadoEdge::CONDUCIR:        return "CONDUCIR";
        case EstadoEdge::APUNTAR_ARCO:    return "APUNTAR_ARCO";
        case EstadoEdge::PATEAR:          return "PATEAR";
        case EstadoEdge::RETROCEDER:      return "RETROCEDER";
        case EstadoEdge::DETECTA_LINEA_1: return "DETECTA_LINEA_1";
        case EstadoEdge::DETECTA_LINEA_2: return "DETECTA_LINEA_2";
        case EstadoEdge::DETECTA_LINEA_3: return "DETECTA_LINEA_3";
    }
    return "?";
}

}  // namespace mix
}  // namespace iitasoccer
