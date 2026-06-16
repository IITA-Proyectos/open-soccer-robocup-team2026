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
    // Zonas (campo "z" aditivo): sentinel por defecto (un frame sin datos no
    // serializa ceros que parecerían lectura real).
    for (int i = 0; i < TT_MAX_TOF; ++i)
        for (int z = 0; z < TT_TOF_ZONES; ++z)
            f.tof_zones[i][z] = TT_TOF_NO_READING;
    // Sentinels N/A de la línea de la base: un frame recién init (sin datos de
    // DOWN) serializa como N/A, no como 0 (que parecería lectura real). El glue
    // los sobrescribe sólo cuando el dato está fresh.
    f.down_line_angle_cd       = TT_NA_I16;
    f.down_escape_angle_cd     = TT_NA_I16;
    f.down_line_cross_track_mm = TT_NA_I16;
    f.down_line_penetration_mm = TT_NA_U16;
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

    // camf — detecciones de la cámara FRONTAL (pre-fusión, A1)
    off = tt_append(buf, cap, off,
        "\"camf\":{\"bvis\":%u,\"bx\":%d,\"by\":%d,\"gy_vis\":%u,\"gy_ang\":%d,"
        "\"gy_dist\":%d,\"gb_vis\":%u,\"gb_ang\":%d,\"gb_dist\":%d},",
        (unsigned)(f.ball_front_visible ? 1 : 0), (int)f.ball_front_x_mm, (int)f.ball_front_y_mm,
        (unsigned)(f.goal_yellow_front_visible ? 1 : 0), (int)f.goal_yellow_front_angle_cd,
        (int)f.goal_yellow_front_distance_mm,
        (unsigned)(f.goal_blue_front_visible ? 1 : 0), (int)f.goal_blue_front_angle_cd,
        (int)f.goal_blue_front_distance_mm);
    if (off < 0) return -1;

    // camb — detecciones de la cámara TRASERA (pre-fusión, A1)
    off = tt_append(buf, cap, off,
        "\"camb\":{\"bvis\":%u,\"bx\":%d,\"by\":%d,\"gy_vis\":%u,\"gy_ang\":%d,"
        "\"gy_dist\":%d,\"gb_vis\":%u,\"gb_ang\":%d,\"gb_dist\":%d},",
        (unsigned)(f.ball_back_visible ? 1 : 0), (int)f.ball_back_x_mm, (int)f.ball_back_y_mm,
        (unsigned)(f.goal_yellow_back_visible ? 1 : 0), (int)f.goal_yellow_back_angle_cd,
        (int)f.goal_yellow_back_distance_mm,
        (unsigned)(f.goal_blue_back_visible ? 1 : 0), (int)f.goal_blue_back_angle_cd,
        (int)f.goal_blue_back_distance_mm);
    if (off < 0) return -1;

    // imu
    off = tt_append(buf, cap, off,
        "\"imu\":{\"hdg\":%.2f,\"left\":%.2f,\"right\":%.2f,\"disagree\":%.2f,"
        "\"lok\":%u,\"rok\":%u,\"valid\":%u,\"sok\":%u,\"sdeg\":%.2f},",
        tt_fz(f.imu_heading_deg), tt_fz(f.imu_left_deg), tt_fz(f.imu_right_deg),
        tt_fz(f.imu_disagreement_deg),
        (unsigned)(f.imu_left_ok ? 1 : 0), (unsigned)(f.imu_right_ok ? 1 : 0),
        (unsigned)(f.imu_heading_valid ? 1 : 0),
        (unsigned)(f.imu_sentinel_ok ? 1 : 0), tt_fz(f.imu_sentinel_deg));
    if (off < 0) return -1;

    // tof
    off = tt_append(buf, cap, off, "\"tof\":{\"n\":%u,\"d\":[", (unsigned)nt);
    if (off < 0) return -1;
    for (int i = 0; i < nt; ++i) {
        off = tt_append(buf, cap, off, (i == 0) ? "%u" : ",%u",
                        (unsigned)f.tof_mm[i]);
        if (off < 0) return -1;
    }
    // "z" (aditivo): zonas crudas por sensor [[16],[16],...] antes del closing }.
    off = tt_append(buf, cap, off, "],\"hc\":%u,\"min\":%u,\"z\":[",
                    (unsigned)f.hcsr04_mm, (unsigned)f.tof_min_mm);
    if (off < 0) return -1;
    for (int i = 0; i < nt; ++i) {
        off = tt_append(buf, cap, off, (i == 0) ? "[" : ",[");
        if (off < 0) return -1;
        for (int z = 0; z < TT_TOF_ZONES; ++z) {
            off = tt_append(buf, cap, off, (z == 0) ? "%u" : ",%u",
                            (unsigned)f.tof_zones[i][z]);
            if (off < 0) return -1;
        }
        off = tt_append(buf, cap, off, "]");
        if (off < 0) return -1;
    }
    off = tt_append(buf, cap, off, "]},");
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

    // base — OTOS (pose + velocidad) que llega de la DOWN (A1)
    off = tt_append(buf, cap, off,
        "\"base\":{\"pfresh\":%u,\"px\":%d,\"py\":%d,\"phdg_cd\":%d,\"pconf\":%u,"
        "\"vfresh\":%u,\"vx\":%d,\"vy\":%d,\"omega\":%d,\"slip\":%u},",
        (unsigned)(f.down_pose_fresh ? 1 : 0), (int)f.down_pose_x_mm, (int)f.down_pose_y_mm,
        (int)f.down_pose_heading_cd, (unsigned)f.down_pose_confidence,
        (unsigned)(f.down_vel_fresh ? 1 : 0), (int)f.down_vel_vx_mm_s, (int)f.down_vel_vy_mm_s,
        (int)f.down_vel_omega_cd_s, (unsigned)f.down_vel_slip);
    if (off < 0) return -1;

    // line — línea + vector de escape que llega de la DOWN (A1)
    off = tt_append(buf, cap, off,
        "\"line\":{\"fresh\":%u,\"schema\":%u,\"valid\":%u,\"angle_cd\":%d,\"escape_cd\":%d,"
        "\"pen_mm\":%u,\"cross_mm\":%d,\"present\":%u,\"sensors\":%u,\"events\":%u,"
        "\"quality\":%u,\"age_ms\":%u},",
        (unsigned)(f.down_line_fresh ? 1 : 0), (unsigned)f.down_line_schema,
        (unsigned)(f.down_line_data_valid ? 1 : 0), (int)f.down_line_angle_cd,
        (int)f.down_escape_angle_cd, (unsigned)f.down_line_penetration_mm,
        (int)f.down_line_cross_track_mm, (unsigned)(f.down_line_present ? 1 : 0),
        (unsigned)f.down_line_sensors_on, (unsigned)f.down_line_event_flags,
        (unsigned)f.down_line_quality, (unsigned)f.down_line_sample_age_ms);
    if (off < 0) return -1;

    // diag + cierre
    off = tt_append(buf, cap, off,
        "\"diag\":{\"frames_sent\":%lu,\"xval_v\":%u,\"xval_s\":%u,\"xval_ni\":%u}}\n",
        (unsigned long)f.frames_sent,
        (unsigned)f.xval_verdict, (unsigned)f.xval_score, (unsigned)f.xval_n_indep);
    if (off < 0) return -1;

    return off;
}

