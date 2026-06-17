// central_telemetry_serial.cpp — Glue Arduino del monitor USB de la CENTRAL.
//
// Clon del de TOP (top_telemetry_serial.cpp): el monitor viaja EN el binario de los
// envs *_bb (flag -DCENTRAL_USB_MONITOR) pero arranca DORMIDO — silencio total por USB
// hasta que el host habla. Tres estados:
//
//   • DORMIDO  (default): no envía NADA. En partido no hay USB → nunca despierta
//              (match-safe). El robot juega idéntico.
//   • MÁQUINA  : la app tools/monitor-base manda "STREAM ON" + "PING" (latido cada
//              1 s) → stream JSON Lines continuo (clave de sniff "central":1) +
//              auto-off a los 3 s de silencio del host.
//   • HUMANO   : en un monitor serie CRUDO, un ENTER (línea vacía o texto no
//              reconocido) imprime UNA línea de texto legible (rol/estado/cmd/salud)
//              por CENTRAL_MONITOR_HOST_TIMEOUT_MS, sin app. Repetir Enter lo mantiene.
//
// IMPORTANTE — la TRAMPA DE LA 'S': la app manda "STREAM ON\n"/"PING\n". El handler de
// chars sueltos de main_central.cpp ('g'/'s'/'d'/'x') interpretaría la 'S' de STREAM
// como STOP del robot. La solución vive en main_central.cpp: las LÍNEAS de texto
// (terminadas en \n/\r) se rutean ACÁ primero (central_telemetry_consume_line); sólo
// los chars de CONTROL reales que NO forman parte de una palabra-comando del monitor
// llegan al handler g/s/d/x. Ver main_central.cpp.
//
// El módulo PURO (serialización JSON + parseo de comandos) vive en
// src/shared/telemetry_central.{h,cpp} (host-testeado: test/test_telemetry_central).
//
// Sin el flag: traducción VACÍA (0 bytes al binario; competencia byte-idéntica).

#include "central_telemetry_serial.h"

#if defined(CENTRAL_USB_MONITOR)

#include <Arduino.h>
#include "telemetry_central.h"
#include "strategy.h"            // strategy_get_role / strategy_get_state_name
#include "world_model.h"         // snapshot/línea/OTOS + frescuras
#include "motors_zircon.h"       // motors_get_applied_pwm
#include "comm_top.h"            // salud del enlace TOP->CENTRAL
#include "comm_down.h"           // salud del enlace DOWN->CENTRAL
#include "loop_monitor.h"        // LoopMonitor (g_loop_monitor)
#include "types.h"
#ifdef CENTRAL_EEPROM_CALIB
#include "central_config.h"          // cc_parse_command (comandos de calibración SET/GET/SAVE)
#include "central_eeprom_config.h"   // central_eeprom_apply_set / central_eeprom_save
#endif

