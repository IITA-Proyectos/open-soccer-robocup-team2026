// telemetry_down.cpp — implementación pura de telemetry_down.h.
// Sin Arduino: solo libc (stdio/stdarg/ctype/stdlib/string). Host-testeable.

#include "telemetry_down.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

namespace iitasoccer {

namespace {

// Append printf-style a buf[off..cap). Devuelve el nuevo offset, o -1 si no
// entra (overflow / truncado) o si los argumentos son inválidos. En éxito el
// buffer queda NUL-terminado por vsnprintf.
__attribute__((format(printf, 4, 5)))
int td_append(char* buf, int cap, int off, const char* fmt, ...) {
    if (!buf || off < 0 || off >= cap) return -1;
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf + off, (size_t)(cap - off), fmt, ap);
    va_end(ap);
    if (n < 0 || n >= cap - off) return -1;  // error o truncado
    return off + n;
}

// Normaliza -0.0 → 0.0 para que la salida sea estable entre plataformas.
inline double td_fz(float v) {
    double d = (double)v;
    if (d == 0.0) d = 0.0;  // colapsa -0.0
    return d;
}

// Emite un array JSON de uint16 (raw/carpet/white_cal). off entrante apunta
// justo después de la apertura del array. Devuelve offset o -1.
int td_append_u16_array(char* buf, int cap, int off,
                        const uint16_t* a, int n) {
    off = td_append(buf, cap, off, "[");
    if (off < 0) return -1;
    for (int i = 0; i < n; ++i) {
        off = td_append(buf, cap, off, (i == 0) ? "%u" : ",%u",
                        (unsigned)a[i]);
        if (off < 0) return -1;
    }
    return td_append(buf, cap, off, "]");
}

}  // namespace

void td_frame_init(TelemetryDownFrame& f, uint8_t num_sensors) {
    memset(&f, 0, sizeof(f));
    f.schema = TELEMETRY_DOWN_SCHEMA;
    f.line_schema = 2;  // LSV2_SCHEMA actual (informativo)
    if (num_sensors > TD_MAX_SENSORS) num_sensors = TD_MAX_SENSORS;
    f.num_sensors = num_sensors;
    // N/A sensatos para que un frame "vacío" no mienta con datos de línea.
    f.line_angle_cd   = TD_NA_I16;
    f.escape_angle_cd = TD_NA_I16;
    f.cross_track_mm  = TD_NA_I16;
    f.penetration_mm  = TD_NA_U16;
}

int td_serialize_jsonl(char* buf, int cap, const TelemetryDownFrame& f) {
    if (!buf || cap <= 0) return -1;
    int n = f.num_sensors;
    if (n < 0) n = 0;
    if (n > TD_MAX_SENSORS) n = TD_MAX_SENSORS;

    int off = 0;

    // Cabecera + ring abierto.
    off = td_append(buf, cap, off,
        "{\"v\":%u,\"seq\":%lu,\"t_ms\":%lu,\"ring\":{\"n\":%u,\"raw\":",
        (unsigned)TELEMETRY_DOWN_SCHEMA,
        (unsigned long)f.seq, (unsigned long)f.t_ms, (unsigned)n);
    if (off < 0) return -1;

    off = td_append_u16_array(buf, cap, off, f.raw, n);
    if (off < 0) return -1;

    off = td_append(buf, cap, off, ",\"white\":%lu,\"carpet\":",
                    (unsigned long)f.white_bits);
    if (off < 0) return -1;
    off = td_append_u16_array(buf, cap, off, f.carpet, n);
    if (off < 0) return -1;

    off = td_append(buf, cap, off, ",\"white_cal\":");
    if (off < 0) return -1;
    off = td_append_u16_array(buf, cap, off, f.white_cal, n);
    if (off < 0) return -1;

    // Cierre ring + objeto line.
    off = td_append(buf, cap, off,
        "},\"line\":{\"schema\":%u,\"valid\":%u,\"present\":%u,"
        "\"angle_cd\":%d,\"escape_cd\":%d,\"pen_mm\":%u,\"xtrack_mm\":%d,"
        "\"non\":%u,\"flags\":%u,\"q\":%u,\"age_ms\":%u},",
        (unsigned)f.line_schema, (unsigned)(f.data_valid ? 1 : 0),
        (unsigned)(f.line_present ? 1 : 0),
        (int)f.line_angle_cd, (int)f.escape_angle_cd,
        (unsigned)f.penetration_mm, (int)f.cross_track_mm,
        (unsigned)f.sensors_on_line, (unsigned)f.event_flags,
        (unsigned)f.quality, (unsigned)f.sample_age_ms);
    if (off < 0) return -1;

    // Objeto otos.
    off = td_append(buf, cap, off,
        "\"otos\":{\"n\":%u,\"lok\":%u,\"rok\":%u,"
        "\"x\":%.2f,\"y\":%.2f,\"hdg\":%.2f,"
        "\"vx\":%.2f,\"vy\":%.2f,\"w\":%.3f,\"slip\":%.2f},",
        (unsigned)f.otos_count, (unsigned)(f.otos_left_ok ? 1 : 0),
        (unsigned)(f.otos_right_ok ? 1 : 0),
        td_fz(f.otos_x_mm), td_fz(f.otos_y_mm), td_fz(f.otos_heading_deg),
        td_fz(f.otos_vx_mm_s), td_fz(f.otos_vy_mm_s),
        td_fz(f.otos_omega_rad_s), td_fz(f.otos_slip_mm_s));
    if (off < 0) return -1;

    // Objeto diag + cierre + newline.
    off = td_append(buf, cap, off,
        "\"diag\":{\"lifted\":%u,\"ltick\":%lu,\"ltick_us\":%lu}}\n",
        (unsigned)(f.lifted ? 1 : 0),
        (unsigned long)f.line_tick_count, (unsigned long)f.line_tick_us);
    if (off < 0) return -1;

    return off;
}