// ── Formato humano (texto legible, modo ENTER) ───────────────────────────────
// ASCII puro a propósito: se lee en un monitor serie crudo, sin app. Sin símbolo
// de grado ni acentos para no ensuciar terminales que no sean UTF-8. Los ángulos
// vienen en centigrados (cd) → se imprimen en grados (cd/100, prefijo 'a').
int tt_format_human(char* buf, int cap, const TopTelemetryFrame& f) {
    if (!buf || cap <= 0) return -1;
    int nt = f.num_tof;
    if (nt < 0) nt = 0;
    if (nt > TT_MAX_TOF) nt = TT_MAX_TOF;

    int off = 0;

    // L1 — encabezado (+ veredicto de salud del heading: v=SANO/SOSP/MALO, s=score, n=refs)
    off = tt_append(buf, cap, off, "[TOP] seq %lu t=%lums frames=%lu xval:v%u s%u n%u\n",
                    (unsigned long)f.seq, (unsigned long)f.t_ms,
                    (unsigned long)f.frames_sent,
                    (unsigned)f.xval_verdict, (unsigned)f.xval_score, (unsigned)f.xval_n_indep);
    if (off < 0) return -1;

    // L2 — camaras (pelota + 2 arcos)
    off = tt_append(buf, cap, off, "  CAM F:%s B:%s | ",
                    f.cam_front_ok ? "OK" : "--", f.cam_back_ok ? "OK" : "--");
    if (off < 0) return -1;
    if (f.ball_visible)
        off = tt_append(buf, cap, off, "ball(%d,%d) c%u v(%d,%d)",
                        (int)f.ball_x_mm, (int)f.ball_y_mm, (unsigned)f.ball_confidence,
                        (int)f.ball_vx_mm_s, (int)f.ball_vy_mm_s);
    else
        off = tt_append(buf, cap, off, "ball --");
    if (off < 0) return -1;
    if (f.goal_yellow_visible)
        off = tt_append(buf, cap, off, " | GY a%.1f d%d",
                        f.goal_yellow_angle_cd / 100.0, (int)f.goal_yellow_distance_mm);
    else
        off = tt_append(buf, cap, off, " | GY --");
    if (off < 0) return -1;
    if (f.goal_blue_visible)
        off = tt_append(buf, cap, off, " | GB a%.1f d%d\n",
                        f.goal_blue_angle_cd / 100.0, (int)f.goal_blue_distance_mm);
    else
        off = tt_append(buf, cap, off, " | GB --\n");
    if (off < 0) return -1;

    // L2b — detecciones POR CÁMARA (pre-fusión): front y back por separado.
    // El delta front↔back delata la pelota fantasma del promedio fusionado.
    off = tt_append(buf, cap, off, "  CAMF ");
    if (off < 0) return -1;
    if (f.ball_front_visible)
        off = tt_append(buf, cap, off, "ball(%d,%d)", (int)f.ball_front_x_mm, (int)f.ball_front_y_mm);
    else
        off = tt_append(buf, cap, off, "ball --");
    if (off < 0) return -1;
    if (f.goal_yellow_front_visible)
        off = tt_append(buf, cap, off, " GY a%.1f d%d", f.goal_yellow_front_angle_cd / 100.0, (int)f.goal_yellow_front_distance_mm);
    else
        off = tt_append(buf, cap, off, " GY --");
    if (off < 0) return -1;
    if (f.goal_blue_front_visible)
        off = tt_append(buf, cap, off, " GB a%.1f d%d\n", f.goal_blue_front_angle_cd / 100.0, (int)f.goal_blue_front_distance_mm);
    else
        off = tt_append(buf, cap, off, " GB --\n");
    if (off < 0) return -1;

    off = tt_append(buf, cap, off, "  CAMB ");
    if (off < 0) return -1;
    if (f.ball_back_visible)
        off = tt_append(buf, cap, off, "ball(%d,%d)", (int)f.ball_back_x_mm, (int)f.ball_back_y_mm);
    else
        off = tt_append(buf, cap, off, "ball --");
    if (off < 0) return -1;
    if (f.goal_yellow_back_visible)
        off = tt_append(buf, cap, off, " GY a%.1f d%d", f.goal_yellow_back_angle_cd / 100.0, (int)f.goal_yellow_back_distance_mm);
    else
        off = tt_append(buf, cap, off, " GY --");
    if (off < 0) return -1;
    if (f.goal_blue_back_visible)
        off = tt_append(buf, cap, off, " GB a%.1f d%d\n", f.goal_blue_back_angle_cd / 100.0, (int)f.goal_blue_back_distance_mm);
    else
        off = tt_append(buf, cap, off, " GB --\n");
    if (off < 0) return -1;

    // L3 — IMU (heading fusionado + por sensor)
    off = tt_append(buf, cap, off,
        "  IMU hdg %.2f %s L%.2f R%.2f dis%.2f (L:%s R:%s) cent:%s %.2f\n",
        tt_fz(f.imu_heading_deg), f.imu_heading_valid ? "VALID" : "INVALID",
        tt_fz(f.imu_left_deg), tt_fz(f.imu_right_deg), tt_fz(f.imu_disagreement_deg),
        f.imu_left_ok ? "ok" : "--", f.imu_right_ok ? "ok" : "--",
        f.imu_sentinel_ok ? "@1Hz" : "--", tt_fz(f.imu_sentinel_deg));
    if (off < 0) return -1;

    // L4 — ToF + HC-SR04
    off = tt_append(buf, cap, off, "  ToF n=%d [", nt);
    if (off < 0) return -1;
    for (int i = 0; i < nt; ++i) {
        if (f.tof_mm[i] == TT_TOF_NO_READING)
            off = tt_append(buf, cap, off, (i == 0) ? "--" : ",--");
        else
            off = tt_append(buf, cap, off, (i == 0) ? "%u" : ",%u", (unsigned)f.tof_mm[i]);
        if (off < 0) return -1;
    }
    off = tt_append(buf, cap, off, "] hc=");
    if (off < 0) return -1;
    if (f.hcsr04_mm == TT_TOF_NO_READING) off = tt_append(buf, cap, off, "--");
    else                                  off = tt_append(buf, cap, off, "%u", (unsigned)f.hcsr04_mm);
    if (off < 0) return -1;
    if (f.tof_min_mm == TT_TOF_NO_READING) off = tt_append(buf, cap, off, " min=-- mm\n");
    else                                   off = tt_append(buf, cap, off, " min=%u mm\n", (unsigned)f.tof_min_mm);
    if (off < 0) return -1;

    // L5 — WorldSnapshot fusionado (lo que viaja a CENTRAL)
    if (!f.snap_valid) {
        off = tt_append(buf, cap, off, "  SNAP (sin snapshot todavia)\n");
        if (off < 0) return -1;
    } else {
        off = tt_append(buf, cap, off, "  SNAP x%d y%d hdg%.2f c%u | ",
                        (int)f.snap_my_x_mm, (int)f.snap_my_y_mm,
                        f.snap_my_heading_cd / 100.0, (unsigned)f.snap_my_confidence);
        if (off < 0) return -1;
        if (f.snap_ball_visible)
            off = tt_append(buf, cap, off, "ball(%d,%d) c%u",
                            (int)f.snap_ball_x_mm, (int)f.snap_ball_y_mm,
                            (unsigned)f.snap_ball_confidence);
        else
            off = tt_append(buf, cap, off, "ball --");
        if (off < 0) return -1;
        off = tt_append(buf, cap, off,
            " | opp a%.1f d%d %s | own %s | obst%u ref%u flags0x%02X\n",
            f.snap_goal_opp_angle_cd / 100.0, (int)f.snap_goal_opp_distance_mm,
            f.snap_goal_opp_visible ? "VIS" : "--",
            f.snap_goal_own_visible ? "VIS" : "--",
            (unsigned)f.snap_min_obstacle_mm, (unsigned)f.snap_referee_cmd,
            (unsigned)f.snap_flags);
        if (off < 0) return -1;
    }

    // L6 — BASE: OTOS (pose + velocidad) que llega de la DOWN. Si no fresh, STALE
    // (no muestro ceros como si fueran posición real).
    off = tt_append(buf, cap, off, "  BASE pose ");
    if (off < 0) return -1;
    if (f.down_pose_fresh)
        off = tt_append(buf, cap, off, "x%d y%d hdg%.2f c%u",
                        (int)f.down_pose_x_mm, (int)f.down_pose_y_mm,
                        f.down_pose_heading_cd / 100.0, (unsigned)f.down_pose_confidence);
    else
        off = tt_append(buf, cap, off, "STALE");
    if (off < 0) return -1;
    off = tt_append(buf, cap, off, " | vel ");
    if (off < 0) return -1;
    if (f.down_vel_fresh)
        off = tt_append(buf, cap, off, "vx%d vy%d w%.1f slip%u\n",
                        (int)f.down_vel_vx_mm_s, (int)f.down_vel_vy_mm_s,
                        f.down_vel_omega_cd_s / 100.0, (unsigned)f.down_vel_slip);
    else
        off = tt_append(buf, cap, off, "STALE\n");
    if (off < 0) return -1;

    // L7 — LINE: línea + vector de escape de la DOWN. INVALID = compuerta maestra;
    // '--' en los campos en sentinela N/A.
    off = tt_append(buf, cap, off, "  LINE %s%s present%u sensors%u",
                    f.down_line_fresh ? "FRESH" : "STALE",
                    f.down_line_data_valid ? "" : " INVALID",
                    (unsigned)(f.down_line_present ? 1 : 0), (unsigned)f.down_line_sensors_on);
    if (off < 0) return -1;
    if (f.down_line_angle_cd == TT_NA_I16) off = tt_append(buf, cap, off, " | angle --");
    else                                   off = tt_append(buf, cap, off, " | angle a%.1f", f.down_line_angle_cd / 100.0);
    if (off < 0) return -1;
    if (f.down_escape_angle_cd == TT_NA_I16) off = tt_append(buf, cap, off, " esc --");
    else                                     off = tt_append(buf, cap, off, " esc a%.1f", f.down_escape_angle_cd / 100.0);
    if (off < 0) return -1;
    if (f.down_line_penetration_mm == TT_NA_U16) off = tt_append(buf, cap, off, " pen --");
    else                                         off = tt_append(buf, cap, off, " pen%umm", (unsigned)f.down_line_penetration_mm);
    if (off < 0) return -1;
    off = tt_append(buf, cap, off, " q%u ev0x%02X\n",
                    (unsigned)f.down_line_quality, (unsigned)f.down_line_event_flags);
    if (off < 0) return -1;

    return off;
}

