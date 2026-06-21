// amix_io.h — VARIABLES PLANAS estilo 2025 que alimentan la FSM de arqueromix.
//
// CONTRATO. Hermano de centralmix/mix_io.h. Reemplaza el "world model" por un bloque
// de variables planas (como las globales sueltas del arquero 2025: haypelota, Xp/Yp,
// error, s1/s2/s3), pero POBLADAS desde los datos de TOP/DOWN por amix_comm.
//
// FLUJO:
//   amix_comm_tick() → decodifica frames TOP (WORLD_SNAPSHOT) y DOWN (LineStatusV2,
//                      Pose2D/Velocity2D) y ESCRIBE estos campos.
//   amix_fsm_tick()  → LEE estos campos (nunca toca Serial ni world_model).
//
// MARCO DE LA PELOTA (contrato, NO el del 2025): +X=derecha, +Y=adelante; mm.
//   El arquero 2025 decidía por (Xp=profundidad, Yp=lateral). Acá:
//     ball_y_mm ≈ profundidad (era Xp);  ball_x_mm ≈ lateral (era Yp).
//   ⚠️ El signo lateral (a qué lado mover) se confirma en banco (ver amix_config).
//
// ⚠️ NO TESTEADO EN HARDWARE.

#pragma once
#include <stdint.h>

namespace iitasoccer {
namespace arqmix {

struct AmixIO {
    // ---- Pelota (marco robot: +X=derecha, +Y=adelante; mm) ----
    float ball_x_mm = 0.0f;          // + = derecha (lateral; era Yp 2025)
    float ball_y_mm = 0.0f;          // + = adelante (profundidad; era Xp 2025)
    bool  ball_visible = false;      // ¿la cámara (TOP) ve la pelota? (haypelota 2025)

    // ---- Arcos (ángulo ° marco robot, +=derecha; distancia mm) ----
    bool  goal_yellow_visible = false;
    bool  goal_blue_visible   = false;
    float goal_yellow_angle = 0.0f;
    float goal_yellow_dist  = 0.0f;
    float goal_blue_angle   = 0.0f;
    float goal_blue_dist    = 0.0f;

    // ---- Heading / rumbo (del SNAPSHOT del TOP por default) ----
    float heading_deg = 0.0f;        // heading actual (grados).
    bool  heading_valid = false;     // ¿el heading es confiable? (bit4 del snapshot / conf OTOS)
    float heading_error_deg = 0.0f;  // heading_deg - heading_inicial, wrap [-180,180]. == 'error' 2025.
    float otos_heading_deg = 0.0f;   // heading crudo del OTOS (DOWN), SIEMPRE disponible para A/B.

    // ---- Línea / piso (de DOWN; reemplaza los 3 sensores de luz del 2025) ----
    bool    line_present = false;    // ¿hay línea? (histéresis de DOWN). == OR de s1/s2/s3 2025.
    float   line_angle_deg = 0.0f;   // ángulo de la línea (0 = frente)
    uint8_t line_depth = 0;          // # sensores sobre la línea (0..32)

    // ---- Árbitro / partido ----
    bool match_running = false;      // ¿partido en curso? La FSM solo MUEVE si es true.

    // ---- Timers que la FSM expone (millis) ----
    unsigned long t_last_ball_seen_ms = 0;  // última vez con ball_visible=true (lo sella amix_comm).

    // ---- Frescura de enlaces (rx-watchdog liviano; lo mantiene amix_comm) ----
    unsigned long t_last_top_frame_ms  = 0;
    unsigned long t_last_down_frame_ms = 0;
    bool top_link_fresh  = false;
    bool down_link_fresh = false;
};

// Instancia global única (estilo 2025: estado compartido por variables globales).
extern AmixIO g_aio;

}  // namespace arqmix
}  // namespace iitasoccer
