// top_telemetry_serial.cpp — Glue Arduino de la telemetría/monitoreo USB de la TOP.
//
// Hermano del de DOWN (down_telemetry_serial.cpp): el monitor viaja EN el binario
// de COMPETENCIA (envs top_robot2_pri y derivados, flag -DTOP_USB_MONITOR) pero
// arranca DORMIDO — silencio total por USB hasta que el host habla. Tres estados:
//
//   • DORMIDO  (default): no envía NADA. En partido no hay USB → nunca despierta
//              (match-safe).
//   • MÁQUINA  : la app tools/monitor-* manda "STREAM ON" + "PING" (latido cada
//              1 s) → stream JSON Lines continuo (para la app) + auto-off a los 3 s
//              de silencio del host.
//   • HUMANO   : en un monitor serie CRUDO, un ENTER (línea vacía o texto no
//              reconocido; CR o LF) imprime un BLOQUE de texto legible
//              (cámaras/IMU/ToF/snapshot) por TOP_MONITOR_HOST_TIMEOUT_MS (3 s),
//              sin app y sin tipear comandos. Repetir Enter lo mantiene.
//
// El flag legacy -DTOP_DEBUG_TELEMETRY (envs top_*_debug_telemetry) también
// enciende este módulo (gate OR) y se comporta IGUAL (dormido + wake). El módulo
// PURO (serialización JSON + texto humano + parseo de comandos) vive en
// src/shared/telemetry_top.{h,cpp} (host-testeado: test/test_telemetry_top).
// Contrato: docs/firmware/TELEMETRIA-TOP.md.
//
// Sin ninguno de los dos flags: traducción VACÍA (0 bytes al binario).

#include "top_telemetry_serial.h"

#if defined(TOP_DEBUG_TELEMETRY) || defined(TOP_USB_MONITOR)

#include <Arduino.h>
#include "telemetry_top.h"
#include "config_top.h"          // NUM_TOF
#include "cameras_runtime.h"
#include "sensors_imu.h"
#include "sensors_tof.h"
#include "comm_central.h"        // comm_central_get_last_snapshot (gateado)
#include "comm_down.h"           // OTOS + línea/escape que llega de la base (A1)
#include "top_eeprom_config.h"   // g_top_cfg + save/load (A2.1: comandos de config)
#include "tof_zone_mask.h"       // tof_zone_mask_set (A2.2: comandos TOF ZONE ON/OFF)
#include "types.h"

