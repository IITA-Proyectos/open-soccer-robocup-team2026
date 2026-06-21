// amix_comm.cpp — comunicación de arqueromix: TOP/DOWN → variables planas (g_aio).
//
// ÚNICO punto que toca Serial. Replica el patrón de comm_top.cpp / comm_down.cpp
// (Serial7=TOP, Serial1=DOWN, 230400, FrameDecoder de proto.h con CRC), pero en vez de
// world_model POBLA g_aio (amix_io.h). Hermano de centralmix/mix_comm.cpp.
//
// DIFERENCIA DE DISEÑO vs centralmix (pedido de Virginia): el HEADING viene del
// SNAPSHOT del TOP (my_heading_centideg + bit4 heading_valid), NO de un BNO local.
// No se inicializa ningún BNO ni Wire. Con -DARQMIX_HEADING_OTOS el heading sale del
// OTOS (Pose2D de DOWN) para A/B; otos_heading_deg queda SIEMPRE disponible.
//
// Fuentes verificadas (no de memoria): comm_top.cpp / comm_down.cpp (Serial/baud/IDs/
// framing), proto.h / types.h / line_view.h / pose_view.h (payloads), y el espejo
// centralmix/mix_comm.cpp (mapeo a variables planas).
//
// ⚠️ NO TESTEADO EN HARDWARE.

#include "amix_comm.h"

#include "amix_io.h"
#include "amix_config.h"

#include "proto.h"        // FrameDecoder, Frame, MsgType
#include "types.h"        // WorldSnapshot, LineStatusV2, Pose2D, Velocity2D
#include "line_view.h"    // lsv2_from_frame + lsv2_line_present/angle/sensors_on_line
#include "pose_view.h"    // pose_from_frame / vel_from_frame

#include <Arduino.h>
#include <string.h>       // memcpy
#include <math.h>         // atan2f, fmodf