namespace iitasoccer {

// Getter del contador de rechazos por schema de línea (CC-01 espejo). Definido en
// comm_down.cpp dentro de namespace iitasoccer; se declara acá en el MISMO namespace
// para que linkee sin tocar comm_down.h (idéntico patrón al de main_central.cpp:43).
uint32_t comm_down_line_schema_rejects();

// El supervisor de loop-time (g_loop_monitor) vive en main_central.cpp (anon namespace).
// Lo exponen estos accessors, definidos ahí bajo el mismo flag; los declaramos acá para
// leer max/ema sin acoplar headers (linkean en namespace iitasoccer).
uint32_t central_loop_max_us();
uint32_t central_loop_ema_us();

namespace {

// ── Estado del monitor ───────────────────────────────────────────────────────
enum class MonMode : uint8_t { ASLEEP, MACHINE, HUMAN };

constexpr uint32_t CENTRAL_MONITOR_HOST_TIMEOUT_MS = 3000;  // host mudo → DORMIDO
constexpr uint32_t CENTRAL_HUMAN_INTERVAL_MS       = 500;   // 2 Hz legible a ojo

MonMode       g_mode        = MonMode::ASLEEP;
uint32_t      g_interval_ms = 50;     // 20 Hz (modo MÁQUINA)
uint32_t      g_seq         = 0;
elapsedMillis g_since_emit;           // cadencia de TX
uint32_t      g_last_host_rx_ms = 0;  // último latido del host (0 = "nunca")

// ── Comando de motores APLICADO el último tick (cacheado por note_command) ──
int16_t g_cmd_vx = 0, g_cmd_vy = 0, g_cmd_w = 0, g_cmd_spin = 0;

// ── Edad de los enlaces: estampamos millis() cuando el contador de frames sube ──
uint32_t g_top_frames_seen  = 0, g_top_last_rx_ms  = 0;
uint32_t g_down_frames_seen = 0, g_down_last_rx_ms = 0;

// Reconstruye el byte de flags del WorldSnapshot desde los getters individuales del
// world_model (no hay getter del byte crudo). bit0 in_own_penalty · b1 partner_alive ·
// b2 partner_sees_ball · b3 match_running · b4 heading_valid. Espejo de types.h.
// ⚠ bit3 usa world_model_match_running(), que OR-ea el override manual de banco
// (force_match_running) — coherente con lo que la FSM realmente ve este tick.
uint8_t snapshot_flags_byte() {
    uint8_t fl = 0;
    if (world_model_in_own_penalty_area()) fl |= 1u << 0;
    if (world_model_partner_alive())       fl |= 1u << 1;
    if (world_model_partner_sees_ball())   fl |= 1u << 2;
    if (world_model_match_running())       fl |= 1u << 3;
    if (world_model_heading_valid())       fl |= 1u << 4;
    return fl;
}

// Llena TODO el frame (menos seq) leyendo el estado vivo de la CENTRAL.
void fill_frame(CentralTelemetryFrame& f) {
    tc_frame_init(f);
    const uint32_t now = millis();
    f.t_ms = now;

    // ── FSM (cerebro) ──
    f.fsm_role  = (strategy_get_role() == RobotRole::ATTACKER) ? 1 : 0;
    f.fsm_state = strategy_get_state_name();   // literal de strategy (vive para siempre)
    f.fsm_match = world_model_match_running() ? 1 : 0;

    // ── Comando aplicado (cacheado en el loop tras motors_apply_command) ──
    f.cmd_vx_mm_s  = g_cmd_vx;
    f.cmd_vy_mm_s  = g_cmd_vy;
    f.cmd_w_cd_s   = g_cmd_w;
    f.cmd_spin_pwm = g_cmd_spin;

    // ── PWM real por rueda ──
    f.pwm[0] = motors_get_applied_pwm(0);
    f.pwm[1] = motors_get_applied_pwm(1);
    f.pwm[2] = motors_get_applied_pwm(2);

    // ── Salud TOP→CENTRAL ──
    f.top_bytes  = comm_top_get_bytes_received();
    f.top_frames = comm_top_get_frames_received();
    f.top_crc    = comm_top_get_crc_errors();
    f.top_resync = comm_top_get_resync_events();
    f.top_badsz  = comm_top_get_snapshot_size_rejects();
    f.top_fresh  = world_model_snapshot_is_fresh() ? 1 : 0;
    f.top_age_ms = (g_top_last_rx_ms == 0) ? 0u : (now - g_top_last_rx_ms);

    // ── Salud DOWN→CENTRAL ──
    f.down_frames      = comm_down_get_frames_received();
    f.down_crc         = comm_down_get_crc_errors();
    f.down_lost        = comm_down_get_frames_lost();
    f.down_resync      = comm_down_get_resync_events();
    f.down_badsch      = comm_down_line_schema_rejects();
    f.down_line_fresh  = world_model_line_is_fresh() ? 1 : 0;
    f.down_valid       = world_model_line_data_valid() ? 1 : 0;
    f.down_event_flags = world_model_line_event_flags();
    f.down_age_ms      = (g_down_last_rx_ms == 0) ? 0u : (now - g_down_last_rx_ms);

    // ── OTOS de piso (en R1 sin BNO ES el rumbo del delantero) ──
    f.otos_fresh       = world_model_otos_is_fresh() ? 1 : 0;
    f.otos_heading_deg = world_model_get_otos_heading_deg();

    // ── Eco del WorldSnapshot recibido del TOP ──
    // 2026-06-17: AMPLIADO con pose XY + heading + confianza + arco rival + ball_v.
    // Si TOP no llena (pose_fusion OFF o ToF sin lecturas), viajan 0/false; la app
    // gatea por my_pose_confidence antes de dibujar el robot en cancha.
    f.snap_ball_visible       = world_model_ball_visible() ? 1 : 0;
    f.snap_ball_x_mm          = static_cast<int16_t>(world_model_get_ball_x_mm());
    f.snap_ball_y_mm          = static_cast<int16_t>(world_model_get_ball_y_mm());
    f.snap_ball_vx_mm_s       = world_model_get_ball_vx_mm_s();
    f.snap_ball_vy_mm_s       = world_model_get_ball_vy_mm_s();
    f.snap_goal_own_visible   = world_model_goal_own_visible() ? 1 : 0;
    f.snap_goal_own_angle_deg = world_model_get_goal_own_angle_deg();
    f.snap_goal_own_distance_mm = static_cast<int16_t>(world_model_get_goal_own_distance_mm());
    f.snap_goal_opp_visible   = world_model_goal_opp_visible() ? 1 : 0;
    f.snap_goal_opp_angle_deg = world_model_get_goal_opp_angle_deg();
    f.snap_goal_opp_distance_mm = static_cast<int16_t>(world_model_get_goal_opp_distance_mm());
    f.snap_my_x_mm            = static_cast<int16_t>(world_model_get_my_x_mm());
    f.snap_my_y_mm            = static_cast<int16_t>(world_model_get_my_y_mm());
    f.snap_my_heading_deg     = world_model_get_my_heading_deg();
    f.snap_heading_valid      = world_model_heading_valid() ? 1 : 0;
    f.snap_my_pose_confidence = world_model_get_my_pose_confidence();
    f.snap_referee_cmd        = world_model_referee_cmd();
    f.snap_flags              = snapshot_flags_byte();

    // ── Loop-time ──
    f.loop_max_us = central_loop_max_us();
    f.loop_ema_us = central_loop_ema_us();
}

// Emite UN frame en JSON Lines (modo MÁQUINA, para la app).
void emit_frame() {
    CentralTelemetryFrame f;
    fill_frame(f);
    f.seq = g_seq++;
    static char buf[640];   // frame v1 ~360 B; margen holgado
    const int n = tc_serialize_jsonl(buf, sizeof(buf), f);
    if (n > 0) {
        Serial.write(reinterpret_cast<const uint8_t*>(buf), n);
    }
}

// Emite UNA línea de TEXTO legible (modo HUMANO, monitor crudo). Resumen de un
// vistazo: rol/estado/match + cmd + salud de enlaces + loop. No es el JSON.
void emit_human() {
    CentralTelemetryFrame f;
    fill_frame(f);
    f.seq = g_seq++;
    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "[CENTRAL] seq %lu %s %s match=%s | cmd vx%d vy%d w%d spin%d | "
        "pwm[%d,%d,%d] | top[fr%lu crc%lu %s] down[rx%lu crc%lu lost%lu %s] | "
        "otos %s loop_us(max/avg)=%lu/%lu\n",
        (unsigned long)f.seq, f.fsm_role ? "ATK" : "GK",
        f.fsm_state ? f.fsm_state : "?", f.fsm_match ? "RUN" : "STOP",
        (int)f.cmd_vx_mm_s, (int)f.cmd_vy_mm_s, (int)f.cmd_w_cd_s, (int)f.cmd_spin_pwm,
        (int)f.pwm[0], (int)f.pwm[1], (int)f.pwm[2],
        (unsigned long)f.top_frames, (unsigned long)f.top_crc, f.top_fresh ? "FRESH" : "STALE",
        (unsigned long)f.down_frames, (unsigned long)f.down_crc, (unsigned long)f.down_lost,
        f.down_line_fresh ? "FRESH" : "STALE",
        f.otos_fresh ? "FRESH" : "STALE",
        (unsigned long)f.loop_max_us, (unsigned long)f.loop_ema_us);
    if (n > 0) {
        if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
        Serial.write(reinterpret_cast<const uint8_t*>(buf), n);
    }
}

// Ejecuta un comando ya parseado y gobierna las transiciones de modo.
void dispatch(const TcCommand& c) {
    // LATIDO: cualquier línea del host renueva la ventana de 3 s.
    const uint32_t now = millis();
    g_last_host_rx_ms = (now == 0) ? 1 : now;   // 0 = "nunca" (sentinel)

    switch (c.cmd) {
        case TcCmd::STREAM_ON:
            g_mode = MonMode::MACHINE;
            emit_frame();              // ack inmediato (estado completo)
            break;

        case TcCmd::STREAM_OFF:
            g_mode = MonMode::ASLEEP;  // pausa explícita
            break;

        case TcCmd::PING:
            // Latido de la app: si ya estamos en MÁQUINA, un frame de ack. Por sí
            // solo NO despierta (si el host pausó con STREAM OFF, sigue dormido).
            if (g_mode == MonMode::MACHINE) emit_frame();
            break;

        case TcCmd::NONE:
        case TcCmd::UNKNOWN:
        default:
            // ENTER pelado o texto no reconocido = un HUMANO en el monitor crudo.
            // Despierta a HUMANO SOLO desde DORMIDO: así un LF suelto tras un
            // comando CRLF de la app (que ya dejó el modo en MÁQUINA) no tumba el
            // stream de máquina en curso.
            if (g_mode == MonMode::ASLEEP) g_mode = MonMode::HUMAN;
            break;
    }
}

}  // namespace

