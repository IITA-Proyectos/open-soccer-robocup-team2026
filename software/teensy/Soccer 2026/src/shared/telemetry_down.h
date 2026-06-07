// telemetry_down.h — Telemetría USB de la placa DOWN (modo DEBUG, gateado).
//
// Módulo PURO host-testeable (solo <stdint.h>, SIN Arduino), igual que
// proto.cpp / calib_storage.cpp / down_model.cpp. Su único trabajo es:
//   1. Serializar un SNAPSHOT del estado de DOWN (anillo de 32 sensores de luz +
//      LineStatusV2 que viaja a CENTRAL + odometría OTOS) a UNA línea JSON
//      (JSON Lines) lista para mandar por el USB CDC del Teensy.
//   2. Parsear los COMANDOS de texto que el host (app de PC) manda de vuelta
//      (calibrar, stream on/off, guardar EEPROM, reset OTOS).
//
// El glue con el hardware (leer line_ring/otos, escribir Serial) vive en
// src/down/down_telemetry_serial.cpp (Arduino-only, GATEADO con
// -DDOWN_DEBUG_TELEMETRY) para mantener este módulo 100% host-testeable.
//
// Por qué JSON Lines (texto) y no binario (v1):
//   - Trivial de parsear en Python (json.loads por línea) y de loguear a archivo.
//   - Human-readable para debug directo en el Serial Monitor.
//   - El USB CDC del Teensy (~1 MB/s) sobra para ~50 Hz × ~900 B/línea (~45 KB/s).
//   Si algún día la tasa lo exige, se migra a binario con un schema nuevo.
//
// Contrato versionado (TELEMETRY_DOWN_SCHEMA). Spec: docs/firmware/TELEMETRIA-DOWN.md
// App de PC que lo consume: tools/monitor-base/.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Versión del esquema de telemetría. Subir si cambia el layout del JSON.
// La app de PC valida este número (campo "v") y rechaza esquemas desconocidos.
//   v1 (2026-06-07): anillo 32 + LineStatusV2 + OTOS fusionado.
//   v2 (2026-06-07): + lecturas por-OTOS izq/der (lx/ly/lh/rx/ry/rh) para el arquero.
constexpr uint8_t TELEMETRY_DOWN_SCHEMA = 2;

constexpr int TD_MAX_SENSORS = 32;

// Sentinels N/A — espejo de los de LineStatusV2 (types.h). Se emiten TAL CUAL
// en el JSON; la app los interpreta como "sin dato".
constexpr int16_t  TD_NA_I16 = INT16_MIN;   // -32768  (angle/escape/xtrack)
constexpr uint16_t TD_NA_U16 = 0xFFFFu;     // 65535   (penetration)

// ── Snapshot que el glue arma cada tick y pasa al serializador ───────────────
// POD plano. Todos los campos son lo que el firmware YA computa para competencia
// (no datos inventados): el anillo crudo + calib de line_ring, el LineStatusV2 que
// arma dm_update y manda comm_central, y la pose fusionada de los OTOS.
struct TelemetryDownFrame {
    uint8_t  schema;            // = TELEMETRY_DOWN_SCHEMA (lo setea el serializador)
    uint32_t seq;              // contador monotónico de frame
    uint32_t t_ms;            // millis() al emitir

    // ── Anillo de luz (line_ring) ──
    uint8_t  num_sensors;     // 1..32 (NUM_LINE_SENSORS)
    uint16_t raw[TD_MAX_SENSORS];        // cuentas ADC crudas por sensor
    uint32_t white_bits;      // bit i = sensor i ve blanco (line_ring_get_white)
    uint16_t carpet[TD_MAX_SENSORS];     // calib carpet por sensor (avg)
    uint16_t white_cal[TD_MAX_SENSORS];  // calib blanco por sensor (avg)