namespace iitasoccer {
namespace arqmix {

// Instancia global única que define el contrato (extern en amix_io.h).
AmixIO g_aio;

namespace {

FrameDecoder g_top_decoder;
FrameDecoder g_down_decoder;

#if defined(__IMXRT1062__) || defined(CORE_TEENSY)
uint8_t s_top_rx_buf[512];
uint8_t s_down_rx_buf[512];
#endif

// Mismo umbral que world_model del CENTRAL (500 ms).
constexpr unsigned long AMIX_LINK_TIMEOUT_MS = 500;

// heading_inicial (initialYaw 2025): se sella la primera vez que hay heading válido.
float g_heading_inicial = 0.0f;
bool  g_heading_inicial_set = false;

inline float wrap180(float deg) {
    deg = fmodf(deg, 360.0f);
    if (deg > 180.0f)  deg -= 360.0f;
    if (deg < -180.0f) deg += 360.0f;
    return deg;
}

inline void seal_heading_inicial_if_needed(float heading_now) {
    if (!g_heading_inicial_set) {
        g_heading_inicial = heading_now;
        g_heading_inicial_set = true;
    }
}

// ============================================================
// TOP — WorldSnapshot 0x60 → pelota / arcos / heading / referee.
// ============================================================
void apply_top_snapshot(const WorldSnapshot& s) {
    const unsigned long now = millis();

    // --- Pelota (marco robot: +X der, +Y adel; mm) ---
    g_aio.ball_x_mm   = static_cast<float>(s.ball_x_mm);   // lateral (era Yp 2025)
    g_aio.ball_y_mm   = static_cast<float>(s.ball_y_mm);   // profundidad (era Xp 2025)
    g_aio.ball_visible = (s.ball_visible != 0);
    // Ángulo a la pelota (IGUAL que centralmix/mix_comm): atan2(X, Y) → 0=adelante,
    // >0=derecha, <0=izquierda. OJO X primero. Robusto a la escala sin calibrar; el FSM
    // del arquero sigue la pelota por este ángulo en vez de mm crudos.
    g_aio.angulo_pelota_deg =
        atan2f(g_aio.ball_x_mm, g_aio.ball_y_mm) * 180.0f / static_cast<float>(M_PI);
    if (g_aio.ball_visible) {
        g_aio.t_last_ball_seen_ms = now;
    }

    // --- Arcos por ROL (opp=RIVAL / own=PROPIO): copia DIRECTA del snapshot, SIN color ---
    // La decisión "qué arco es el rival y cuál el propio" YA la tomó la placa TOP con goal_polarity
    // (regla: el arco al FRENTE del robot = rival; el de atrás = propio; con un latch que la fija al
    // arranque). El arquero NO mira color (amarillo/azul) NI invierte nada con flags de color: sólo
    // consume goal_opp/goal_own ya resueltos. Sentinela: visible=0 → ángulo/dist NO son válidos.
    g_aio.goal_opp_visible = (s.goal_opp_visible != 0);
    g_aio.goal_opp_angle   = g_aio.goal_opp_visible ? (s.goal_opp_angle_centideg / 100.0f) : 0.0f;
    g_aio.goal_opp_dist    = g_aio.goal_opp_visible ? static_cast<float>(s.goal_opp_distance_mm) : 0.0f;
    g_aio.goal_own_visible = (s.goal_own_visible != 0);
    g_aio.goal_own_angle   = g_aio.goal_own_visible ? (s.goal_own_angle_centideg / 100.0f) : 0.0f;
    g_aio.goal_own_dist    = g_aio.goal_own_visible ? static_cast<float>(s.goal_own_distance_mm) : 0.0f;

#ifndef ARQMIX_HEADING_OTOS
    // --- Heading desde el SNAPSHOT del TOP (fuente por default) ---
    const bool snap_heading_valid = (s.flags & 0x10) != 0;  // bit4 = heading_valid
    const float snap_heading = s.my_heading_centideg / 100.0f;
    if (snap_heading_valid) {
        g_aio.heading_deg   = snap_heading;
        g_aio.heading_valid = true;
        seal_heading_inicial_if_needed(snap_heading);
        g_aio.heading_error_deg = wrap180(g_aio.heading_deg - g_heading_inicial);
    } else {
        g_aio.heading_valid = false;  // sin heading confiable → la FSM usa la banda centrada
    }
#endif

    // --- Árbitro: flags bit3 = match_running ---
    g_aio.match_running = (s.flags & 0x08) != 0;

    g_aio.t_last_top_frame_ms = now;
}

void handle_top_frame(const Frame& f) {
    if (f.type != MsgType::WORLD_SNAPSHOT) return;
    if (f.payload_len != sizeof(WorldSnapshot)) return;  // schema desfasado → descartar
    WorldSnapshot snap{};
    memcpy(&snap, f.payload, sizeof(WorldSnapshot));
    apply_top_snapshot(snap);
}

// ============================================================
// DOWN — LineStatusV2 0x10 (+ OTOS Pose2D 0x11 / Velocity2D 0x12).
// La LÍNEA reemplaza los 3 sensores de luz locales del arquero 2025.
// ============================================================
void apply_down_line(const LineStatusV2& ls) {
    g_aio.line_present   = lsv2_line_present(ls);
    g_aio.line_angle_deg = lsv2_line_angle_deg(ls);   // 0 = frente
    g_aio.line_depth     = lsv2_sensors_on_line(ls);  // 0..32
    g_aio.t_last_down_frame_ms = millis();
}

void apply_down_pose(const Pose2D& pose) {
    g_aio.otos_heading_deg = pose.heading_centideg / 100.0f;  // crudo OTOS, siempre disponible
#ifdef ARQMIX_HEADING_OTOS
    g_aio.heading_deg   = g_aio.otos_heading_deg;
    g_aio.heading_valid = (pose.confidence > 0);
    if (g_aio.heading_valid) {
        seal_heading_inicial_if_needed(g_aio.heading_deg);
        g_aio.heading_error_deg = wrap180(g_aio.heading_deg - g_heading_inicial);
    }
#endif
    g_aio.t_last_down_frame_ms = millis();
}

void apply_down_vel(const Velocity2D& /*vel*/) {
    g_aio.t_last_down_frame_ms = millis();  // frescura del enlace
}

void handle_down_frame(const Frame& f) {
    LineStatusV2 ls{};
    if (lsv2_from_frame(f, ls)) { apply_down_line(ls); return; }
    Pose2D pose{};
    if (pose_from_frame(f, pose)) { apply_down_pose(pose); return; }
    Velocity2D vel{};
    if (vel_from_frame(f, vel)) { apply_down_vel(vel); return; }
}

void update_link_freshness() {
    const unsigned long now = millis();
    g_aio.top_link_fresh =
        (g_aio.t_last_top_frame_ms > 0) &&
        ((now - g_aio.t_last_top_frame_ms) < AMIX_LINK_TIMEOUT_MS);
    g_aio.down_link_fresh =
        (g_aio.t_last_down_frame_ms > 0) &&
        ((now - g_aio.t_last_down_frame_ms) < AMIX_LINK_TIMEOUT_MS);
}

}  // namespace

void amix_comm_init() {
    Serial7.begin(AMIX_UART_BAUD);  // TOP
    Serial1.begin(AMIX_UART_BAUD);  // DOWN
#if defined(__IMXRT1062__) || defined(CORE_TEENSY)
    Serial7.addMemoryForRead(s_top_rx_buf,  sizeof(s_top_rx_buf));
    Serial1.addMemoryForRead(s_down_rx_buf, sizeof(s_down_rx_buf));
#endif
    g_top_decoder.reset();
    g_down_decoder.reset();
    // (Sin BNO local: el heading se sella con el primer snapshot/pose válido.)
}

void amix_comm_tick() {
    while (Serial7.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial7.read());
        if (g_top_decoder.feed(b)) {
            handle_top_frame(g_top_decoder.get_frame());
        }
    }
    while (Serial1.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial1.read());
        if (g_down_decoder.feed(b)) {
            handle_down_frame(g_down_decoder.get_frame());
        }
    }
    update_link_freshness();
}

}  // namespace arqmix
}  // namespace iitasoccer
