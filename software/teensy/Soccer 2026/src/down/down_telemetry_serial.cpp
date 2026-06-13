// down_telemetry_serial.cpp — Glue Arduino de la telemetría USB de DOWN.
//
// DOS MODOS de compilación (excluyentes en la práctica, mismo código):
//   • -DDOWN_DEBUG_TELEMETRY (envs *_debug_telemetry, banco): stream ON desde
//     el boot a 20 Hz — el comportamiento histórico.
//   • -DDOWN_USB_MONITOR (envs de COMPETENCIA down/down_robot2, pedido María
//     2026-06-12): el monitor viaja EN el binario de partido pero DORMIDO.
//     Silencio total por USB hasta que la app manda un comando (STREAM ON /
//     PING); ahí streamea, y si el host se calla DOWN_MONITOR_HOST_TIMEOUT_MS
//     (la app manda PING cada 1 s como latido) el stream se apaga SOLO →
//     desenchufar el cable = modo partido, sin reflashear nada.
// Sin ninguno de los dos flags: traducción VACÍA (binario sin un byte de esto).
//
// El módulo PURO (serialización JSON + parseo de comandos) vive en
// src/shared/telemetry_down.{h,cpp} (host-testeado). Acá solo está el pegamento
// con el hardware: leer line_ring/otos, escribir Serial, ejecutar comandos.
//
// Contrato: docs/firmware/TELEMETRIA-DOWN.md.

#include "down_telemetry_serial.h"

#if defined(DOWN_DEBUG_TELEMETRY) || defined(DOWN_USB_MONITOR)

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
// En modo USB_MONITOR (competencia) el stream arranca APAGADO y solo habla si
// el host habló primero; en modo DEBUG_TELEMETRY (banco) arranca prendido.
#ifdef DOWN_USB_MONITOR
constexpr bool     kStreamDefaultOn          = false;
constexpr uint32_t DOWN_MONITOR_HOST_TIMEOUT_MS = 3000;  // host mudo → stream OFF
#else
constexpr bool     kStreamDefaultOn          = true;
#endif
bool          g_stream_on    = kStreamDefaultOn;
uint32_t      g_interval_ms  = 50;     // 20 Hz por default (1000/20)
uint32_t      g_seq          = 0;
elapsedMillis g_since_emit;            // gobierna la cadencia de TX
uint32_t      g_last_host_rx_ms = 0;   // último comando válido del host (latido)

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

    // ── Sintonía fina (schema v3): umbral efectivo + habilitado + sensibilidad ──
    // Sale del DownModel (la verdad de detección), no del line_ring.
    comm_central_get_tuning(f.threshold, &f.enabled_bits, &f.global_sens,
                            f.persensor_sens, NUM_LINE_SENSORS);

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
    // Lecturas por-OTOS sin fusionar (schema v2, getters gateados).
    f.otos_left_x_mm         = otos_get_left_x_mm();
    f.otos_left_y_mm         = otos_get_left_y_mm();
    f.otos_left_heading_deg  = otos_get_left_heading_deg();
    f.otos_right_x_mm        = otos_get_right_x_mm();
    f.otos_right_y_mm        = otos_get_right_y_mm();
    f.otos_right_heading_deg = otos_get_right_heading_deg();

    // ── Diagnóstico ──
    f.lifted          = line_ring_is_lifted() ? 1 : 0;
    f.line_tick_count = line_ring_get_tick_count();
    f.line_tick_us    = line_ring_get_last_tick_us();

    static char buf[1600];  // v3 sumó threshold[]/persensor_sens[] (~+320 B)
    const int n = td_serialize_jsonl(buf, sizeof(buf), f);
    if (n > 0) {
        Serial.write(reinterpret_cast<const uint8_t*>(buf), n);
    }
}

