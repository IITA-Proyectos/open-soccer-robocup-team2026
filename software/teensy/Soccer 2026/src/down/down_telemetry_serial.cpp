// down_telemetry_serial.cpp — Glue Arduino de la telemetría USB de DOWN.
//
// TODO el cuerpo está dentro de #ifdef DOWN_DEBUG_TELEMETRY: con el flag OFF
// (envs de competencia down / down_lean / down_wdt) este archivo compila a una
// traducción VACÍA → no aporta ni un byte al binario de competencia.
//
// El módulo PURO (serialización JSON + parseo de comandos) vive en
// src/shared/telemetry_down.{h,cpp} (host-testeado). Acá solo está el pegamento
// con el hardware: leer line_ring/otos, escribir Serial, ejecutar comandos.
//
// Contrato: docs/firmware/TELEMETRIA-DOWN.md.

#include "down_telemetry_serial.h"

#ifdef DOWN_DEBUG_TELEMETRY

#include <Arduino.h>
#include "telemetry_down.h"
#include "config_down.h"
#include "line_ring.h"
#include "otos.h"
#include "eeprom_calib.h"
#include "line_calib.h"
#include "comm_central.h"
#include "types.h"

namespace iitasoccer {

namespace {

// ── Estado interno del stream ────────────────────────────────────────────────
bool          g_stream_on    = true;   // stream ON por default
uint32_t      g_interval_ms  = 50;     // 20 Hz por default (1000/20)
uint32_t      g_seq          = 0;
elapsedMillis g_since_emit;            // gobierna la cadencia de TX

// ── Auto-calibración (captura min/max por sensor mientras se mueve el robot) ──
bool     g_autocal_on = false;
uint16_t g_autocal_min[TD_MAX_SENSORS];
uint16_t g_autocal_max[TD_MAX_SENSORS];

// ── Buffer de RX (acumula una línea de comando hasta '\n') ──
char g_rx_line[64];
int  g_rx_len = 0;

// Arma y emite UN frame de telemetría leyendo el estado vivo de DOWN.
void emit_frame() {
    TelemetryDownFrame f;
    td_frame_init(f, static_cast<uint8_t>(NUM_LINE_SENSORS));

    f.seq  = g_seq++;
    f.t_ms = millis();

    // ── Anillo de luz (line_ring) ──
    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        const uint8_t idx = static_cast<uint8_t>(i);
        f.raw[i] = line_ring_get_raw(idx);
        if (line_ring_get_white(idx)) {
            f.white_bits |= (1u << i);
        }
        f.carpet[i]    = line_ring_get_carpet_avg(idx);
        f.white_cal[i] = line_ring_get_white_avg(idx);
    }

    // ── Línea procesada: el ÚLTIMO LineStatusV2 que se difundió a CENTRAL ──
    LineStatusV2 lsv2;
    if (comm_central_get_last_line_status(lsv2)) {
        f.line_schema     = lsv2.schema_version;
        f.data_valid      = lsv2.data_valid;
        f.line_present    = lsv2.line_present;
        f.line_angle_cd   = lsv2.line_angle_centideg;
        f.escape_angle_cd = lsv2.escape_angle_centideg;
        f.penetration_mm  = lsv2.penetration_mm;
        f.cross_track_mm  = lsv2.cross_track_mm;
        f.sensors_on_line = lsv2.sensors_on_line;
        f.event_flags     = lsv2.event_flags;
        f.quality         = lsv2.quality;
        f.sample_age_ms   = lsv2.sample_age_ms;
    }
    // Si todavía no se envió ningún LineStatusV2, dejamos los N/A de td_frame_init.

    // ── Odometría (OTOS fusionado) ──
    f.otos_count       = static_cast<uint8_t>(NUM_OTOS);
    f.otos_left_ok     = otos_is_left_ready()  ? 1 : 0;
    f.otos_right_ok    = otos_is_right_ready() ? 1 : 0;
    f.otos_x_mm        = otos_get_x_mm();
    f.otos_y_mm        = otos_get_y_mm();
    f.otos_heading_deg = otos_get_heading_deg();
    f.otos_vx_mm_s     = otos_get_vx_mm_s();
    f.otos_vy_mm_s     = otos_get_vy_mm_s();
    f.otos_omega_rad_s = otos_get_omega_rad_s();
    f.otos_slip_mm_s   = otos_get_slip_estimate();

    // ── Diagnóstico ──
    f.lifted          = line_ring_is_lifted() ? 1 : 0;
    f.line_tick_count = line_ring_get_tick_count();
    f.line_tick_us    = line_ring_get_last_tick_us();

