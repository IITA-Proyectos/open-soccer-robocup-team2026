// telemetry_central.h — Telemetría USB de la placa CENTRAL (monitor DORMIDO, gateado).
//
// Hermano de telemetry_top.h / telemetry_down.h: módulo PURO host-testeable (solo
// <stdint.h>, SIN Arduino). Serializa un SNAPSHOT del estado del CEREBRO —el rol y
// estado de la FSM, el comando de motores aplicado + el PWM real por rueda, la salud
// de los enlaces TOP→CENTRAL y DOWN→CENTRAL, el OTOS de piso, el eco del WorldSnapshot
// recibido del TOP, y el loop-time— a UNA línea JSON (JSON Lines), y parsea los
// comandos de texto del host (stream/ping).
//
// El glue con el hardware vive en src/central/central_telemetry_serial.cpp (Arduino-only,
// gateado con -DCENTRAL_USB_MONITOR —monitor dormido en competencia—). Sin el flag, el
// glue es traducción VACÍA (0 bytes al binario; competencia byte-idéntica).
//
// Contrato v1 (clave de sniff EXCLUSIVA top-level "central":1; ni TOP "cam" ni DOWN
// "ring" la tienen). App de PC: tools/monitor-base/ (vista "Cerebro" + "Salud CENTRAL").
// Test host: test/test_telemetry_central.

#pragma once
#include <stdint.h>

namespace iitasoccer {

constexpr uint8_t TELEMETRY_CENTRAL_SCHEMA = 1;   // v1 (2026-06-15): primer contrato

// ── Snapshot que el glue arma cada tick y pasa al serializador ───────────────
// POD plano. Todo lo que la CENTRAL YA computa para competencia: estado de la FSM,
// comando aplicado + PWM real por rueda, salud de enlaces, OTOS, eco del snapshot.
struct CentralTelemetryFrame {
    uint8_t  schema;          // = TELEMETRY_CENTRAL_SCHEMA (lo setea el serializador)
    uint32_t seq;
    uint32_t t_ms;

    // ── FSM (cerebro) ──
    // role: 0 = GK (arquero), 1 = ATK (delantero). El serializador imprime "GK"/"ATK".
    uint8_t     fsm_role;     // 0=GK, 1=ATK
    const char* fsm_state;    // puntero a literal de strategy (viven para siempre); nullptr→""
    uint8_t     fsm_match;    // 0/1 (match_running)

    // ── Comando de motores APLICADO el último tick (MotorCommand) ──
    int16_t  cmd_vx_mm_s;
    int16_t  cmd_vy_mm_s;
    int16_t  cmd_w_cd_s;      // omega en centideg/s
    int16_t  cmd_spin_pwm;    // giro crudo por PWM (SEARCH delantero); 0 = sin override

    // ── PWM REAL aplicado por motor (signed, post-pisos/kick) ──
    int16_t  pwm[3];          // motors_get_applied_pwm(0..2)

    // ── Salud del enlace TOP→CENTRAL (Serial7) ──
    uint32_t top_bytes;       // comm_top_get_bytes_received (rxB)
    uint32_t top_frames;      // comm_top_get_frames_received (fr)
    uint32_t top_crc;         // comm_top_get_crc_errors
    uint32_t top_resync;      // comm_top_get_resync_events (rsy)
    uint32_t top_badsz;       // comm_top_get_snapshot_size_rejects (badsz)
    uint8_t  top_fresh;       // world_model_snapshot_is_fresh (0/1)
    uint32_t top_age_ms;      // millis()-last_rx; 0 si fresh nunca llegó (ver glue)

