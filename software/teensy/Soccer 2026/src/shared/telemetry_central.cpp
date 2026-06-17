// telemetry_central.cpp — implementación pura de telemetry_central.h.
// Sin Arduino: solo libc. Host-testeable. Mismo patrón que telemetry_top.cpp.

#include "telemetry_central.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

namespace iitasoccer {

namespace {

__attribute__((format(printf, 4, 5)))
int tc_append(char* buf, int cap, int off, const char* fmt, ...) {
    if (!buf || off < 0 || off >= cap) return -1;
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf + off, (size_t)(cap - off), fmt, ap);
    va_end(ap);
    if (n < 0 || n >= cap - off) return -1;
    return off + n;
}

inline double tc_fz(float v) {
    double d = (double)v;
    if (d == 0.0) d = 0.0;  // colapsa -0.0
    return d;
}

}  // namespace

void tc_frame_init(CentralTelemetryFrame& f) {
    memset(&f, 0, sizeof(f));
    f.schema    = TELEMETRY_CENTRAL_SCHEMA;
    f.fsm_state = nullptr;   // serializa como ""
}

int tc_serialize_jsonl(char* buf, int cap, const CentralTelemetryFrame& f) {
    if (!buf || cap <= 0) return -1;

    int off = 0;

    // Encabezado: clave de sniff EXCLUSIVA "central":1, luego "v" (lo exige
    // is_telemetry_line: la línea empieza con '{' y trae "v":).
    off = tc_append(buf, cap, off,
        "{\"central\":1,\"v\":%u,\"seq\":%lu,\"t_ms\":%lu,",
        (unsigned)TELEMETRY_CENTRAL_SCHEMA,
        (unsigned long)f.seq, (unsigned long)f.t_ms);
    if (off < 0) return -1;

    // fsm
    off = tc_append(buf, cap, off,
        "\"fsm\":{\"role\":\"%s\",\"state\":\"%s\",\"match\":%u},",
        f.fsm_role ? "ATK" : "GK",
        f.fsm_state ? f.fsm_state : "",
        (unsigned)(f.fsm_match ? 1 : 0));
    if (off < 0) return -1;

    // cmd
    off = tc_append(buf, cap, off,
        "\"cmd\":{\"vx\":%d,\"vy\":%d,\"w\":%d,\"spin\":%d},",
        (int)f.cmd_vx_mm_s, (int)f.cmd_vy_mm_s, (int)f.cmd_w_cd_s, (int)f.cmd_spin_pwm);
    if (off < 0) return -1;

    // pwm
    off = tc_append(buf, cap, off,
        "\"pwm\":[%d,%d,%d],",
        (int)f.pwm[0], (int)f.pwm[1], (int)f.pwm[2]);
    if (off < 0) return -1;

    // top (salud del enlace TOP->CENTRAL)
    off = tc_append(buf, cap, off,
        "\"top\":{\"rxB\":%lu,\"fr\":%lu,\"crc\":%lu,\"rsy\":%lu,\"badsz\":%lu,"
        "\"fresh\":%u,\"age\":%lu},",
        (unsigned long)f.top_bytes, (unsigned long)f.top_frames,
        (unsigned long)f.top_crc, (unsigned long)f.top_resync,
        (unsigned long)f.top_badsz, (unsigned)(f.top_fresh ? 1 : 0),
        (unsigned long)f.top_age_ms);
    if (off < 0) return -1;

    // down (salud del enlace DOWN->CENTRAL + estado de la línea)
    off = tc_append(buf, cap, off,
        "\"down\":{\"rx\":%lu,\"crc\":%lu,\"lost\":%lu,\"rsy\":%lu,\"badsch\":%lu,"
        "\"line_fresh\":%u,\"valid\":%u,\"ev\":%u,\"age\":%lu},",
        (unsigned long)f.down_frames, (unsigned long)f.down_crc,
        (unsigned long)f.down_lost, (unsigned long)f.down_resync,
        (unsigned long)f.down_badsch, (unsigned)(f.down_line_fresh ? 1 : 0),
        (unsigned)(f.down_valid ? 1 : 0), (unsigned)f.down_event_flags,
        (unsigned long)f.down_age_ms);
    if (off < 0) return -1;

    // otos (rumbo de piso de DOWN; en R1 sin BNO ES el heading del delantero)
    off = tc_append(buf, cap, off,
        "\"otos\":{\"fresh\":%u,\"hdg\":%.2f},",
        (unsigned)(f.otos_fresh ? 1 : 0), tc_fz(f.otos_heading_deg));
    if (off < 0) return -1;

    // snap (eco del WorldSnapshot recibido del TOP).
    // 2026-06-17: AMPLIADO ADITIVO con pose XY + heading_valid + pose_conf + arco rival
    // + ball_v. Campos nuevos al final del objeto para retro-compat (parsers v1 viejos
    // siguen leyendo los originales). Schema sigue v=1.
    off = tc_append(buf, cap, off,
        "\"snap\":{\"ball_vis\":%u,\"ball_x\":%d,\"ball_y\":%d,"
        "\"goal_own_ang\":%.2f,\"goal_own_dist\":%d,\"referee\":%u,\"flags\":%u,"
        "\"my_x\":%d,\"my_y\":%d,\"my_hdg\":%.2f,\"hdg_valid\":%u,\"my_conf\":%u,"
        "\"ball_vx\":%d,\"ball_vy\":%d,"
        "\"goal_opp_vis\":%u,\"goal_opp_ang\":%.2f,\"goal_opp_dist\":%d},",
        (unsigned)(f.snap_ball_visible ? 1 : 0),
        (int)f.snap_ball_x_mm, (int)f.snap_ball_y_mm,
        tc_fz(f.snap_goal_own_angle_deg), (int)f.snap_goal_own_distance_mm,
        (unsigned)f.snap_referee_cmd, (unsigned)f.snap_flags,
        (int)f.snap_my_x_mm, (int)f.snap_my_y_mm,
        tc_fz(f.snap_my_heading_deg),
        (unsigned)(f.snap_heading_valid ? 1 : 0),
        (unsigned)f.snap_my_pose_confidence,
        (int)f.snap_ball_vx_mm_s, (int)f.snap_ball_vy_mm_s,
        (unsigned)(f.snap_goal_opp_visible ? 1 : 0),
        tc_fz(f.snap_goal_opp_angle_deg), (int)f.snap_goal_opp_distance_mm);
    if (off < 0) return -1;

    // loop (supervisor de loop-time) + cierre
    off = tc_append(buf, cap, off,
        "\"loop\":{\"max_us\":%lu,\"ema_us\":%lu}}\n",
        (unsigned long)f.loop_max_us, (unsigned long)f.loop_ema_us);
    if (off < 0) return -1;

    return off;
}