// ── Ruteo de líneas desde main_central.cpp (resuelve la TRAMPA DE LA 'S') ─────
// main_central.cpp arma las líneas de su handler USB y nos pasa CADA línea completa.
// Devuelve true si la línea ERA un comando del monitor (PING/STREAM/ENTER no-comando)
// → main NO debe re-procesar sus chars como g/s/d/x. Devuelve false si la línea
// contiene un control real del robot (g/s/d/x) → main la maneja.
//
// Regla: una línea es "del monitor" si tc_parse_command la reconoce como PING/STREAM,
// O si está VACÍA (ENTER pelado → modo humano). Una línea con un char de control
// (g/s/d/x, sola o con basura) NO es del monitor → la maneja main. Así "STREAM ON"
// va al monitor (y su 'S' jamás llega al STOP), y "s" sola frena el robot como siempre.
bool central_telemetry_consume_line(const char* line, int len) {
    const TcCommand c = tc_parse_command(line, len);
    if (c.cmd == TcCmd::PING || c.cmd == TcCmd::STREAM_ON || c.cmd == TcCmd::STREAM_OFF) {
        dispatch(c);
        return true;   // comando del monitor: NO lo toca el handler g/s/d/x
    }
    if (c.cmd == TcCmd::NONE) {
        // Línea vacía (ENTER pelado): la usa el monitor (modo humano). Igual la
        // dejamos pasar a main como "no consumida" para no romper su semántica de
        // ENTER=GO; pero despertamos el humano acá. Empate inocuo: main trata
        // ENTER como GO sólo bajo CENTRAL_ENABLE_MANUAL_START.
        dispatch(c);
        return false;
    }
#ifdef CENTRAL_EEPROM_CALIB
    // Comandos de CALIBRACIÓN (SET/GET/SAVE). Se prueban ANTES de devolver "no consumida":
    // si la línea es un comando de calibración VÁLIDO, lo aplicamos y la consumimos (así su
    // texto no cae en el handler g/s/d/x de main como control del robot).
    {
        const CcCommand cc = cc_parse_command(line, len);
        if (cc.valid) {
            if      (cc.cmd == CcCmd::SET)  central_eeprom_apply_set(cc.param, cc.idx, cc.value);
            else if (cc.cmd == CcCmd::SAVE) central_eeprom_save();
            else if (cc.cmd == CcCmd::GET)  emit_frame();  // (el eco "ccfg" en el frame = incremento siguiente)
            return true;
        }
    }
#endif
    // c.cmd == UNKNOWN: texto no-comando (incluye líneas con g/s/d/x). NO lo
    // consumimos: main lo procesa char-por-char (control real del robot).
    return false;
}