// ── Parser de comandos ───────────────────────────────────────────────────────
namespace {

constexpr int TD_TOK_MAX  = 4;   // tokens que miramos
constexpr int TD_TOK_LEN  = 12;  // largo máx por token (+ NUL)

// Tokeniza `s[0..len)` en hasta TD_TOK_MAX tokens UPPERCASE NUL-terminados.
// Devuelve la cantidad de tokens encontrados.
int td_tokenize(const char* s, int len, char tok[TD_TOK_MAX][TD_TOK_LEN]) {
    int nt = 0;
    int i = 0;
    while (i < len && nt < TD_TOK_MAX) {
        // saltar separadores (espacios, tabs, CR/LF, comas)
        while (i < len && (isspace((unsigned char)s[i]) || s[i] == ',')) ++i;
        if (i >= len) break;
        int k = 0;
        while (i < len && !isspace((unsigned char)s[i]) && s[i] != ',') {
            if (k < TD_TOK_LEN - 1) {
                tok[nt][k++] = (char)toupper((unsigned char)s[i]);
            }
            ++i;
        }
        tok[nt][k] = '\0';
        ++nt;
    }
    return nt;
}

inline bool td_eq(const char* a, const char* b) { return strcmp(a, b) == 0; }

}  // namespace

TdCommand td_parse_command(const char* s, int len) {
    TdCommand out{TdCmd::NONE, 0};
    if (!s || len <= 0) return out;

    char tok[TD_TOK_MAX][TD_TOK_LEN];
    const int nt = td_tokenize(s, len, tok);
    if (nt == 0) return out;  // NONE

    out.cmd = TdCmd::UNKNOWN;

    if (td_eq(tok[0], "PING")) {
        out.cmd = TdCmd::PING;
    } else if (td_eq(tok[0], "STREAM") && nt >= 2) {
        if (td_eq(tok[1], "ON"))  out.cmd = TdCmd::STREAM_ON;
        else if (td_eq(tok[1], "OFF")) out.cmd = TdCmd::STREAM_OFF;
    } else if (td_eq(tok[0], "RATE") && nt >= 2) {
        char* end = nullptr;
        const long hz = strtol(tok[1], &end, 10);
        if (end && *end == '\0' && hz > 0) {
            out.cmd = TdCmd::SET_RATE;
            out.arg = (int32_t)hz;
        }
    } else if (td_eq(tok[0], "OTOS") && nt >= 2 && td_eq(tok[1], "RESET")) {
        out.cmd = TdCmd::OTOS_RESET;
    } else if (td_eq(tok[0], "CAL") && nt >= 2) {
        if (td_eq(tok[1], "CARPET"))      out.cmd = TdCmd::CAL_CARPET;
        else if (td_eq(tok[1], "WHITE"))  out.cmd = TdCmd::CAL_WHITE;
        else if (td_eq(tok[1], "SAVE"))   out.cmd = TdCmd::CAL_SAVE;
        else if (td_eq(tok[1], "LOAD"))   out.cmd = TdCmd::CAL_LOAD;
        else if (td_eq(tok[1], "AUTO") && nt >= 3) {
            if (td_eq(tok[2], "ON"))       out.cmd = TdCmd::CAL_AUTO_ON;
            else if (td_eq(tok[2], "OFF")) out.cmd = TdCmd::CAL_AUTO_OFF;
        }
    }
    return out;
}

}  // namespace iitasoccer
