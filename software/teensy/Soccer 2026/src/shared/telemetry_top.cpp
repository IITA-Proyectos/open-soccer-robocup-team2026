// telemetry_top.cpp — implementación pura de telemetry_top.h (FASE 2).
// Sin Arduino: solo libc. Host-testeable. Mismo patrón que telemetry_down.cpp.

#include "telemetry_top.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

namespace iitasoccer {

namespace {

__attribute__((format(printf, 4, 5)))
int tt_append(char* buf, int cap, int off, const char* fmt, ...) {
    if (!buf || off < 0 || off >= cap) return -1;
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf + off, (size_t)(cap - off), fmt, ap);
    va_end(ap);
    if (n < 0 || n >= cap - off) return -1;
    return off + n;
}

inline double tt_fz(float v) {
    double d = (double)v;
    if (d == 0.0) d = 0.0;  // colapsa -0.0
    return d;
}

}  // namespace

void tt_frame_init(TopTelemetryFrame& f, uint8_t num_tof) {
    memset(&f, 0, sizeof(f));
    f.schema = TELEMETRY_TOP_SCHEMA;
    if (num_tof > TT_MAX_TOF) num_tof = TT_MAX_TOF;
    f.num_tof = num_tof;
    for (int i = 0; i < TT_MAX_TOF; ++i) f.tof_mm[i] = TT_TOF_NO_READING;
    f.hcsr04_mm = TT_TOF_NO_READING;
    f.tof_min_mm = TT_TOF_NO_READING;
}

int tt_serialize_jsonl(char* buf, int cap, const TopTelemetryFrame& f) {
    if (!buf || cap <= 0) return -1;
    int nt = f.num_tof;
    if (nt < 0) nt = 0;
    if (nt > TT_MAX_TOF) nt = TT_MAX_TOF;

    int off = 0;

    off = tt_append(buf, cap, off,
        "{\"v\":%u,\"seq\":%lu,\"t_ms\":%lu,",
        (unsigned)TELEMETRY_TOP_SCHEMA,
        (unsigned long)f.seq, (unsigned long)f.t_ms);
    if (off < 0) return -1;

    // cam
    off = tt_append(buf, cap, off,
        "\"cam\":{\"fok\":%u,\"bok\":%u,\"bvis\":%u,\"bx\":%d,\"by\":%d,"
        "\"bconf\":%u,\"bvx\":%d,\"bvy\":%d,\"gy_vis\":%u,\"gy_ang\":%d,"
        "\"gy_dist\":%d,\"gb_vis\":%u,\"gb_ang\":%d,\"gb_dist\":%d,"
        "\"crc\":%lu,\"resync\":%lu},",
        (unsigned)(f.cam_front_ok ? 1 : 0), (unsigned)(f.cam_back_ok ? 1 : 0),
        (unsigned)(f.ball_visible ? 1 : 0), (int)f.ball_x_mm, (int)f.ball_y_mm,
        (unsigned)f.ball_confidence, (int)f.ball_vx_mm_s, (int)f.ball_vy_mm_s,
        (unsigned)(f.goal_yellow_visible ? 1 : 0), (int)f.goal_yellow_angle_cd,
        (int)f.goal_yellow_distance_mm,
        (unsigned)(f.goal_blue_visible ? 1 : 0), (int)f.goal_blue_angle_cd,
        (int)f.goal_blue_distance_mm,
        (unsigned long)f.cam_crc_errors, (unsigned long)f.cam_resyncs);
    if (off < 0) return -1;

    // imu
    off = tt_append(buf, cap, off,
        "\"imu\":{\"hdg\":%.2f,\"left\":%.2f,\"right\":%.2f,\"disagree\":%.2f,"
        "\"lok\":%u,\"rok\":%u,\"valid\":%u},",
        tt_fz(f.imu_heading_deg), tt_fz(f.imu_left_deg), tt_fz(f.imu_right_deg),
        tt_fz(f.imu_disagreement_deg),
        (unsigned)(f.imu_left_ok ? 1 : 0), (unsigned)(f.imu_right_ok ? 1 : 0),
        (unsigned)(f.imu_heading_valid ? 1 : 0));
    if (off < 0) return -1;

    // tof
    off = tt_append(buf, cap, off, "\"tof\":{\"n\":%u,\"d\":[", (unsigned)nt);
    if (off < 0) return -1;
    for (int i = 0; i < nt; ++i) {
        off = tt_append(buf, cap, off, (i == 0) ? "%u" : ",%u",
                        (unsigned)f.tof_mm[i]);
        if (off < 0) return -1;
    }
    off = tt_append(buf, cap, off, "],\"hc\":%u,\"min\":%u},",
                    (unsigned)f.hcsr04_mm, (unsigned)f.tof_min_mm);
    if (off < 0) return -1;

    // snap
    off = tt_append(buf, cap, off,
        "\"snap\":{\"valid\":%u,\"x\":%d,\"y\":%d,\"hdg_cd\":%d,\"conf\":%u,"
        "\"bx\":%d,\"by\":%d,\"bvis\":%u,\"bconf\":%u,\"bvx\":%d,\"bvy\":%d,"
        "\"opp_ang\":%d,\"opp_dist\":%d,\"opp_vis\":%u,\"own_vis\":%u,"
        "\"own_ang\":%d,\"own_dist\":%d,\"obst\":%u,\"ref\":%u,\"flags\":%u},",
        (unsigned)(f.snap_valid ? 1 : 0), (int)f.snap_my_x_mm, (int)f.snap_my_y_mm,
        (int)f.snap_my_heading_cd, (unsigned)f.snap_my_confidence,
        (int)f.snap_ball_x_mm, (int)f.snap_ball_y_mm,
        (unsigned)(f.snap_ball_visible ? 1 : 0), (unsigned)f.snap_ball_confidence,
        (int)f.snap_ball_vx_mm_s, (int)f.snap_ball_vy_mm_s,
        (int)f.snap_goal_opp_angle_cd, (int)f.snap_goal_opp_distance_mm,
        (unsigned)(f.snap_goal_opp_visible ? 1 : 0),
        (unsigned)(f.snap_goal_own_visible ? 1 : 0),
        (int)f.snap_goal_own_angle_cd, (int)f.snap_goal_own_distance_mm,
        (unsigned)f.snap_min_obstacle_mm, (unsigned)f.snap_referee_cmd,
        (unsigned)f.snap_flags);
    if (off < 0) return -1;

    // diag + cierre
    off = tt_append(buf, cap, off,
        "\"diag\":{\"frames_sent\":%lu}}\n", (unsigned long)f.frames_sent);
    if (off < 0) return -1;

    return off;
}