    // ── Línea procesada (LineStatusV2 — lo que viaja a CENTRAL) ──
    uint8_t  line_schema;     // LSV2_SCHEMA (informativo)
    uint8_t  data_valid;      // 0/1 compuerta maestra
    uint8_t  line_present;    // 0/1
    int16_t  line_angle_cd;   // centideg, N/A = TD_NA_I16
    int16_t  escape_angle_cd; // centideg, N/A = TD_NA_I16
    uint16_t penetration_mm;  // N/A = TD_NA_U16
    int16_t  cross_track_mm;  // N/A = TD_NA_I16
    uint8_t  sensors_on_line; // 0..32
    uint8_t  event_flags;     // EV_* OR-eados
    uint8_t  quality;         // 0..100
    uint8_t  sample_age_ms;   // 0..255

    // ── Odometría (OTOS fusionado) ──
    uint8_t  otos_count;      // NUM_OTOS configurado (0/1/2)
    uint8_t  otos_left_ok;    // 0/1 (otos_is_left_ready)
    uint8_t  otos_right_ok;   // 0/1 (otos_is_right_ready)
    float    otos_x_mm;
    float    otos_y_mm;
    float    otos_heading_deg;
    float    otos_vx_mm_s;
    float    otos_vy_mm_s;
    float    otos_omega_rad_s;
    float    otos_slip_mm_s;
    // Lecturas por-OTOS SIN fusionar (schema v2): el diferencial izq/der que usa
    // el arquero para verificar que avanza derecho sobre la línea.
    float    otos_left_x_mm;
    float    otos_left_y_mm;
    float    otos_left_heading_deg;
    float    otos_right_x_mm;
    float    otos_right_y_mm;
    float    otos_right_heading_deg;

    // ── Diagnóstico ──
    uint8_t  lifted;          // 0/1 (line_ring_is_lifted)
    uint32_t line_tick_count; // line_ring_get_tick_count
    uint32_t line_tick_us;    // line_ring_get_last_tick_us (duración del tick = carga)
};

// Inicializa un frame a ceros + schema + N/A sensatos. Útil en el glue y tests.
void td_frame_init(TelemetryDownFrame& f, uint8_t num_sensors);

// Serializa `f` a UNA línea JSON terminada en '\n', dentro de `buf` (capacidad
// `cap`). Setea f NO se modifica salvo schema implícito (se emite
// TELEMETRY_DOWN_SCHEMA siempre). Retorna la cantidad de bytes escritos
// (sin contar el '\0'), o -1 si el buffer es muy chico / argumentos inválidos.
// El buffer queda NUL-terminado en caso de éxito. Recomendado cap >= 1024.
int td_serialize_jsonl(char* buf, int cap, const TelemetryDownFrame& f);

// ── Comandos host → firmware ─────────────────────────────────────────────────
enum class TdCmd : uint8_t {
    NONE = 0,     // línea vacía / solo espacios
    UNKNOWN,      // no reconocido
    PING,         // "PING"            → el firmware puede responder pong
    STREAM_ON,    // "STREAM ON"       → arranca el stream de telemetría
    STREAM_OFF,   // "STREAM OFF"      → para el stream
    SET_RATE,     // "RATE <hz>"       → cambia la tasa (arg = hz)
    CAL_CARPET,   // "CAL CARPET"      → line_ring_calibrate_carpet()
    CAL_WHITE,    // "CAL WHITE"       → line_ring_calibrate_white()
    CAL_AUTO_ON,  // "CAL AUTO ON"     → captura min/max por sensor mientras se mueve
    CAL_AUTO_OFF, // "CAL AUTO OFF"    → cierra la auto-calib (carpet=min, white=max)
    CAL_SAVE,     // "CAL SAVE"        → guarda calib a EEPROM
    CAL_LOAD,     // "CAL LOAD"        → carga calib de EEPROM
    OTOS_RESET,   // "OTOS RESET"      → otos_reset()
};

struct TdCommand {
    TdCmd   cmd;
    int32_t arg;   // usado por SET_RATE (hz); 0 para el resto
};

// Parsea una línea de comando (case-insensitive, tokens separados por espacios;
// ignora '\r'/'\n' finales). `len` = largo de `s` (sin requerir NUL). Devuelve
// {NONE,0} para línea vacía y {UNKNOWN,0} si no matchea ningún comando.
TdCommand td_parse_command(const char* s, int len);

}  // namespace iitasoccer
