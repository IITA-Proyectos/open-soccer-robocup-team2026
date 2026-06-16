// telemetry_top.h — Telemetría USB de la placa TOP (modo DEBUG, gateado). FASE 2.
//
// Hermano de telemetry_down.h: módulo PURO host-testeable (solo <stdint.h>, SIN
// Arduino). Serializa un SNAPSHOT del estado de la TOP —las cámaras (pelota + 2
// arcos), los 2 IMU (heading), los 4 ToF + HC-SR04, y el WorldSnapshot fusionado
// que viaja a la CENTRAL— a UNA línea JSON (JSON Lines), y parsea los comandos
// de texto del host (stream, rate, recalibrar/guardar IMU).
//
// El glue con el hardware vive en src/top/top_telemetry_serial.cpp (Arduino-only,
// gateado con -DTOP_USB_MONITOR —monitor dormido en competencia— o el legacy
// -DTOP_DEBUG_TELEMETRY). Contrato: docs/firmware/TELEMETRIA-TOP.md.
// App de PC: tools/monitor-base/ (vista TOP) / tools/monitor-top/.

#pragma once
#include <stdint.h>

namespace iitasoccer {

constexpr uint8_t TELEMETRY_TOP_SCHEMA = 2;   // v2 (2026-06-13): +per-cámara +base(OTOS/línea)

constexpr int TT_MAX_TOF = 6;   // 4 fijos hoy + 2 futuros (NUM_TOF_MAX)
constexpr int TT_TOF_ZONES = 16;  // zonas 4x4 por sensor (campo "z" aditivo v2)

// Sentinel "sin lectura" de los ToF (espejo de TOF_NO_READING).
constexpr uint16_t TT_TOF_NO_READING = 0xFFFFu;

// Sentinels N/A para los campos de la base (espejo de LSV2_NA_* de types.h; se
// definen acá para que el módulo siga puro/desacoplado — los valores coinciden:
// el glue copia los LSV2_NA_* crudos y caen en estos mismos números).
constexpr int16_t  TT_NA_I16 = -32768;
constexpr uint16_t TT_NA_U16 = 0xFFFFu;

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

    // ── Detecciones POR CÁMARA (pre-fusión) — A1 monitor de posicionamiento ──
    // Lo que ve CADA óptica por separado, ANTES de fusionar. Combinar con
    // cam_front_ok/cam_back_ok (watchdog) para distinguir "no ve" de "caída".
    // El delta front↔back delata la pelota fantasma del promedio fusionado.
    uint8_t  ball_front_visible;
    int16_t  ball_front_x_mm;
    int16_t  ball_front_y_mm;
    uint8_t  ball_back_visible;
    int16_t  ball_back_x_mm;
    int16_t  ball_back_y_mm;
    uint8_t  goal_yellow_front_visible;
    int16_t  goal_yellow_front_angle_cd;
    int16_t  goal_yellow_front_distance_mm;
    uint8_t  goal_yellow_back_visible;
    int16_t  goal_yellow_back_angle_cd;
    int16_t  goal_yellow_back_distance_mm;
    uint8_t  goal_blue_front_visible;
    int16_t  goal_blue_front_angle_cd;
    int16_t  goal_blue_front_distance_mm;
    uint8_t  goal_blue_back_visible;
    int16_t  goal_blue_back_angle_cd;
    int16_t  goal_blue_back_distance_mm;

    // ── IMU (2× BNO055 fusionados) ──
    float    imu_heading_deg;     // fusión circular, CCW+
    float    imu_left_deg;
    float    imu_right_deg;
    float    imu_disagreement_deg;
    uint8_t  imu_left_ok;
    uint8_t  imu_right_ok;
    uint8_t  imu_heading_valid;   // flag heading_valid del snapshot (bit4)
    // Centinela (TASK-213): el 2º BNO NO entra a la fusión (imu_right_ok=0 por
    // TOP_BNO_PRIMARY_ONLY) pero SÍ se lee @1 Hz como 2da opinión. Estos 2 campos dejan
    // que el monitor lo muestre como "centinela @1Hz" + su heading, en vez de "falla".
    uint8_t  imu_sentinel_ok;     // 1 = el 2º BNO se inicializó y se lee como centinela
    float    imu_sentinel_deg;    // último yaw del centinela (CCW+ crudo), @1 Hz

    // ── ToF (4 VL53L7CX) + HC-SR04 ──
    uint8_t  num_tof;             // NUM_TOF (4)
    uint16_t tof_mm[TT_MAX_TOF];  // por sensor; TT_TOF_NO_READING = sin lectura
    uint16_t hcsr04_mm;
    uint16_t tof_min_mm;          // min de los 4 ToF + HC-SR04
    // v2 ADITIVO ("z"): zonas crudas 4x4 (16) por sensor, ANTES del promedio
    // (mean_valid_zones). Cada zona sin lectura = TT_TOF_NO_READING. Permite ver
    // QUÉ zona miente y, a futuro, enmascararlas (A2.2). Emitir "z" es aditivo:
    // un parser viejo lo ignora; el contrato (schema 2) NO se rompe.
    uint16_t tof_zones[TT_MAX_TOF][TT_TOF_ZONES];

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

