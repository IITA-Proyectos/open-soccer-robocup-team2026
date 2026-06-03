// line_ring.cpp — Lectura hardware del anillo de sensores y aplicación de filtros.
//
// El hardware (lectura de los 32 sensores via muxes) vive acá. La lógica de
// procesamiento (filtros temporal/espacial/hysteresis/lifted, cálculo de ángulo)
// vive en src/shared/line_filters.{h,cpp} — funciones puras testeables en host.
// Ver test/test_line_filters/ para tests unitarios de la lógica.

#include "line_ring.h"
#include "line_filters.h"

#include <Arduino.h>

namespace iitasoccer {

namespace {

// === Buffers de lectura ===
uint16_t g_raw[NUM_LINE_SENSORS] = {0};            // lectura cruda del ADC
uint16_t g_raw_filtered[NUM_LINE_SENSORS] = {0};   // post filtro temporal

// === Calibración por sensor ===
uint16_t g_threshold[NUM_LINE_SENSORS];
uint16_t g_carpet_avg[NUM_LINE_SENSORS];
uint16_t g_white_avg[NUM_LINE_SENSORS];

// === Estado de los filtros ===
FilterBuffer g_temporal_bufs[NUM_LINE_SENSORS];
bool g_sensor_white[NUM_LINE_SENSORS] = {false};           // post temporal+hysteresis
bool g_sensor_white_validated[NUM_LINE_SENSORS] = {false}; // post spatial
LiftedDetector g_lifted_state = {};

// === Outputs procesados ===
float   g_angle_deg = 0.0f;
uint8_t g_depth = 0;
bool    g_imminent_exit = false;

uint32_t g_tick_count = 0;
uint32_t g_last_tick_us = 0;

constexpr uint8_t IMMINENT_EXIT_DEPTH = 3;

void sample_all_sensors_hardware() {
    // Para cada uno de los 8 sensores lógicos por mux, calcular el canal real
    // del CD4051 según el scrambling de Enzo (MUX_CH_FOR_SENSOR), setear los
    // selectores de los 4 muxes simultáneamente (12 pines = 4 muxes × A/B/C),
    // esperar a que el mux asiente, y leer los 4 ADC en una sola pasada.
    for (uint8_t i = 0; i < NUM_SENSORS_PER_MUX; ++i) {
        const uint8_t ch = MUX_CH_FOR_SENSOR[i];
        for (int m = 0; m < DOWN_NUM_MUXES_CONNECTED; ++m) {
            digitalWrite(PIN_MUX_A[m], (ch & 0x01) ? HIGH : LOW);
            digitalWrite(PIN_MUX_B[m], (ch & 0x02) ? HIGH : LOW);
            digitalWrite(PIN_MUX_C[m], (ch & 0x04) ? HIGH : LOW);
        }
        delayMicroseconds(5);  // settle time CD4051
        for (int m = 0; m < DOWN_NUM_MUXES_CONNECTED; ++m) {
            const uint8_t idx = m * NUM_SENSORS_PER_MUX + i;
            g_raw[idx] = static_cast<uint16_t>(analogRead(PIN_MUX_OUT[m]));
        }
    }
}

void reset_filter_state() {
    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        g_temporal_bufs[i] = FilterBuffer{};
        g_sensor_white[i] = false;
        g_sensor_white_validated[i] = false;
    }
    g_lifted_state = LiftedDetector{};
}

}  // namespace

void line_ring_init() {
    // 12 pines de selección (A/B/C por cada mux, no compartidos).
    for (int m = 0; m < DOWN_NUM_MUXES_CONNECTED; ++m) {
        pinMode(PIN_MUX_A[m], OUTPUT);
        pinMode(PIN_MUX_B[m], OUTPUT);
        pinMode(PIN_MUX_C[m], OUTPUT);
    }
    // INH atado a GND físico en el PCB — no se controla por firmware.

    // Calibración default: a la espera de calibración real con carpet/white.
    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        g_threshold[i] = LINE_DEFAULT_THRESHOLD;
        g_carpet_avg[i] = 200;     // estimación carpet verde
        g_white_avg[i]  = 800;     // estimación blanco
    }
    reset_filter_state();

    analogReadResolution(10);  // 10-bit (0-1023). Teensy 4.0 soporta hasta 12-bit.
}