    static char buf[1100];
    const int n = td_serialize_jsonl(buf, sizeof(buf), f);
    if (n > 0) {
        Serial.write(reinterpret_cast<const uint8_t*>(buf), n);
    }
}

// Ejecuta un comando ya parseado.
void dispatch(const TdCommand& c) {
    switch (c.cmd) {
        case TdCmd::PING:
            // Ack de enlace: un frame inmediato (lleva el estado completo).
            emit_frame();
            break;

        case TdCmd::STREAM_ON:
            g_stream_on = true;
            break;

        case TdCmd::STREAM_OFF:
            g_stream_on = false;
            break;

        case TdCmd::SET_RATE: {
            // arg = Hz. Clampear a [1, 200] e invertir a período en ms.
            int32_t hz = c.arg;
            if (hz < 1)   hz = 1;
            if (hz > 200) hz = 200;
            g_interval_ms = static_cast<uint32_t>(1000 / hz);
            break;
        }

        case TdCmd::CAL_CARPET:
            line_ring_calibrate_carpet();
            break;

        case TdCmd::CAL_WHITE:
            line_ring_calibrate_white();
            break;

        case TdCmd::CAL_AUTO_ON:
            g_autocal_on = true;
            for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
                g_autocal_min[i] = 0xFFFF;
                g_autocal_max[i] = 0;
            }
            break;

        case TdCmd::CAL_AUTO_OFF:
            g_autocal_on = false;
            // carpet = min capturado, white = max capturado.
            line_ring_set_calibration(g_autocal_min, g_autocal_max, NUM_LINE_SENSORS);
            break;

        case TdCmd::CAL_SAVE: {
            // Derivar SensorCalib de los promedios actuales del line_ring y persistir.
            SensorCalib cal[NUM_LINE_SENSORS];
            for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
                const uint8_t idx = static_cast<uint8_t>(i);
                lc_set_static(cal[i],
                              line_ring_get_carpet_avg(idx),
                              line_ring_get_white_avg(idx));
            }
            ec_save_calibration(cal, NUM_LINE_SENSORS);
            break;
        }

        case TdCmd::CAL_LOAD: {
            SensorCalib cal[NUM_LINE_SENSORS];
            if (ec_load_calibration(cal, NUM_LINE_SENSORS)) {
                uint16_t carpet[NUM_LINE_SENSORS];
                uint16_t white[NUM_LINE_SENSORS];
                for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
                    carpet[i] = cal[i].carpet;
                    white[i]  = cal[i].white;
                }
                line_ring_set_calibration(carpet, white, NUM_LINE_SENSORS);
            }
            break;
        }

        case TdCmd::OTOS_RESET:
            otos_reset();
            break;

        case TdCmd::NONE:
        case TdCmd::UNKNOWN:
        default:
            // Línea vacía / comando no reconocido → ignorar.
            break;
    }
}

// Drena el USB (Serial) sin bloquear, arma líneas y despacha comandos.
void pump_rx() {
    while (Serial.available() > 0) {
        const char ch = static_cast<char>(Serial.read());
        if (ch == '\n') {
            dispatch(td_parse_command(g_rx_line, g_rx_len));
            g_rx_len = 0;
        } else if (ch != '\r') {
            if (g_rx_len < static_cast<int>(sizeof(g_rx_line))) {
                g_rx_line[g_rx_len++] = ch;
            } else {
                // Línea demasiado larga (basura): descartar para no trabarse.
                g_rx_len = 0;
            }
        }
    }
}

}  // namespace

void down_telemetry_init() {
    g_stream_on   = true;
    g_interval_ms = 50;   // 20 Hz
    g_seq         = 0;
    g_autocal_on  = false;
    g_rx_len      = 0;
    g_since_emit  = 0;
    Serial.println("[DOWN-TELEM] v1 ready");
}

void down_telemetry_tick() {
    // RX: comandos del host (no bloquea).
    pump_rx();

    // Auto-calibración: capturar min/max por sensor mientras se mueve el robot.
    if (g_autocal_on) {
        for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
            const uint16_t raw = line_ring_get_raw(static_cast<uint8_t>(i));
            if (raw < g_autocal_min[i]) g_autocal_min[i] = raw;
            if (raw > g_autocal_max[i]) g_autocal_max[i] = raw;
        }
    }

    // TX: emitir un frame si el stream está ON y ya pasó el intervalo.
    if (g_stream_on && g_since_emit >= g_interval_ms) {
        g_since_emit = 0;
        emit_frame();
    }
}

}  // namespace iitasoccer

#endif  // DOWN_DEBUG_TELEMETRY