// Ejecuta un comando ya parseado.
void dispatch(const TdCommand& c) {
    // Cualquier comando RECONOCIDO cuenta como latido del host (la app manda
    // PING cada 1 s): renueva la ventana del modo monitor.
    if (c.cmd != TdCmd::NONE && c.cmd != TdCmd::UNKNOWN) {
        g_last_host_rx_ms = millis();
        if (g_last_host_rx_ms == 0) g_last_host_rx_ms = 1;  // 0 = "nunca" (sentinel)
    }
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
            // Fix TASK-306: re-derivar el DownModel — sin esto el LineStatusV2
            // hacia CENTRAL seguía con la calib VIEJA hasta el reboot.
            comm_central_invalidate_calib();
            Serial.println("[DOWN] verde capturado (calib aplicada en vivo)");
            break;

        case TdCmd::CAL_WHITE:
            line_ring_calibrate_white();
            comm_central_invalidate_calib();   // fix TASK-306 (ídem carpet)
            Serial.println("[DOWN] blanco capturado (calib aplicada en vivo)");
            break;

        case TdCmd::CAL_AUTO_ON:
            g_autocal_on = true;
            for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
                g_autocal_min[i] = 0xFFFF;
                g_autocal_max[i] = 0;
            }
            break;

        case TdCmd::CAL_AUTO_OFF: {
            g_autocal_on = false;
            // SANITY (TASK-306): si la auto-calib no capturó nada (OFF inmediato
            // sin pasear el robot: min=0xFFFF/max=0 → calib INVERTIDA que
            // ec_save persistiría), rechazar en vez de aplicar basura.
            bool degenerada = false;
            for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
                if (g_autocal_min[i] >= g_autocal_max[i]) { degenerada = true; break; }
            }
            if (degenerada) {
                Serial.println("[DOWN] ERROR: auto-calib sin datos (pasea el robot por verde Y blanco antes del OFF) — NO aplicada");
                break;
            }
            // carpet = min capturado, white = max capturado.
            line_ring_set_calibration(g_autocal_min, g_autocal_max, NUM_LINE_SENSORS);
            comm_central_invalidate_calib();   // fix TASK-306
            Serial.println("[DOWN] auto-calib aplicada (min/max capturados)");
            break;
        }

        case TdCmd::CAL_SAVE: {
            // Persistir calib (carpet/white) + sintonía (enabled/sensitivity por
            // sensor) + global_sens. comm_central deriva el carpet/white fresco del
            // line_ring preservando las perillas. ACK/NAK explícito (TASK-306).
            if (comm_central_save_calib_and_tuning()) {
                Serial.println("[DOWN] calib + sintonia persistida en EEPROM");
            } else {
                Serial.println("[DOWN] ERROR: fallo guardar calib en EEPROM");
            }
            break;
        }

        case TdCmd::CAL_LOAD: {
            // Recarga en vivo: calib + sintonía → DownModel; carpet/white → line_ring.
            if (comm_central_reload_from_eeprom_live()) {
                Serial.println("[DOWN] calib + sintonia cargada de EEPROM (en vivo)");
            } else {
                Serial.println("[DOWN] ERROR: EEPROM sin calib valida");
            }
            break;
        }

        case TdCmd::OTOS_RESET:
            otos_reset();
            break;

        // ── Sintonía fina (schema v3) ──
        case TdCmd::SENS_GLOBAL:
            comm_central_set_global_sens(c.arg);
            Serial.print("[DOWN] sensibilidad global = ");
            Serial.println(c.arg);
            break;
        case TdCmd::SENS_SET:
            comm_central_set_sensor_sens(c.arg, c.arg2);
            break;
        case TdCmd::SENSOR_ENABLE:
            comm_central_set_sensor_enabled(c.arg, true);
            break;
        case TdCmd::SENSOR_DISABLE:
            comm_central_set_sensor_enabled(c.arg, false);
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
    g_stream_on   = kStreamDefaultOn;
    g_interval_ms = 50;   // 20 Hz
    g_seq         = 0;
    g_autocal_on  = false;
    g_rx_len      = 0;
    g_since_emit  = 0;
    g_last_host_rx_ms = 0;
#ifdef DOWN_USB_MONITOR
    // Una sola línea de boot (Teensy la descarta si no hay host conectado);
    // después, SILENCIO hasta que la app hable.
    Serial.println("[DOWN-MONITOR] dormido — esperando a la app (STREAM ON / PING)");
#else
    Serial.println("[DOWN-TELEM] v1 ready");
#endif
}

void down_telemetry_tick() {
    // RX: comandos del host (no bloquea).
    pump_rx();

#ifdef DOWN_USB_MONITOR
    // APAGADO AUTOMÁTICO (modo competencia): si el host se calló (la app manda
    // PING cada 1 s; desenchufar el cable o cerrar la app corta ese latido),
    // volver al silencio. El robot queda en modo partido sin tocar nada.
    if (g_stream_on && g_last_host_rx_ms != 0 &&
        (millis() - g_last_host_rx_ms) > DOWN_MONITOR_HOST_TIMEOUT_MS) {
        g_stream_on   = false;
        g_autocal_on  = false;   // una auto-calib a medias no debe quedar armada
        g_last_host_rx_ms = 0;
    }
#endif

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

#endif  // DOWN_DEBUG_TELEMETRY || DOWN_USB_MONITOR