void line_ring_tick() {
    const uint32_t t_start = micros();

    // 1. Lectura hardware: 32 sensores via muxes.
    sample_all_sensors_hardware();

    // 2. Filtro temporal (moving average) por sensor.
    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        g_raw_filtered[i] = lf_temporal_update(g_temporal_bufs[i], g_raw[i]);
    }

    // 3. Hysteresis por sensor (anti-flicker en el umbral).
    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        g_sensor_white[i] = lf_hysteresis_on_white(
            g_raw_filtered[i], g_threshold[i], g_sensor_white[i]);
    }

    // 4. Filtro espacial (cada sensor blanco requiere un vecino blanco).
    lf_spatial_filter(g_sensor_white, g_sensor_white_validated, NUM_LINE_SENSORS);

    // 5. Detección de robot levantado.
    lf_lifted_update(g_lifted_state, millis(),
                     g_raw_filtered, g_carpet_avg, NUM_LINE_SENSORS);

    // 6. Centroide angular sobre sensores validados.
    AngleResult result = lf_compute_centroid_angle(
        g_sensor_white_validated, g_raw_filtered,
        g_threshold, g_white_avg, NUM_LINE_SENSORS);

    g_depth = static_cast<uint8_t>(result.depth);
    g_angle_deg = result.valid ? result.angle_deg : 0.0f;

    // imminent_exit: depth alto Y robot NO está levantado (sino son datos basura).
    g_imminent_exit = (g_depth >= IMMINENT_EXIT_DEPTH) && !g_lifted_state.is_lifted;

    g_last_tick_us = micros() - t_start;
    g_tick_count++;
}

// === Accesores ===
float    line_ring_get_angle_deg()      { return g_angle_deg; }
uint8_t  line_ring_get_depth()          { return g_depth; }
bool     line_ring_get_imminent_exit()  { return g_imminent_exit; }
bool     line_ring_is_lifted()          { return g_lifted_state.is_lifted; }

uint16_t line_ring_get_raw(uint8_t sensor_idx) {
    if (sensor_idx >= NUM_LINE_SENSORS) return 0;
    return g_raw[sensor_idx];
}

bool line_ring_get_white(uint8_t sensor_idx) {
    if (sensor_idx >= NUM_LINE_SENSORS) return false;
    return g_sensor_white_validated[sensor_idx];
}

// === Calibración ===
// Las dos funciones siguientes capturan promedios con el robot en posición
// conocida (sobre carpet o sobre línea blanca). Solo deben llamarse en modo
// admin — bloquean el loop ~320 ms.

void line_ring_calibrate_carpet() {
    uint32_t accum[NUM_LINE_SENSORS] = {0};
    constexpr int N_SAMPLES = 32;
    for (int s = 0; s < N_SAMPLES; ++s) {
        sample_all_sensors_hardware();
        for (int i = 0; i < NUM_LINE_SENSORS; ++i) accum[i] += g_raw[i];
        delay(10);
    }
    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        g_carpet_avg[i] = static_cast<uint16_t>(accum[i] / N_SAMPLES);
        g_threshold[i] = static_cast<uint16_t>((g_carpet_avg[i] + g_white_avg[i]) / 2);
    }
    // Reset buffers de filtros — los datos antiguos ya no representan la nueva calibración.
    reset_filter_state();
}

void line_ring_calibrate_white() {
    uint32_t accum[NUM_LINE_SENSORS] = {0};
    constexpr int N_SAMPLES = 32;
    for (int s = 0; s < N_SAMPLES; ++s) {
        sample_all_sensors_hardware();
        for (int i = 0; i < NUM_LINE_SENSORS; ++i) accum[i] += g_raw[i];
        delay(10);
    }
    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        g_white_avg[i] = static_cast<uint16_t>(accum[i] / N_SAMPLES);
        g_threshold[i] = static_cast<uint16_t>((g_carpet_avg[i] + g_white_avg[i]) / 2);
    }
    reset_filter_state();
}

void line_ring_set_calibration(const uint16_t* carpet, const uint16_t* white, int n) {
    if (carpet == nullptr || white == nullptr) return;
    if (n > NUM_LINE_SENSORS) n = NUM_LINE_SENSORS;
    for (int i = 0; i < n; ++i) {
        g_carpet_avg[i] = carpet[i];
        g_white_avg[i]  = white[i];
        g_threshold[i]  = static_cast<uint16_t>((g_carpet_avg[i] + g_white_avg[i]) / 2);
    }
    reset_filter_state();
}

uint16_t line_ring_get_carpet_avg(uint8_t sensor_idx) {
    if (sensor_idx >= NUM_LINE_SENSORS) return 0;
    return g_carpet_avg[sensor_idx];
}

uint16_t line_ring_get_white_avg(uint8_t sensor_idx) {
    if (sensor_idx >= NUM_LINE_SENSORS) return 0;
    return g_white_avg[sensor_idx];
}

uint32_t line_ring_get_tick_count()   { return g_tick_count; }
uint32_t line_ring_get_last_tick_us() { return g_last_tick_us; }

}  // namespace iitasoccer