// ── Parser de comandos ───────────────────────────────────────────────────────
namespace {

constexpr int TT_TOK_MAX = 4;   // A2.1: "TOF <n> POS LEFT" son 4 tokens
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
    TtCommand out{TtCmd::NONE, 0, 0};
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
    } else if (tt_eq(tok[0], "CAM") && nt >= 3) {        // CAM F|B ON|OFF
        const bool on = tt_eq(tok[2], "ON"), off2 = tt_eq(tok[2], "OFF");
        if (on || off2) {
            if (tt_eq(tok[1], "F"))      out.cmd = on ? TtCmd::CAM_F_ON : TtCmd::CAM_F_OFF;
            else if (tt_eq(tok[1], "B")) out.cmd = on ? TtCmd::CAM_B_ON : TtCmd::CAM_B_OFF;
        }
    } else if (tt_eq(tok[0], "BNO") && nt >= 3) {        // BNO L|R ON|OFF
        const bool on = tt_eq(tok[2], "ON"), off2 = tt_eq(tok[2], "OFF");
        if (on || off2) {
            if (tt_eq(tok[1], "L"))      out.cmd = on ? TtCmd::BNO_L_ON : TtCmd::BNO_L_OFF;
            else if (tt_eq(tok[1], "R")) out.cmd = on ? TtCmd::BNO_R_ON : TtCmd::BNO_R_OFF;
        }
    } else if (tt_eq(tok[0], "US") && nt >= 2) {         // US ON|OFF
        if (tt_eq(tok[1], "ON"))       out.cmd = TtCmd::US_ON;
        else if (tt_eq(tok[1], "OFF")) out.cmd = TtCmd::US_OFF;
    } else if (tt_eq(tok[0], "TOF") && nt >= 3) {        // TOF <n> ON|OFF | POS <dir>
        char* end = nullptr;
        const long n = strtol(tok[1], &end, 10);
        if (end && *end == '\0' && n >= 0 && n < 64) {
            if (tt_eq(tok[2], "ON"))  { out.cmd = TtCmd::TOF_SET_ENABLED; out.arg = (int32_t)n; out.arg2 = 1; }
            else if (tt_eq(tok[2], "OFF")) { out.cmd = TtCmd::TOF_SET_ENABLED; out.arg = (int32_t)n; out.arg2 = 0; }
            else if (tt_eq(tok[2], "POS") && nt >= 4) {
                int32_t bearing = -1;
                if (tt_eq(tok[3], "FRONT"))      bearing = 0;
                else if (tt_eq(tok[3], "RIGHT")) bearing = 90;
                else if (tt_eq(tok[3], "BACK"))  bearing = 180;
                else if (tt_eq(tok[3], "LEFT"))  bearing = 270;
                if (bearing >= 0) { out.cmd = TtCmd::TOF_SET_POS; out.arg = (int32_t)n; out.arg2 = bearing; }
            }
        }
    } else if (tt_eq(tok[0], "CFG") && nt >= 2) {        // CFG SAVE|LOAD|RESET
        if (tt_eq(tok[1], "SAVE"))       out.cmd = TtCmd::CFG_SAVE;
        else if (tt_eq(tok[1], "LOAD"))  out.cmd = TtCmd::CFG_LOAD;
        else if (tt_eq(tok[1], "RESET")) out.cmd = TtCmd::CFG_RESET;
    }
    return out;
}

}  // namespace iitasoccer
