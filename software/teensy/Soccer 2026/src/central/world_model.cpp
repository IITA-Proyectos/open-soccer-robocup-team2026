#include "world_model.h"
#include "line_view.h"
#include <Arduino.h>

namespace iitasoccer {

namespace {

WorldSnapshot g_snap{};
LineStatusV2  g_line{};
uint32_t g_snap_last_ms = 0;
uint32_t g_line_last_ms = 0;

Pose2D     g_otos_pose{};
Velocity2D g_otos_vel{};
uint32_t   g_otos_last_ms = 0;
constexpr uint32_t OTOS_TIMEOUT_MS = 500;

constexpr uint32_t SNAPSHOT_TIMEOUT_MS = 500;
constexpr uint32_t LINE_TIMEOUT_MS = 500;

inline bool flag_set(uint8_t flags, uint8_t bit) {
    return (flags & (1 << bit)) != 0;
}

}  // namespace

void world_model_init() {
    g_snap = WorldSnapshot{};
    g_line = LineStatusV2{};
    g_snap_last_ms = 0;
    g_line_last_ms = 0;
    g_otos_pose = Pose2D{};
    g_otos_vel  = Velocity2D{};
    g_otos_last_ms = 0;
}

void world_model_apply_snapshot(const WorldSnapshot& snap) {
    g_snap = snap;
    g_snap_last_ms = millis();
}

void world_model_apply_line(const LineStatusV2& line) {
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

bool     world_model_line_detected()        { return lsv2_line_present(g_line); }
float    world_model_get_line_angle_deg()   { return lsv2_line_angle_deg(g_line); }
uint8_t  world_model_get_line_depth()       { return lsv2_penetration_u8(g_line); }
bool     world_model_imminent_exit()        { return lsv2_imminent_exit(g_line); }
bool     world_model_line_data_valid()      { return g_line.data_valid != 0; }
uint8_t  world_model_line_event_flags()     { return g_line.event_flags; }

// cross_track perpendicular firmado (mm). Delegamos en el helper PURO de
// line_view.h (lsv2_cross_track_mm) que ya honra la compuerta data_valid y el
// centinela N/A devolviendo 0.0f. La VALIDEZ va aparte porque 0.0f es ambiguo
// (robot centrado sobre la línea == 0 mm, igual que "sin señal"); el arquero
// necesita distinguirlos para elegir control-por-cross_track vs fallback-por-depth.
float    world_model_get_cross_track_mm()   { return lsv2_cross_track_mm(g_line); }
bool     world_model_cross_track_valid()    { return g_line.data_valid != 0
                                                  && g_line.cross_track_mm != LSV2_NA_I16; }

// Override de arranque manual (F3). En competencia el flag de build esta OFF y
// nadie llama al setter -> g_force_match_running queda false (sin efecto).
static bool g_force_match_running = false;
void world_model_set_force_match_running(bool on) { g_force_match_running = on; }
bool world_model_match_running()        { return flag_set(g_snap.flags, 3) || g_force_match_running; }
bool world_model_in_own_penalty_area()  { return flag_set(g_snap.flags, 0); }
bool world_model_partner_alive()        { return flag_set(g_snap.flags, 1); }
bool world_model_partner_sees_ball()    { return flag_set(g_snap.flags, 2); }

uint8_t world_model_referee_cmd()       { return g_snap.referee_cmd; }

void world_model_apply_otos_pose(const Pose2D& pose) { g_otos_pose = pose; g_otos_last_ms = millis(); }
void world_model_apply_otos_vel(const Velocity2D& vel) { g_otos_vel = vel; }  // freshness va con la pose (llegan juntas a 100 Hz)
bool world_model_otos_is_fresh() { return g_otos_last_ms > 0 && (millis() - g_otos_last_ms) < OTOS_TIMEOUT_MS; }

float   world_model_get_otos_x_mm()         { return static_cast<float>(g_otos_pose.x_mm); }
float   world_model_get_otos_y_mm()         { return static_cast<float>(g_otos_pose.y_mm); }
float   world_model_get_otos_heading_deg()  { return g_otos_pose.heading_centideg / 100.0f; }
float   world_model_get_otos_vx_mm_s()      { return static_cast<float>(g_otos_vel.vx_mm_s); }
float   world_model_get_otos_vy_mm_s()      { return static_cast<float>(g_otos_vel.vy_mm_s); }
float   world_model_get_otos_omega_deg_s()  { return g_otos_vel.omega_centideg_s / 100.0f; }
uint8_t world_model_get_otos_slip()         { return g_otos_vel.slip_estimate; }
uint8_t world_model_otos_pose_confidence()  { return g_otos_pose.confidence; }

}  // namespace iitasoccer