// ── Parser de comandos ───────────────────────────────────────────────────────
namespace {

constexpr int TT_TOK_MAX = 3;
constexpr int TT_TOK_LEN = 12;

int tt_tokenize(const char* s, int len, char tok[TT_TOK_MAX][TT_TOK_LEN]) {
    int nt = 0, i = 0;
    while (i < len && nt < TT_TOK_MAX) {
        while (i < len && (isspace((unsigned char)s[i]) || s[i] == ',')) ++i;
        if (i >= len) break;
        int k = 0;
        while (i < len && !isspace((unsigned char)s[i]) && s[i] != ',') {
            if (k < TT_TOK_LEN - 1) tok[nt][k++] = (char)toupper((unsigned char)s[i]);
            ++i;
        }
        tok[nt][k] = '\0';
        ++nt;
    }
    return nt;
}

inline bool tt_eq(const char* a, const char* b) { return strcmp(a, b) == 0; }

}  // namespace

TtCommand tt_parse_command(const char* s, int len) {
    TtCommand out{TtCmd::NONE, 0};
    if (!s || len <= 0) return out;

    char tok[TT_TOK_MAX][TT_TOK_LEN];
    const int nt = tt_tokenize(s, len, tok);
    if (nt == 0) return out;

    out.cmd = TtCmd::UNKNOWN;
    if (tt_eq(tok[0], "PING")) {
        out.cmd = TtCmd::PING;
    } else if (tt_eq(tok[0], "STREAM") && nt >= 2) {
        if (tt_eq(tok[1], "ON"))       out.cmd = TtCmd::STREAM_ON;
        else if (tt_eq(tok[1], "OFF")) out.cmd = TtCmd::STREAM_OFF;
    } else if (tt_eq(tok[0], "RATE") && nt >= 2) {
        char* end = nullptr;
        const long hz = strtol(tok[1], &end, 10);
        if (end && *end == '\0' && hz > 0) {
            out.cmd = TtCmd::SET_RATE;
            out.arg = (int32_t)hz;
        }
    } else if (tt_eq(tok[0], "IMU") && nt >= 2) {
        if (tt_eq(tok[1], "ZERO"))      out.cmd = TtCmd::IMU_ZERO;
        else if (tt_eq(tok[1], "SAVE")) out.cmd = TtCmd::IMU_SAVE;
    }
    return out;
}

}  // namespace iitasoccer
