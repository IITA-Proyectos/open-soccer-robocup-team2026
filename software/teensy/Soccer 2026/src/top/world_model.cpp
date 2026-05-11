#include "world_model.h"
#include "comm_down.h"
#include "comm_arbiter.h"
#include "sensors_imu.h"
#include "sensors_tof.h"
#include "cameras.h"

#include <Arduino.h>

namespace iitasoccer {

namespace {

// Estado interno. Float para facilitar cálculos; los getters convierten si hace falta.
float g_my_x = 0.0f;
float g_my_y = 0.0f;
float g_my_heading = 0.0f;
uint8_t g_my_confidence = 0;

bool   g_ball_visible = false;
float  g_ball_x = 0.0f;
float  g_ball_y = 0.0f;
uint32_t g_ball_last_seen_ms = 0;

// Goals
bool g_goal_opp_visible = false;
float g_goal_opp_angle = 0.0f;
float g_goal_opp_distance = 0.0f;
bool g_goal_own_visible = false;

// Línea
bool  g_line_detected = false;
float g_line_angle = 0.0f;
bool  g_imminent_exit = false;

// Obstáculo
uint16_t g_min_obstacle = TOF_NO_READING;

// Partner
bool  g_partner_alive = false;
float g_partner_x = 0.0f;
float g_partner_y = 0.0f;
bool  g_partner_sees_ball = false;

}  // namespace

void world_model_init() {
    world_model_reset();
}

void world_model_update() {
    // Pose propia: combina OTOS (de DOWN) + IMU.
    if (comm_down_is_pose_fresh()) {
        const Pose2D& pose = comm_down_get_pose();
        g_my_x = static_cast<float>(pose.x_mm);
        g_my_y = static_cast<float>(pose.y_mm);
        g_my_confidence = pose.confidence;
    }
    // IMU pisa el heading del OTOS si el BNO055 está OK (más confiable a corto plazo).
    if (sensors_imu_left_ready() || sensors_imu_right_ready()) {
        g_my_heading = sensors_imu_get_heading_deg();
    }

    // Pelota desde cámara — usamos solo la cámara 1 por ahora. Cuando la 2 esté
    // operativa, fusionar (preferir la mejor visibility).
    // TODO: este código asume que cameras_tick() y cameras_get_packet() existen.
    //       Falta wirearlo en main_top.cpp. Por ahora dejamos al world_model leer
    //       desde una variable global que populará el main loop.

    // Línea desde DOWN.
    if (comm_down_is_line_fresh()) {
        const LineStatus& ls = comm_down_get_line_status();
        g_line_angle = ls.angle_centideg / 100.0f;
        g_imminent_exit = (ls.imminent_exit_flag != 0);
        g_line_detected = (ls.depth_mm > 0);
    } else {
        g_line_detected = false;
        g_imminent_exit = false;
    }

    // Obstáculo más cercano.
    g_min_obstacle = sensors_tof_get_min_distance_mm();

    // Match state lo consulta el strategy directo a comm_arbiter (no hay que
    // duplicarlo acá).
}

float   world_model_get_my_x_mm()        { return g_my_x; }
float   world_model_get_my_y_mm()        { return g_my_y; }
float   world_model_get_my_heading_deg() { return g_my_heading; }
uint8_t world_model_get_my_confidence()  { return g_my_confidence; }

bool    world_model_ball_visible()       { return g_ball_visible; }
float   world_model_get_ball_x_mm()      { return g_ball_x; }
float   world_model_get_ball_y_mm()      { return g_ball_y; }
uint32_t world_model_get_ball_age_ms()   {
    return g_ball_visible ? (millis() - g_ball_last_seen_ms) : 0xFFFFFFFFUL;
}

bool  world_model_goal_opp_visible()        { return g_goal_opp_visible; }
float world_model_get_goal_opp_angle_deg()  { return g_goal_opp_angle; }
float world_model_get_goal_opp_distance_mm(){ return g_goal_opp_distance; }
bool  world_model_goal_own_visible()        { return g_goal_own_visible; }

bool  world_model_line_detected()           { return g_line_detected; }
float world_model_get_line_angle_deg()      { return g_line_angle; }
bool  world_model_imminent_exit()           { return g_imminent_exit; }

uint16_t world_model_get_min_obstacle_mm()  { return g_min_obstacle; }

bool   world_model_partner_alive()       { return g_partner_alive; }
float  world_model_get_partner_x_mm()    { return g_partner_x; }
float  world_model_get_partner_y_mm()    { return g_partner_y; }
bool   world_model_partner_sees_ball()   { return g_partner_sees_ball; }

bool   world_model_match_running()       { return comm_arbiter_is_match_running(); }

void world_model_reset() {
    g_my_x = g_my_y = g_my_heading = 0.0f;
    g_my_confidence = 0;
    g_ball_visible = false;
    g_ball_last_seen_ms = 0;
    g_goal_opp_visible = g_goal_own_visible = false;
    g_line_detected = g_imminent_exit = false;
    g_min_obstacle = TOF_NO_READING;
    g_partner_alive = false;
    g_partner_sees_ball = false;
}

}  // namespace iitasoccer
