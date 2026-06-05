// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#include "world_model.h"
#include <Arduino.h>

namespace iitasoccer {

namespace {

WorldSnapshot g_snap{};
LineStatus    g_line{};
uint32_t g_snap_last_ms = 0;
uint32_t g_line_last_ms = 0;

constexpr uint32_t SNAPSHOT_TIMEOUT_MS = 500;
constexpr uint32_t LINE_TIMEOUT_MS = 500;

inline bool flag_set(uint8_t flags, uint8_t bit) {
    return (flags & (1 << bit)) != 0;
}

}  // namespace

void world_model_init() {
    g_snap = WorldSnapshot{};
    g_line = LineStatus{};
    g_snap_last_ms = 0;
    g_line_last_ms = 0;
}

void world_model_apply_snapshot(const WorldSnapshot& snap) {
    g_snap = snap;
    g_snap_last_ms = millis();
}

void world_model_apply_line(const LineStatus& line) {
    g_line = line;
    g_line_last_ms = millis();
}

bool world_model_snapshot_is_fresh() {
    return g_snap_last_ms > 0 && (millis() - g_snap_last_ms) < SNAPSHOT_TIMEOUT_MS;
}

bool world_model_line_is_fresh() {
    return g_line_last_ms > 0 && (millis() - g_line_last_ms) < LINE_TIMEOUT_MS;
}

float world_model_get_my_x_mm()             { return static_cast<float>(g_snap.my_x_mm); }
float world_model_get_my_y_mm()             { return static_cast<float>(g_snap.my_y_mm); }
float world_model_get_my_heading_deg()      { return g_snap.my_heading_centideg / 100.0f; }

bool  world_model_ball_visible()            { return g_snap.ball_visible != 0; }
float world_model_get_ball_x_mm()           { return static_cast<float>(g_snap.ball_x_mm); }
float world_model_get_ball_y_mm()           { return static_cast<float>(g_snap.ball_y_mm); }

bool  world_model_goal_opp_visible()        { return g_snap.goal_opp_visible != 0; }
float world_model_get_goal_opp_angle_deg()  { return g_snap.goal_opp_angle_centideg / 100.0f; }
float world_model_get_goal_opp_distance_mm(){ return static_cast<float>(g_snap.goal_opp_distance_mm); }

uint16_t world_model_get_min_obstacle_mm()  { return g_snap.min_obstacle_mm; }

bool     world_model_line_detected()        { return g_line.depth_mm > 0; }
float    world_model_get_line_angle_deg()   { return g_line.angle_centideg / 100.0f; }
uint8_t  world_model_get_line_depth()       { return g_line.depth_mm; }
bool     world_model_imminent_exit()        { return g_line.imminent_exit_flag != 0; }

bool world_model_match_running()        { return flag_set(g_snap.flags, 3); }
bool world_model_in_own_penalty_area()  { return flag_set(g_snap.flags, 0); }
bool world_model_partner_alive()        { return flag_set(g_snap.flags, 1); }
bool world_model_partner_sees_ball()    { return flag_set(g_snap.flags, 2); }

uint8_t world_model_referee_cmd()       { return g_snap.referee_cmd; }

}  // namespace iitasoccer