// ── Parser de comandos ───────────────────────────────────────────────────────
namespace {

constexpr int TC_TOK_MAX = 3;
constexpr int TC_TOK_LEN = 12;

int tc_tokenize(const char* s, int len, char tok[TC_TOK_MAX][TC_TOK_LEN]) {
    int nt = 0, i = 0;
    while (i < len && nt < TC_TOK_MAX) {
        while (i < len && (isspace((unsigned char)s[i]) || s[i] == ',')) ++i;
        if (i >= len) break;
        int k = 0;
        while (i < len && !isspace((unsigned char)s[i]) && s[i] != ',') {
            if (k < TC_TOK_LEN - 1) tok[nt][k++] = (char)toupper((unsigned char)s[i]);
            ++i;
        }
        tok[nt][k] = '\0';
        ++nt;
    }
    return nt;
}

inline bool tc_eq(const char* a, const char* b) { return strcmp(a, b) == 0; }

}  // namespace

TcCommand tc_parse_command(const char* s, int len) {
    TcCommand out{TcCmd::NONE, 0};
    if (!s || len <= 0) return out;

    char tok[TC_TOK_MAX][TC_TOK_LEN];
    const int nt = tc_tokenize(s, len, tok);
    if (nt == 0) return out;   // línea vacía / sólo separadores = NONE (ENTER pelado)

    out.cmd = TcCmd::UNKNOWN;
    if (tc_eq(tok[0], "PING")) {
        out.cmd = TcCmd::PING;
    } else if (tc_eq(tok[0], "STREAM") && nt >= 2) {
        if (tc_eq(tok[1], "ON"))       out.cmd = TcCmd::STREAM_ON;
        else if (tc_eq(tok[1], "OFF")) out.cmd = TcCmd::STREAM_OFF;
    }
    return out;
}

}  // namespace iitasoccer
