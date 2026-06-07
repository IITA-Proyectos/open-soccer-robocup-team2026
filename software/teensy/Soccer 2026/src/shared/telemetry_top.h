// telemetry_top.h — Telemetría USB de la placa TOP (modo DEBUG, gateado). FASE 2.
//
// Hermano de telemetry_down.h: módulo PURO host-testeable (solo <stdint.h>, SIN
// Arduino). Serializa un SNAPSHOT del estado de la TOP —las cámaras (pelota + 2
// arcos), los 2 IMU (heading), los 4 ToF + HC-SR04, y el WorldSnapshot fusionado
// que viaja a la CENTRAL— a UNA línea JSON (JSON Lines), y parsea los comandos
// de texto del host (stream, rate, recalibrar/guardar IMU).
//
// El glue con el hardware vive en src/top/top_telemetry_serial.cpp (Arduino-only,
// GATEADO con -DTOP_DEBUG_TELEMETRY). Contrato: docs/firmware/TELEMETRIA-TOP.md.
// App de PC: tools/monitor-base/ (vista TOP) / tools/monitor-top/.

#pragma once
#include <stdint.h>

namespace iitasoccer {

constexpr uint8_t TELEMETRY_TOP_SCHEMA = 1;

constexpr int TT_MAX_TOF = 6;   // 4 fijos hoy + 2 futuros (NUM_TOF_MAX)

// Sentinel "sin lectura" de los ToF (espejo de TOF_NO_READING).
constexpr uint16_t TT_TOF_NO_READING = 0xFFFFu;

// ── Snapshot que el glue arma cada tick y pasa al serializador ───────────────
// POD plano. Todo lo que el firmware YA computa para competencia: las cámaras,
// los IMU, los ToF, y el WorldSnapshot EXACTO que comm_central_send_snapshot()
// difundió a la CENTRAL.
struct TopTelemetryFrame {
    uint8_t  schema;          // = TELEMETRY_TOP_SCHEMA (lo setea el serializador)
    uint32_t seq;
    uint32_t t_ms;

    // ── Cámaras (fusión front+back) ──
    uint8_t  cam_front_ok;
    uint8_t  cam_back_ok;
    uint8_t  ball_visible;
    int16_t  ball_x_mm;       // marco robot: +x derecha, +y frente
    int16_t  ball_y_mm;
    uint8_t  ball_confidence; // 0..100
    int16_t  ball_vx_mm_s;
    int16_t  ball_vy_mm_s;
    uint8_t  goal_yellow_visible;
    int16_t  goal_yellow_angle_cd;
    int16_t  goal_yellow_distance_mm;
    uint8_t  goal_blue_visible;
    int16_t  goal_blue_angle_cd;
    int16_t  goal_blue_distance_mm;
    uint32_t cam_crc_errors;
    uint32_t cam_resyncs;

    // ── IMU (2× BNO055 fusionados) ──
    float    imu_heading_deg;     // fusión circular, CCW+
    float    imu_left_deg;
    float    imu_right_deg;
    float    imu_disagreement_deg;
    uint8_t  imu_left_ok;
    uint8_t  imu_right_ok;
    uint8_t  imu_heading_valid;   // flag heading_valid del snapshot (bit4)

    // ── ToF (4 VL53L7CX) + HC-SR04 ──
    uint8_t  num_tof;             // NUM_TOF (4)
    uint16_t tof_mm[TT_MAX_TOF];  // por sensor; TT_TOF_NO_READING = sin lectura
    uint16_t hcsr04_mm;
    uint16_t tof_min_mm;          // min de los 4 ToF + HC-SR04

    // ── WorldSnapshot fusionado (lo que viaja a CENTRAL) ──
    uint8_t  snap_valid;          // 0 si todavía no se envió ninguno
    int16_t  snap_my_x_mm;
    int16_t  snap_my_y_mm;
    int16_t  snap_my_heading_cd;
    uint8_t  snap_my_confidence;
    int16_t  snap_ball_x_mm;
    int16_t  snap_ball_y_mm;
    uint8_t  snap_ball_visible;
    uint8_t  snap_ball_confidence;
    int16_t  snap_ball_vx_mm_s;
    int16_t  snap_ball_vy_mm_s;
    int16_t  snap_goal_opp_angle_cd;
    int16_t  snap_goal_opp_distance_mm;
    uint8_t  snap_goal_opp_visible;
    uint8_t  snap_goal_own_visible;
    int16_t  snap_goal_own_angle_cd;
    int16_t  snap_goal_own_distance_mm;
    uint16_t snap_min_obstacle_mm;
    uint8_t  snap_referee_cmd;    // 0=stop,1=start,2=halftime,3=reset
    uint8_t  snap_flags;          // bit0 in_own_penalty, b1 partner_alive,
                                  // b2 partner_sees_ball, b3 match_running, b4 heading_valid

    // ── Diagnóstico ──
    uint32_t frames_sent;         // snapshots enviados a CENTRAL
};

// Inicializa un frame a ceros + schema + ToF en sentinel.
void tt_frame_init(TopTelemetryFrame& f, uint8_t num_tof);

// Serializa `f` a UNA línea JSON terminada en '\n' dentro de buf (capacidad cap).
// Retorna bytes escritos (sin contar '\0') o -1 si no entra / args inválidos.
// Recomendado cap >= 768.
int tt_serialize_jsonl(char* buf, int cap, const TopTelemetryFrame& f);

// ── Comandos host → firmware ─────────────────────────────────────────────────
enum class TtCmd : uint8_t {
    NONE = 0,
    UNKNOWN,
    PING,
    STREAM_ON,
    STREAM_OFF,
    SET_RATE,     // "RATE <hz>"
    IMU_ZERO,     // "IMU ZERO"  → sensors_imu_recalibrate_zero()
    IMU_SAVE,     // "IMU SAVE"  → sensors_imu_save_calibration()
};

struct TtCommand {
    TtCmd   cmd;
    int32_t arg;
};

TtCommand tt_parse_command(const char* s, int len);

}  // namespace iitasoccer