void central_telemetry_note_command(int16_t vx_mm_s, int16_t vy_mm_s,
                                    int16_t omega_cd_s, int16_t spin_pwm) {
    g_cmd_vx   = vx_mm_s;
    g_cmd_vy   = vy_mm_s;
    g_cmd_w    = omega_cd_s;
    g_cmd_spin = spin_pwm;
}

void central_telemetry_init() {
    g_mode        = MonMode::ASLEEP;
    g_interval_ms = 50;   // 20 Hz (modo máquina)
    g_seq         = 0;
    g_since_emit  = 0;
    g_last_host_rx_ms = 0;
    g_top_frames_seen  = 0; g_top_last_rx_ms  = 0;
    g_down_frames_seen = 0; g_down_last_rx_ms = 0;
    // Una sola línea de boot (Teensy la descarta si no hay host); después, SILENCIO
    // hasta que el host hable (la app con STREAM ON / PING, o un Enter en el monitor).
    Serial.println("[CENTRAL-MONITOR] dormido - esperando app (STREAM ON / PING) o ENTER");
}

void central_telemetry_tick() {
    const uint32_t now = millis();

    // Estampar la "edad" de cada enlace: si el contador de frames subió desde la
    // última vuelta, llegó algo nuevo → sellar el instante. (No bloquea; barato.)
    const uint32_t tfr = comm_top_get_frames_received();
    if (tfr != g_top_frames_seen)  { g_top_frames_seen  = tfr; g_top_last_rx_ms  = (now == 0) ? 1 : now; }
    const uint32_t dfr = comm_down_get_frames_received();
    if (dfr != g_down_frames_seen) { g_down_frames_seen = dfr; g_down_last_rx_ms = (now == 0) ? 1 : now; }

    // APAGADO AUTOMÁTICO: si el host se calló (la app manda PING cada 1 s; un Enter
    // cuenta como latido) por más de CENTRAL_MONITOR_HOST_TIMEOUT_MS, volver al silencio.
    // Solo aplica si ALGÚN host habló (g_last_host_rx_ms != 0) → en partido (nadie
    // habla por USB) el monitor nunca despierta ni se "apaga", queda match-safe.
    if (g_mode != MonMode::ASLEEP && g_last_host_rx_ms != 0 &&
        (now - g_last_host_rx_ms) > CENTRAL_MONITOR_HOST_TIMEOUT_MS) {
        g_mode = MonMode::ASLEEP;
        g_last_host_rx_ms = 0;
    }

    if (g_mode == MonMode::ASLEEP) return;

    // TX: emitir según el modo, respetando la cadencia.
    const uint32_t interval =
        (g_mode == MonMode::HUMAN) ? CENTRAL_HUMAN_INTERVAL_MS : g_interval_ms;
    if (g_since_emit >= interval) {
        g_since_emit = 0;
        if (g_mode == MonMode::HUMAN) emit_human();
        else                          emit_frame();
    }
}

}  // namespace iitasoccer

#endif  // CENTRAL_USB_MONITOR
