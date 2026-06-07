// top_telemetry_serial.cpp — Glue Arduino de la telemetría USB de la TOP (FASE 2).
//
// TODO el cuerpo dentro de #ifdef TOP_DEBUG_TELEMETRY: con el flag OFF (envs de
// competencia top_robot1/top_robot2) compila a traducción VACÍA → 0 bytes al
// binario de competencia. El módulo PURO (serialización + comandos) vive en
// src/shared/telemetry_top.{h,cpp} (host-testeado). Contrato: docs/firmware/TELEMETRIA-TOP.md.

#include "top_telemetry_serial.h"

#ifdef TOP_DEBUG_TELEMETRY

#include <Arduino.h>
#include "telemetry_top.h"
#include "config_top.h"          // NUM_TOF
#include "cameras_runtime.h"
#include "sensors_imu.h"
#include "sensors_tof.h"
#include "comm_central.h"        // comm_central_get_last_snapshot (gateado)
#include "types.h"

namespace iitasoccer {

namespace {

bool          g_stream_on   = true;
uint32_t      g_interval_ms = 50;   // 20 Hz por default
uint32_t      g_seq         = 0;
elapsedMillis g_since_emit;

char g_rx_line[64];
int  g_rx_len = 0;

void emit_frame() {
    TopTelemetryFrame f;
    tt_frame_init(f, static_cast<uint8_t>(NUM_TOF));

    f.seq  = g_seq++;
    f.t_ms = millis();

    // ── Cámaras ──
    f.cam_front_ok          = cameras_front_alive() ? 1 : 0;
    f.cam_back_ok           = cameras_back_alive() ? 1 : 0;
    f.ball_visible          = cameras_ball_visible() ? 1 : 0;
    f.ball_x_mm             = cameras_get_ball_x_mm();
    f.ball_y_mm             = cameras_get_ball_y_mm();
    f.ball_confidence       = cameras_get_ball_confidence();
    f.ball_vx_mm_s          = cameras_get_ball_vx_mm_s();
    f.ball_vy_mm_s          = cameras_get_ball_vy_mm_s();
    f.goal_yellow_visible   = cameras_goal_yellow_visible() ? 1 : 0;
    f.goal_yellow_angle_cd  = cameras_get_goal_yellow_angle_centideg();
    f.goal_yellow_distance_mm = cameras_get_goal_yellow_distance_mm();
    f.goal_blue_visible     = cameras_goal_blue_visible() ? 1 : 0;
    f.goal_blue_angle_cd    = cameras_get_goal_blue_angle_centideg();
    f.goal_blue_distance_mm = cameras_get_goal_blue_distance_mm();
    f.cam_crc_errors        = cameras_get_crc_errors_total();
    f.cam_resyncs           = cameras_resyncs_total();

    // ── IMU ──
    f.imu_heading_deg       = sensors_imu_get_heading_deg();
    f.imu_left_deg          = sensors_imu_get_left_heading_deg();
    f.imu_right_deg         = sensors_imu_get_right_heading_deg();
    f.imu_disagreement_deg  = sensors_imu_get_disagreement_deg();
    f.imu_left_ok           = sensors_imu_left_ready() ? 1 : 0;
    f.imu_right_ok          = sensors_imu_right_ready() ? 1 : 0;
    f.imu_heading_valid     = sensors_imu_get_heading_valid() ? 1 : 0;

    // ── ToF + HC-SR04 ──
    int nt = NUM_TOF;
    if (nt > TT_MAX_TOF) nt = TT_MAX_TOF;
    for (int i = 0; i < nt; ++i) {
        f.tof_mm[i] = sensors_tof_get_distance_mm(static_cast<uint8_t>(i));
    }
    f.hcsr04_mm  = sensors_hcsr04_get_distance_mm();
    f.tof_min_mm = sensors_tof_get_min_distance_mm();

    // ── WorldSnapshot EXACTO que se difundió a CENTRAL ──
    WorldSnapshot snap;
    if (comm_central_get_last_snapshot(snap)) {
        f.snap_valid                = 1;
        f.snap_my_x_mm              = snap.my_x_mm;
        f.snap_my_y_mm              = snap.my_y_mm;
        f.snap_my_heading_cd        = snap.my_heading_centideg;
        f.snap_my_confidence        = snap.my_pose_confidence;
        f.snap_ball_x_mm            = snap.ball_x_mm;
        f.snap_ball_y_mm            = snap.ball_y_mm;
        f.snap_ball_visible         = snap.ball_visible;
        f.snap_ball_confidence      = snap.ball_confidence;
        f.snap_ball_vx_mm_s         = snap.ball_vx_mm_s;
        f.snap_ball_vy_mm_s         = snap.ball_vy_mm_s;
        f.snap_goal_opp_angle_cd    = snap.goal_opp_angle_centideg;
        f.snap_goal_opp_distance_mm = snap.goal_opp_distance_mm;
        f.snap_goal_opp_visible     = snap.goal_opp_visible;
        f.snap_goal_own_visible     = snap.goal_own_visible;
        f.snap_goal_own_angle_cd    = snap.goal_own_angle_centideg;
        f.snap_goal_own_distance_mm = snap.goal_own_distance_mm;
        f.snap_min_obstacle_mm      = snap.min_obstacle_mm;
        f.snap_referee_cmd          = snap.referee_cmd;
        f.snap_flags                = snap.flags;
    }
    // Si todavía no se envió ningún snapshot, snap_valid queda 0 (td_frame_init).

    f.frames_sent = comm_central_get_frames_sent();

    static char buf[1024];
    const int n = tt_serialize_jsonl(buf, sizeof(buf), f);
    if (n > 0) {
        Serial.write(reinterpret_cast<const uint8_t*>(buf), n);
    }
}

void dispatch(const TtCommand& c) {
    switch (c.cmd) {
        case TtCmd::PING:        emit_frame(); break;
        case TtCmd::STREAM_ON:   g_stream_on = true; break;
        case TtCmd::STREAM_OFF:  g_stream_on = false; break;
        case TtCmd::SET_RATE: {
            int32_t hz = c.arg;
            if (hz < 1)   hz = 1;
            if (hz > 200) hz = 200;
            g_interval_ms = static_cast<uint32_t>(1000 / hz);
            break;
        }
        case TtCmd::IMU_ZERO:    sensors_imu_recalibrate_zero(); break;
        case TtCmd::IMU_SAVE:    sensors_imu_save_calibration(); break;
        case TtCmd::NONE:
        case TtCmd::UNKNOWN:
        default: break;
    }
}

void pump_rx() {
    while (Serial.available() > 0) {
        const char ch = static_cast<char>(Serial.read());
        if (ch == '\n') {
            dispatch(tt_parse_command(g_rx_line, g_rx_len));
            g_rx_len = 0;
        } else if (ch != '\r') {
            if (g_rx_len < static_cast<int>(sizeof(g_rx_line))) {
                g_rx_line[g_rx_len++] = ch;
            } else {
                g_rx_len = 0;
            }
        }
    }
}

}  // namespace

void top_telemetry_init() {
    g_stream_on   = true;
    g_interval_ms = 50;
    g_seq         = 0;
    g_rx_len      = 0;
    g_since_emit  = 0;
    Serial.println("[TOP-TELEM] v1 ready");
}

void top_telemetry_tick() {
    pump_rx();
    if (g_stream_on && g_since_emit >= g_interval_ms) {
        g_since_emit = 0;
        emit_frame();
    }
}

}  // namespace iitasoccer

#endif  // TOP_DEBUG_TELEMETRY