namespace iitasoccer {

namespace {

// ── Estado del monitor ───────────────────────────────────────────────────────
enum class MonMode : uint8_t { ASLEEP, MACHINE, HUMAN };

constexpr uint32_t TOP_MONITOR_HOST_TIMEOUT_MS = 3000;  // host mudo → DORMIDO
constexpr uint32_t TOP_HUMAN_INTERVAL_MS       = 500;   // 2 Hz legible a ojo

MonMode       g_mode        = MonMode::ASLEEP;
uint32_t      g_interval_ms = 50;     // 20 Hz (modo MÁQUINA)
uint32_t      g_seq         = 0;
elapsedMillis g_since_emit;           // cadencia de TX
uint32_t      g_last_host_rx_ms = 0;  // último latido del host (0 = "nunca")

char g_rx_line[64];
int  g_rx_len = 0;

// Llena TODO el frame (menos seq) leyendo el estado vivo de la TOP. Compartido
// por el emisor de máquina (JSON) y el de humano (texto).
void fill_frame(TopTelemetryFrame& f) {
    tt_frame_init(f, static_cast<uint8_t>(NUM_TOF));
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

    // ── Detecciones POR CÁMARA (pre-fusión, A1) ──
    f.ball_front_visible            = cameras_get_ball_front_visible() ? 1 : 0;
    f.ball_front_x_mm               = cameras_get_ball_front_x_mm();
    f.ball_front_y_mm               = cameras_get_ball_front_y_mm();
    f.ball_back_visible             = cameras_get_ball_back_visible() ? 1 : 0;
    f.ball_back_x_mm                = cameras_get_ball_back_x_mm();
    f.ball_back_y_mm                = cameras_get_ball_back_y_mm();
    f.goal_yellow_front_visible     = cameras_get_goal_yellow_front_visible() ? 1 : 0;
    f.goal_yellow_front_angle_cd    = cameras_get_goal_yellow_front_angle_centideg();
    f.goal_yellow_front_distance_mm = cameras_get_goal_yellow_front_distance_mm();
    f.goal_yellow_back_visible      = cameras_get_goal_yellow_back_visible() ? 1 : 0;
    f.goal_yellow_back_angle_cd     = cameras_get_goal_yellow_back_angle_centideg();
    f.goal_yellow_back_distance_mm  = cameras_get_goal_yellow_back_distance_mm();
    f.goal_blue_front_visible       = cameras_get_goal_blue_front_visible() ? 1 : 0;
    f.goal_blue_front_angle_cd      = cameras_get_goal_blue_front_angle_centideg();
    f.goal_blue_front_distance_mm   = cameras_get_goal_blue_front_distance_mm();
    f.goal_blue_back_visible        = cameras_get_goal_blue_back_visible() ? 1 : 0;
    f.goal_blue_back_angle_cd       = cameras_get_goal_blue_back_angle_centideg();
    f.goal_blue_back_distance_mm    = cameras_get_goal_blue_back_distance_mm();

    // ── IMU ──
    f.imu_heading_deg       = sensors_imu_get_heading_deg();
    f.imu_left_deg          = sensors_imu_get_left_heading_deg();
    f.imu_right_deg         = sensors_imu_get_right_heading_deg();
    f.imu_disagreement_deg  = sensors_imu_get_disagreement_deg();
    f.imu_left_ok           = sensors_imu_left_ready() ? 1 : 0;
    f.imu_right_ok          = sensors_imu_right_ready() ? 1 : 0;
    f.imu_heading_valid     = sensors_imu_get_heading_valid() ? 1 : 0;
    // Centinela (TASK-213): el 2º BNO vive como 2da opinión @1Hz aunque NO esté en la fusión
    // (imu_right_ok=0 por TOP_BNO_PRIMARY_ONLY). Getters con #else=false/0 → sin sentinel da 0.
    f.imu_sentinel_ok       = sensors_imu_sentinel_ready() ? 1 : 0;
    f.imu_sentinel_deg      = sensors_imu_sentinel_heading_deg();

#ifdef TOP_ENABLE_HEADING_XVAL
    // Salud del heading por cross-validación (TASK-213). Este es el ÚNICO call-site de
    // los getters xval → 100% dentro de #ifdef: con el flag OFF nadie los referencia y
    // --gc-sections (default Teensy) los descarta → binario de competencia byte-idéntico.
    f.xval_verdict = sensors_imu_xval_verdict();
    f.xval_score   = sensors_imu_xval_score();
    f.xval_n_indep = sensors_imu_xval_n_indep();
#endif

    // ── ToF + HC-SR04 ──
    int nt = NUM_TOF;
    if (nt > TT_MAX_TOF) nt = TT_MAX_TOF;
    for (int i = 0; i < nt; ++i) {
        f.tof_mm[i] = sensors_tof_get_distance_mm(static_cast<uint8_t>(i));
        // Zonas crudas 4x4 (campo "z" aditivo) — antes del promedio.
        for (int z = 0; z < TT_TOF_ZONES; ++z) {
            f.tof_zones[i][z] = sensors_tof_get_zone_mm(
                static_cast<uint8_t>(i), static_cast<uint8_t>(z));
        }
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
    // Si todavía no se envió ningún snapshot, snap_valid queda 0 (tt_frame_init).

    f.frames_sent = comm_central_get_frames_sent();

    // ── Lo que llega de la BASE (DOWN): OTOS + línea + vector de escape (A1) ──
    // Gateado por frescura: si no fresh, los valores quedan en sus defaults de
    // tt_frame_init (pose/vel en 0, línea en sentinelas N/A) → no se muestran ceros
    // como si fueran reales. Solo lectura; no toca nada de la conducta.
    f.down_pose_fresh = comm_down_is_pose_fresh() ? 1 : 0;
    if (f.down_pose_fresh) {
        const Pose2D& p = comm_down_get_pose();
        f.down_pose_x_mm       = p.x_mm;
        f.down_pose_y_mm       = p.y_mm;
        f.down_pose_heading_cd = p.heading_centideg;
        f.down_pose_confidence = p.confidence;
    }
    f.down_vel_fresh = comm_down_is_vel_fresh() ? 1 : 0;
    if (f.down_vel_fresh) {
        const Velocity2D& v = comm_down_get_velocity();
        f.down_vel_vx_mm_s    = v.vx_mm_s;
        f.down_vel_vy_mm_s    = v.vy_mm_s;
        f.down_vel_omega_cd_s = v.omega_centideg_s;
        f.down_vel_slip       = v.slip_estimate;
    }
    f.down_line_fresh = comm_down_is_line_fresh() ? 1 : 0;
    if (f.down_line_fresh) {
        const LineStatusV2& l = comm_down_get_line_status();
        f.down_line_schema         = l.schema_version;
        f.down_line_data_valid     = l.data_valid;
        f.down_line_angle_cd       = l.line_angle_centideg;
        f.down_escape_angle_cd     = l.escape_angle_centideg;
        f.down_line_penetration_mm = l.penetration_mm;
        f.down_line_cross_track_mm = l.cross_track_mm;
        f.down_line_present        = l.line_present;
        f.down_line_sensors_on     = l.sensors_on_line;
        f.down_line_event_flags    = l.event_flags;
        f.down_line_quality        = l.quality;
        f.down_line_sample_age_ms  = l.sample_age_ms;
    }
}

// Emite UN frame en JSON Lines (modo MÁQUINA, para la app).
void emit_frame() {
    TopTelemetryFrame f;
    fill_frame(f);
    f.seq = g_seq++;
    static char buf[2048];   // +"z" (zonas 4x4 por ToF) ⇒ frame v2 ~1350 B con 4 ToF
    const int n = tt_serialize_jsonl(buf, sizeof(buf), f);
    if (n > 0) {
        Serial.write(reinterpret_cast<const uint8_t*>(buf), n);
    }
}

// Emite UN bloque de TEXTO legible (modo HUMANO, monitor crudo).
void emit_human() {
    TopTelemetryFrame f;
    fill_frame(f);
    f.seq = g_seq++;
    static char hbuf[1024];
    const int n = tt_format_human(hbuf, sizeof(hbuf), f);
    if (n > 0) {
        Serial.write(reinterpret_cast<const uint8_t*>(hbuf), n);
    }
    // Línea CFG (A2.1): qué sensores están habilitados + bearing por ToF. La arma
    // el GLUE porque tiene g_top_cfg (el módulo puro tt_format_human no lo conoce).
    // No va en el JSON (sin schema bump): es solo para leer a ojo en banco.
    char cfg[200];
    int m = snprintf(cfg, sizeof(cfg), "  CFG cam[F%d B%d] bno[L%d R%d] us%d | tof[",
                     g_top_cfg.cam_front_en ? 1 : 0, g_top_cfg.cam_back_en ? 1 : 0,
                     g_top_cfg.bno_left_en ? 1 : 0, g_top_cfg.bno_right_en ? 1 : 0,
                     g_top_cfg.ultrasonic_en ? 1 : 0);
    for (int i = 0; i < NUM_TOF && i < TOP_CFG_NUM_TOF && m < (int)sizeof(cfg) - 1; ++i) {
        m += snprintf(cfg + m, sizeof(cfg) - m, "%s%d:en%d@%d", i ? " " : "", i,
                      g_top_cfg.tof[i].enabled ? 1 : 0, g_top_cfg.tof[i].mount_bearing_deg);
    }
    if (m < (int)sizeof(cfg) - 2) m += snprintf(cfg + m, sizeof(cfg) - m, "]\n");
    if (m > 0) Serial.write(reinterpret_cast<const uint8_t*>(cfg), m);
}

// Ejecuta un comando ya parseado y gobierna las transiciones de modo.
void dispatch(const TtCommand& c) {
    // LATIDO: cualquier línea del host renueva la ventana de 3 s.
    uint32_t now = millis();
    g_last_host_rx_ms = (now == 0) ? 1 : now;   // 0 = "nunca" (sentinel)

    switch (c.cmd) {
        case TtCmd::STREAM_ON:
            g_mode = MonMode::MACHINE;
            emit_frame();              // ack inmediato (estado completo)
            break;

        case TtCmd::STREAM_OFF:
            g_mode = MonMode::ASLEEP;  // pausa explícita
            break;

        case TtCmd::PING:
            // Latido de la app: si ya estamos en MÁQUINA, un frame de ack. Por sí
            // solo NO despierta (si el host pausó con STREAM OFF, sigue dormido).
            if (g_mode == MonMode::MACHINE) emit_frame();
            break;

        case TtCmd::SET_RATE: {
            int32_t hz = c.arg;
            if (hz < 1)   hz = 1;
            if (hz > 200) hz = 200;
            g_interval_ms = static_cast<uint32_t>(1000 / hz);
            break;
        }

        case TtCmd::IMU_ZERO:
            sensors_imu_recalibrate_zero();
            Serial.println("[TOP] heading cero recalibrado");
            break;

        case TtCmd::IMU_SAVE:
            sensors_imu_save_calibration();
            Serial.println("[TOP] calibracion IMU guardada");
            break;

        // ── Config de sensores (A2.1): mutan g_top_cfg en RAM (efecto inmediato en
        // los apply-points) + ACK. Persisten recién con CFG SAVE. ──
        case TtCmd::CAM_F_ON:  g_top_cfg.cam_front_en  = 1; Serial.println("[TOP] CAM F ON");  break;
        case TtCmd::CAM_F_OFF: g_top_cfg.cam_front_en  = 0; Serial.println("[TOP] CAM F OFF"); break;
        case TtCmd::CAM_B_ON:  g_top_cfg.cam_back_en   = 1; Serial.println("[TOP] CAM B ON");  break;
        case TtCmd::CAM_B_OFF: g_top_cfg.cam_back_en   = 0; Serial.println("[TOP] CAM B OFF"); break;
        case TtCmd::BNO_L_ON:  g_top_cfg.bno_left_en   = 1; Serial.println("[TOP] BNO L ON");  break;
        case TtCmd::BNO_L_OFF: g_top_cfg.bno_left_en   = 0; Serial.println("[TOP] BNO L OFF (re-init para efecto pleno)"); break;
        case TtCmd::BNO_R_ON:  g_top_cfg.bno_right_en  = 1; Serial.println("[TOP] BNO R ON");  break;
        case TtCmd::BNO_R_OFF: g_top_cfg.bno_right_en  = 0; Serial.println("[TOP] BNO R OFF (re-init para efecto pleno)"); break;
        case TtCmd::US_ON:     g_top_cfg.ultrasonic_en = 1; Serial.println("[TOP] US ON");     break;
        case TtCmd::US_OFF:    g_top_cfg.ultrasonic_en = 0; Serial.println("[TOP] US OFF");    break;

        case TtCmd::TOF_SET_ENABLED:
            if (c.arg >= 0 && c.arg < TOP_CFG_NUM_TOF) {
                g_top_cfg.tof[c.arg].enabled = (c.arg2 != 0) ? 1 : 0;
                Serial.print("[TOP] TOF "); Serial.print(c.arg);
                Serial.println(c.arg2 ? " ON" : " OFF");
            } else { Serial.println("[TOP] ERROR: indice ToF fuera de rango"); }
            break;

        case TtCmd::TOF_SET_POS:
            if (c.arg >= 0 && c.arg < TOP_CFG_NUM_TOF) {
                g_top_cfg.tof[c.arg].mount_bearing_deg = (int16_t)c.arg2;
                Serial.print("[TOP] TOF "); Serial.print(c.arg);
                Serial.print(" POS bearing="); Serial.print(c.arg2);
                // El bearing se aplica en localization_runtime_init() (boot). En vivo
                // requiere re-init de localización (no cableado) → efecto pleno tras
                // CFG SAVE + power-cycle.
                Serial.println(" (efecto pleno tras CFG SAVE + reinicio)");
            } else { Serial.println("[TOP] ERROR: indice ToF fuera de rango"); }
            break;

        // A2.2 — MASCARA DE ZONAS. Muta g_top_cfg.tof[n].zone_mask, que sensors_tof lee EN VIVO
        // cada tick -> efecto INMEDIATO (la zona anulada deja de contar en la distancia ya). CFG
        // SAVE para que sobreviva el reboot. (≠ POS, que se aplica en boot.)
        case TtCmd::TOF_ZONE_ON:
        case TtCmd::TOF_ZONE_OFF:
            if (c.arg >= 0 && c.arg < TOP_CFG_NUM_TOF && c.arg2 >= 0 && c.arg2 < 64) {
                const bool on = (c.cmd == TtCmd::TOF_ZONE_ON);
                g_top_cfg.tof[c.arg].zone_mask =
                    tof_zone_mask_set(g_top_cfg.tof[c.arg].zone_mask, (uint8_t)c.arg2, on);
                Serial.print("[TOP] TOF "); Serial.print(c.arg);
                Serial.print(on ? " ZONE ON " : " ZONE OFF "); Serial.print(c.arg2);
                Serial.println(" (efecto inmediato; CFG SAVE para persistir)");
            } else { Serial.println("[TOP] ERROR: indice ToF/zona fuera de rango"); }
            break;

        case TtCmd::TOF_SET_ZONEMASK:
            if (c.arg >= 0 && c.arg < TOP_CFG_NUM_TOF) {
                // 16 zonas (4x4) -> mascara de 16 bits; zonas 16..63 (no existen) quedan en 0.
                g_top_cfg.tof[c.arg].zone_mask = (uint64_t)((uint32_t)c.arg2 & 0xFFFFu);
                Serial.print("[TOP] TOF "); Serial.print(c.arg);
                Serial.print(" ZONEMASK 0x"); Serial.print((uint32_t)(c.arg2 & 0xFFFF), HEX);
                Serial.println(" (efecto inmediato; CFG SAVE para persistir)");
            } else { Serial.println("[TOP] ERROR: indice ToF fuera de rango"); }
            break;

        case TtCmd::CFG_SAVE:
            if (top_config_save(g_top_cfg)) Serial.println("[TOP] config guardada en EEPROM");
            else                            Serial.println("[TOP] ERROR: no se pudo guardar config");
            break;
        case TtCmd::CFG_LOAD:
            top_config_load(&g_top_cfg);
            Serial.println("[TOP] config recargada de EEPROM");
            break;
        case TtCmd::CFG_RESET:
            top_config_defaults(g_top_cfg);
            Serial.println("[TOP] config a DEFAULTS (todo habilitado) - CFG SAVE para persistir");
            break;

        case TtCmd::NONE:
        case TtCmd::UNKNOWN:
        default:
            // ENTER pelado o texto no reconocido = un HUMANO en el monitor crudo.
            // Despierta a HUMANO SOLO desde DORMIDO: así un LF suelto tras un
            // comando CRLF de la app (que ya dejó el modo en MÁQUINA) no tumba el
            // stream de máquina en curso.
            if (g_mode == MonMode::ASLEEP) g_mode = MonMode::HUMAN;
            break;
    }
}

// Drena el USB (Serial) sin bloquear, arma líneas y despacha comandos. CR o LF
// terminan la línea → un Enter del monitor serie despacha sea cual sea su
// fin-de-línea (CR, LF o CRLF). La línea vacía (Enter pelado) también despacha →
// en competencia despierta el modo HUMANO (ver dispatch).
void pump_rx() {
    while (Serial.available() > 0) {
        const char ch = static_cast<char>(Serial.read());
        if (ch == '\n' || ch == '\r') {
            dispatch(tt_parse_command(g_rx_line, g_rx_len));
            g_rx_len = 0;
        } else if (g_rx_len < static_cast<int>(sizeof(g_rx_line))) {
            g_rx_line[g_rx_len++] = ch;
        } else {
            // Línea demasiado larga (basura): descartar para no trabarse.
            g_rx_len = 0;
        }
    }
}

}  // namespace

void top_telemetry_init() {
    g_mode        = MonMode::ASLEEP;
    g_interval_ms = 50;   // 20 Hz (modo máquina)
    g_seq         = 0;
    g_rx_len      = 0;
    g_since_emit  = 0;
    g_last_host_rx_ms = 0;
    // Una sola línea de boot (Teensy la descarta si no hay host); después, SILENCIO
    // hasta que el host hable (la app con STREAM ON / PING, o un Enter en el monitor).
    Serial.println("[TOP-MONITOR] dormido - esperando app (STREAM ON / PING) o ENTER");
}

void top_telemetry_tick() {
    // RX: comandos del host (no bloquea).
    pump_rx();

    // APAGADO AUTOMÁTICO: si el host se calló (la app manda PING cada 1 s; un Enter
    // cuenta como latido) por más de TOP_MONITOR_HOST_TIMEOUT_MS, volver al silencio.
    // Solo aplica si ALGÚN host habló (g_last_host_rx_ms != 0) → en partido (nadie
    // habla por USB) el monitor nunca despierta ni se "apaga", queda match-safe.
    if (g_mode != MonMode::ASLEEP && g_last_host_rx_ms != 0 &&
        (millis() - g_last_host_rx_ms) > TOP_MONITOR_HOST_TIMEOUT_MS) {
        g_mode = MonMode::ASLEEP;
        g_last_host_rx_ms = 0;
    }

    if (g_mode == MonMode::ASLEEP) return;

    // TX: emitir según el modo, respetando la cadencia.
    const uint32_t interval =
        (g_mode == MonMode::HUMAN) ? TOP_HUMAN_INTERVAL_MS : g_interval_ms;
    if (g_since_emit >= interval) {
        g_since_emit = 0;
        if (g_mode == MonMode::HUMAN) emit_human();
        else                          emit_frame();
    }
}

}  // namespace iitasoccer

#endif  // TOP_DEBUG_TELEMETRY || TOP_USB_MONITOR
