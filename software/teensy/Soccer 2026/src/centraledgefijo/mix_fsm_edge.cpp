// mix_fsm_edge.cpp — FSM delantero estilo "Edge" (rodeo reactivo). Ver mix_fsm_edge.h.
//
// Se activa con -DMIX_ATTACK_EDGE. El núcleo del rodeo (la curva) vive en mix_edge.cpp
// (PURO, host-testeable); acá está la INTEGRACIÓN: árbitro + estados + primitivas.
//
// ⚠️ NO TESTEADO EN HARDWARE. Compila != anda: el signo del giro al arco (MIX_EDGE_FACE_KP),
// las distancias en cm y el sentido de mix_mover_vector se titulan en banco.

#include <Arduino.h>
#include <math.h>

#include "mix_config.h"
#include "mix_io.h"
#include "mix_motors.h"
#include "mix_edge.h"
#include "mix_fsm_edge.h"

namespace iitasoccer {
namespace mix {

// ============================================================
// Estados.
// ============================================================
enum class EstadoEdge {
    KICKOFF,
    BUSCAR,
    RODEAR,
    EMPUJAR,
    RETROCEDER,
    DETECTA_LINEA_1,
    DETECTA_LINEA_2,
    DETECTA_LINEA_3,
};

static EstadoEdge      estado            = EstadoEdge::KICKOFF;
static unsigned long   millis_inicio_estado = 0;

// ============================================================
// Parámetros del rodeo, armados desde mix_config.h (MIX_EDGE_*). Una sola vez.
// ============================================================
static EdgeParams edge_params() {
    EdgeParams p{};
    p.k_near        = MIX_EDGE_K_NEAR;
    p.b1_deg        = MIX_EDGE_B1_DEG;
    p.k_side        = MIX_EDGE_K_SIDE;
    p.b2_deg        = MIX_EDGE_B2_DEG;
    p.k_wide        = MIX_EDGE_K_WIDE;
    p.go_max_deg    = MIX_EDGE_GO_MAX_DEG;
    p.push_dist_cm  = MIX_EDGE_PUSH_DIST_CM;
    p.push_align_deg= MIX_EDGE_PUSH_ALIGN_DEG;
    p.push_goal_deg = MIX_EDGE_PUSH_GOAL_DEG;
    return p;   // POSICIÓN PURA: sin params de velocidad (esta carpeta no usa feedforward).
}

// ============================================================
// Helpers de línea (reconstrucción de los 3 "sensores" del 2025 desde DOWN, idéntica a
// mix_fsm.cpp; se replica acá porque allá es static). Sectores ±60° → RE-TUNEAR en banco.
// ============================================================
static inline bool linea_presente() {
    return g_io.line_present && (g_io.line_depth >= MIX_LINE_DEPTH_TRIGGER);
}
static inline bool linea_s1() { return linea_presente() && (g_io.line_angle_deg < -60.0f); } // izq
static inline bool linea_s2() { return linea_presente() && (fabsf(g_io.line_angle_deg) <= 60.0f); } // frente
static inline bool linea_s3() { return linea_presente() && (g_io.line_angle_deg >  60.0f); } // der

// ============================================================
// Giro para mirar al arco rival (decoupled de la traslación, como el "AC" de Edge).
// + / − = un sentido u otro; el SIGNO es perilla de banco (MIX_EDGE_FACE_KP). Si no se ve
// el arco → 0 (no girar mientras rodea; el rumbo queda donde está).
// ============================================================
static int omega_mira_arco() {
    if (!g_io.goal_opp_visible) return 0;
    float corr = MIX_EDGE_FACE_KP * g_io.goal_opp_angle;
    if (corr >  (float)MIX_EDGE_OMEGA_MAX) corr =  (float)MIX_EDGE_OMEGA_MAX;
    if (corr < -(float)MIX_EDGE_OMEGA_MAX) corr = -(float)MIX_EDGE_OMEGA_MAX;
    return (int)corr;
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

    // Power-cycle detectado por el TOP (link perdido→recuperado) → volver al arranque.
    if (g_io.top_link_fresh && !prev_top_link) {
        estado = EstadoEdge::KICKOFF;
        millis_inicio_estado = millis();
        kickoff_done = false;
        prev_go      = false;
    }
    prev_top_link = g_io.top_link_fresh;

    // STOP del árbitro: frenar y no mover.
    if (!g_io.match_running) {
        seen_stop = true;
        prev_go   = false;
        parar();
        return;
    }
    // GO sin STOP previo (transitorio del boot / pin pegado): no moverse aún.
    if (!seen_stop) {
        parar();
        return;
    }
    const bool go_edge = !prev_go;
    prev_go = true;

    // KICKOFF se arma una vez, en el primer GO real (timer anclado al GO).
    if (go_edge && !kickoff_done) {
        estado = EstadoEdge::KICKOFF;
        millis_inicio_estado = millis();
        kickoff_done = true;
    }

    // ---- Lecturas del tick ----
    const bool haypelota = g_io.ball_visible;

    switch (estado) {

        // ----------------------------------------------------
        case EstadoEdge::KICKOFF:
            // Ve la pelota → directo a rodearla.
            if (haypelota) {
                estado = EstadoEdge::RODEAR;
                millis_inicio_estado = millis();
                break;
            }
            // La línea tiene prioridad sobre la medialuna (no salir de cancha).
            if (linea_s1()) { estado = EstadoEdge::DETECTA_LINEA_1; millis_inicio_estado = millis(); break; }
            if (linea_s2()) { estado = EstadoEdge::DETECTA_LINEA_2; millis_inicio_estado = millis(); break; }
            if (linea_s3()) { estado = EstadoEdge::DETECTA_LINEA_3; millis_inicio_estado = millis(); break; }
            // No la ve → impulso fuerte y corto de medialuna hacia el centro, después buscar.
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
                estado = EstadoEdge::RODEAR;
                millis_inicio_estado = millis();
            } else {
                girar();   // gira en el lugar buscando (sentido a confirmar en banco)
            }

            if (linea_s1()) { estado = EstadoEdge::DETECTA_LINEA_1; millis_inicio_estado = millis(); }
            if (linea_s2()) { estado = EstadoEdge::DETECTA_LINEA_2; millis_inicio_estado = millis(); }
            if (linea_s3()) { estado = EstadoEdge::DETECTA_LINEA_3; millis_inicio_estado = millis(); }
            break;

        // ----------------------------------------------------
        // ★ RODEAR — el corazón. Una fórmula reactiva: dirección de avance amplificada
        //   (rodea por detrás) + giro que mira al arco, a full velocidad. Sin sub-estados.
        // ----------------------------------------------------
        case EstadoEdge::RODEAR: {
            // Acción: armar la entrada del núcleo puro desde g_io (pelota x/y, SIN velocidad) y mover.
            EdgeIn in{};
            in.ball_x_cm      = g_io.ball_x_cm;
            in.ball_y_cm      = g_io.ball_y_cm;
            in.ball_visible   = haypelota;
            in.goal_visible   = g_io.goal_opp_visible;
            in.goal_angle_deg = g_io.goal_opp_angle;
            const EdgeOut out = mix_edge_attack(in, edge_params());

            // traslación = rodeo; giro = mirar al arco (decoupled).
            mix_mover_vector(out.go_ang_deg, MIX_EDGE_SPEED, omega_mira_arco());

            // si está cerca + al frente + arco alineado → empujar (gol por inercia)
            if (out.push_ready) {
                estado = EstadoEdge::EMPUJAR;
                millis_inicio_estado = millis();
            }

            // si deja de ver la pelota hace rato → volver a buscar
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
        // EMPUJAR — empuje recto al arco por inercia. Reusa avanzar_patear (rampa fuerte +
        // heading-hold del OTOS para salir DERECHO). Tiempo fijo, "a ciegas" (la pelota tapa
        // la cámara contra el paragolpes), igual que el empuje del 2025.
        // ----------------------------------------------------
        case EstadoEdge::EMPUJAR:
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
        // RETROCEDER — despegarse corto de la pelota/línea antes de volver a buscar.
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
                parar();
                millis_inicio_estado = millis();
                estado = EstadoEdge::BUSCAR;
            }
            break;

        case EstadoEdge::DETECTA_LINEA_2:
            retroceder2();
            if (millis() - millis_inicio_estado >= 400) {
                parar();
                millis_inicio_estado = millis();
                estado = EstadoEdge::BUSCAR;
            }
            break;

        case EstadoEdge::DETECTA_LINEA_3:
            retroceder3();
            if (millis() - millis_inicio_estado >= 400) {
                parar();
                millis_inicio_estado = millis();
                estado = EstadoEdge::BUSCAR;
            }
            break;
    }
}

// ============================================================
// Nombre del estado (debug por USB).
// ============================================================
const char* mix_fsm_edge_estado_nombre() {
    switch (estado) {
        case EstadoEdge::KICKOFF:    return "KICKOFF";
        case EstadoEdge::BUSCAR:     return "BUSCAR";
        case EstadoEdge::RODEAR:     return "RODEAR";
        case EstadoEdge::EMPUJAR:    return "EMPUJAR";
        case EstadoEdge::RETROCEDER: return "RETROCEDER";
        case EstadoEdge::DETECTA_LINEA_1: return "DETECTA_LINEA_1";
        case EstadoEdge::DETECTA_LINEA_2: return "DETECTA_LINEA_2";
        case EstadoEdge::DETECTA_LINEA_3: return "DETECTA_LINEA_3";
    }
    return "?";
}

}  // namespace mix
}  // namespace iitasoccer