    // ── Lo que llega de la BASE (DOWN broadcast): OTOS + línea + vector de escape ──
    // A1. El TOP recibe esto por comm_down; *_fresh = dato vivo (< 500 ms). Si no
    // fresh, NO interpretar los valores (pueden ser stale/cero). data_valid es la
    // compuerta maestra de la geometría de línea. El "vector de escape" = dirección
    // down_escape_angle_cd + magnitud down_line_penetration_mm.
    uint8_t  down_pose_fresh;
    int16_t  down_pose_x_mm;
    int16_t  down_pose_y_mm;
    int16_t  down_pose_heading_cd;
    uint8_t  down_pose_confidence;
    uint8_t  down_vel_fresh;
    int16_t  down_vel_vx_mm_s;
    int16_t  down_vel_vy_mm_s;
    int16_t  down_vel_omega_cd_s;
    uint8_t  down_vel_slip;
    uint8_t  down_line_fresh;
    uint8_t  down_line_schema;
    uint8_t  down_line_data_valid;
    int16_t  down_line_angle_cd;        // N/A = TT_NA_I16
    int16_t  down_escape_angle_cd;      // N/A = TT_NA_I16 (dirección del vector de escape)
    uint16_t down_line_penetration_mm;  // N/A = TT_NA_U16 (magnitud del vector de escape)
    int16_t  down_line_cross_track_mm;  // N/A = TT_NA_I16
    uint8_t  down_line_present;
    uint8_t  down_line_sensors_on;
    uint8_t  down_line_event_flags;
    uint8_t  down_line_quality;
    uint8_t  down_line_sample_age_ms;

    // ── Cross-validación de salud del heading (TASK-213, aditivo "xval") ──
    // Veredicto del BNO primario contra datos independientes (OTOS+cámara+centinela).
    // Default 0 (SANO) si el flag TOP_ENABLE_HEADING_XVAL está OFF (nadie lo llena).
    uint8_t  xval_verdict;   // 0=SANO, 1=SOSPECHA, 2=MALO (XvalVerdict)
    uint8_t  xval_score;     // 0..100 (salud del primario, EMA)
    uint8_t  xval_n_indep;   // refs independientes válidas (0/1/2) en la última ventana
};

// Inicializa un frame a ceros + schema + ToF en sentinel.
void tt_frame_init(TopTelemetryFrame& f, uint8_t num_tof);

// Serializa `f` a UNA línea JSON terminada en '\n' dentro de buf (capacidad cap).
// Retorna bytes escritos (sin contar '\0') o -1 si no entra / args inválidos.
// Recomendado cap >= 1536 (v2 con per-cámara + base ronda ~950 B).
int tt_serialize_jsonl(char* buf, int cap, const TopTelemetryFrame& f);

// Formatea `f` como BLOQUE de TEXTO HUMANO multi-línea (no JSON), legible de un
// vistazo en un monitor serie crudo. Se usa en el modo "ENTER" del monitor de
// competencia: el alumno aprieta Enter y ve cámaras/IMU/ToF/snapshot en claro,
// sin la app de PC. Termina en '\n'. Retorna bytes escritos (sin contar '\0') o
// -1 si no entra / args inválidos. Recomendado cap >= 1024 (v2: ~9 líneas).
int tt_format_human(char* buf, int cap, const TopTelemetryFrame& f);

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
    // ── Config de sensores (A2.1) — mutan g_top_cfg en RAM (efecto inmediato) ──
    CAM_F_ON, CAM_F_OFF,   // "CAM F ON|OFF"
    CAM_B_ON, CAM_B_OFF,   // "CAM B ON|OFF"
    BNO_L_ON, BNO_L_OFF,   // "BNO L ON|OFF"
    BNO_R_ON, BNO_R_OFF,   // "BNO R ON|OFF"
    US_ON, US_OFF,         // "US ON|OFF"
    TOF_SET_ENABLED,       // "TOF <n> ON|OFF"            → arg=n, arg2=1/0
    TOF_SET_POS,           // "TOF <n> POS FRONT|BACK|RIGHT|LEFT" → arg=n, arg2=bearing_deg
    TOF_ZONE_ON,           // "TOF <n> ZONE ON <idx>"     → arg=n, arg2=zona idx (A2.2: activar zona)
    TOF_ZONE_OFF,          // "TOF <n> ZONE OFF <idx>"    → arg=n, arg2=zona idx (A2.2: anular zona)
    TOF_SET_ZONEMASK,      // "TOF <n> ZONEMASK <hex>"    → arg=n, arg2=mascara 16-bit (1=usar,0=anular)
    CFG_SAVE,              // "CFG SAVE"  → persiste TopConfig en EEPROM
    CFG_LOAD,              // "CFG LOAD"  → recarga de EEPROM
    CFG_RESET,             // "CFG RESET" → defaults en RAM (no persiste)
};

struct TtCommand {
    TtCmd   cmd;
    int32_t arg;    // ToF index (TOF_*), o Hz (SET_RATE)
    int32_t arg2;   // valor (TOF_SET_ENABLED: 0/1; TOF_SET_POS: bearing_deg)
};

TtCommand tt_parse_command(const char* s, int len);

}  // namespace iitasoccer