    // ── Salud del enlace DOWN→CENTRAL (Serial1) ──
    uint32_t down_frames;     // comm_down_get_frames_received (rx)
    uint32_t down_crc;        // comm_down_get_crc_errors
    uint32_t down_lost;       // comm_down_get_frames_lost (⚠ se solapa con crc; ver comm_down.cpp)
    uint32_t down_resync;     // comm_down_get_resync_events (rsy)
    uint32_t down_badsch;     // comm_down_line_schema_rejects (badsch)
    uint8_t  down_line_fresh; // world_model_line_is_fresh (0/1)
    uint8_t  down_valid;      // world_model_line_data_valid (0/1)
    uint8_t  down_event_flags;// world_model_line_event_flags (EV_* OR-eados)
    uint32_t down_age_ms;     // millis()-last_line_rx

    // ── OTOS de piso (Capa 1 broadcast de DOWN) — en R1 sin BNO ES el rumbo ──
    uint8_t  otos_fresh;      // world_model_otos_is_fresh (0/1)
    float    otos_heading_deg;// world_model_get_otos_heading_deg

    // ── Eco del WorldSnapshot que la CENTRAL RECIBE del TOP ──
    // 2026-06-17: AMPLIADO con pose XY del robot + heading + heading_valid + confianza
    // + arco RIVAL + velocidad de la pelota. Aditivo: el contrato JSON sigue siendo v=1
    // (campos nuevos = ADITIVOS, no bumpean schema). Cuando TOP no ancla (pose_fusion
    // OFF o ToF sin lecturas válidas), my_x/my_y vienen 0 y my_pose_confidence=0 → la
    // app debe filtrar por confianza antes de dibujar la pose.
    uint8_t  snap_ball_visible;
    int16_t  snap_ball_x_mm;
    int16_t  snap_ball_y_mm;
    int16_t  snap_ball_vx_mm_s;         // velocidad pelota (marco robot), mm/s
    int16_t  snap_ball_vy_mm_s;
    uint8_t  snap_goal_own_visible;     // gatea ang/dist del arco propio
    float    snap_goal_own_angle_deg;   // válido sólo si goal_own_visible
    int16_t  snap_goal_own_distance_mm; // válido sólo si goal_own_visible
    uint8_t  snap_goal_opp_visible;     // gatea ang/dist del arco rival
    float    snap_goal_opp_angle_deg;   // válido sólo si goal_opp_visible
    int16_t  snap_goal_opp_distance_mm; // válido sólo si goal_opp_visible
    int16_t  snap_my_x_mm;              // pose del robot (cancha). 0 si TOP no ancla
    int16_t  snap_my_y_mm;
    float    snap_my_heading_deg;       // heading absoluto. Válido si snap_heading_valid
    uint8_t  snap_heading_valid;        // flag bit4 del WorldSnapshot
    uint8_t  snap_my_pose_confidence;   // 0-100. 0 = pose desconocida (no dibujar)
    uint8_t  snap_referee_cmd;
    uint8_t  snap_flags;                // byte crudo de WorldSnapshot.flags

    // ── Loop-time (supervisor R2) ──
    uint32_t loop_max_us;     // g_loop_monitor.max_us (peor vuelta vista)
    uint32_t loop_ema_us;     // g_loop_monitor.ema_us (promedio suave)
};

// Inicializa un frame a ceros + schema. fsm_state queda nullptr (serializa como "").
void tc_frame_init(CentralTelemetryFrame& f);

// Serializa `f` a UNA línea JSON terminada en '\n' dentro de buf (capacidad cap).
// Retorna bytes escritos (sin contar '\0') o -1 si no entra / args inválidos.
// Recomendado cap >= 512 (el frame v1 ronda ~360 B).
int tc_serialize_jsonl(char* buf, int cap, const CentralTelemetryFrame& f);

// ── Comandos host → firmware ─────────────────────────────────────────────────
enum class TcCmd : uint8_t {
    NONE = 0,
    UNKNOWN,
    PING,
    STREAM_ON,
    STREAM_OFF,
};

struct TcCommand {
    TcCmd   cmd;
    int32_t arg;    // reservado (futuro: RATE/SET); hoy sin uso
};

TcCommand tc_parse_command(const char* s, int len);

}  // namespace iitasoccer
