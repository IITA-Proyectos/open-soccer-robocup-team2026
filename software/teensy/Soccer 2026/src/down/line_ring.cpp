// line_ring.cpp — Lectura hardware del anillo de sensores y aplicación de filtros.
//
// El hardware (lectura de los 32 sensores via muxes) vive acá. La lógica de
// procesamiento (filtros temporal/espacial/hysteresis/lifted, cálculo de ángulo)
// vive en src/shared/line_filters.{h,cpp} — funciones puras testeables en host.
// Ver test/test_line_filters/ para tests unitarios de la lógica.

#include "line_ring.h"
#include "line_filters.h"

#include <Arduino.h>
#if defined(DOWN_ADC_FAST) && defined(DOWN_ADC_DUAL)
#include <ADC.h>             // pedvide/ADC (viene en el core Teensy) — lectura dual-ADC sincronizada
#include "adc_scan_plan.h"   // plan de apareo de muxes ADC1/ADC2 (PURO, host-tested)
#endif

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
uint32_t g_last_tick_us = 0;     // DURACIÓN del último tick (diagnóstico de carga)
uint32_t g_last_sample_us = 0;   // TIMESTAMP (micros()) del último muestreo

constexpr uint8_t IMMINENT_EXIT_DEPTH = 3;

#if defined(DOWN_ADC_FAST) && defined(DOWN_ADC_DUAL)
// F1 Capa 2 (§3.2 ARQUITECTURA-LAZO-DOWN-RT): objeto ADC del core (pedvide) para las
// lecturas SINCRONIZADAS (ADC1+ADC2 a la vez) y el plan de apareo de muxes (puro). Se
// construyen una sola vez en line_ring_init(). Gateado off-by-default → competencia byte-idéntica.
ADC g_adc;
AdcScanPlan g_scan_plan{};
#endif

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
#if defined(DOWN_ADC_FAST) && defined(DOWN_ADC_DUAL)
        // F1 Capa 2: leer los muxes DE A PARES en paralelo (uno por ADC1, otro por ADC2).
        // El plan (puro) conserva el orden lógico del barrido histórico → g_raw[] sale igual
        // ordenado, sin re-mapear sensores. 2 lecturas dobles en vez de 4 simples.
        for (int p = 0; p < g_scan_plan.n_pairs; ++p) {
            const uint8_t ma = g_scan_plan.pairs[p][0];   // → ADC1 (result_adc0)
            const uint8_t mb = g_scan_plan.pairs[p][1];   // → ADC2 (result_adc1)
            g_adc.startSynchronizedSingleRead(PIN_MUX_OUT[ma], PIN_MUX_OUT[mb]);
            const ADC::Sync_result r = g_adc.readSynchronizedSingle();
            g_raw[ma * NUM_SENSORS_PER_MUX + i] = static_cast<uint16_t>(r.result_adc0);
            g_raw[mb * NUM_SENSORS_PER_MUX + i] = static_cast<uint16_t>(r.result_adc1);
        }
        if (g_scan_plan.has_solo) {   // mux impar sobrante (placa degradada) → lectura simple
            const uint8_t ms = g_scan_plan.solo_mux;
            g_raw[ms * NUM_SENSORS_PER_MUX + i] =
                static_cast<uint16_t>(g_adc.adc0->analogRead(PIN_MUX_OUT[ms]));
        }
#else
        // Path histórico / F1 Capa 1: N analogRead secuenciales. Bajo DOWN_ADC_FAST (sin
        // DUAL) el averaging baja a 1 (seteado en line_ring_init) → ~20.9µs → ~4.9µs por
        // lectura sin cambiar la estructura del barrido. Sin el flag, averaging=4 (histórico).
        for (int m = 0; m < DOWN_NUM_MUXES_CONNECTED; ++m) {
            const uint8_t idx = m * NUM_SENSORS_PER_MUX + i;
            g_raw[idx] = static_cast<uint16_t>(analogRead(PIN_MUX_OUT[m]));
        }
#endif
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

#if defined(DOWN_ADC_FAST) && defined(DOWN_ADC_DUAL)
    // F1 Capa 2 (§3.2): la lectura va por el objeto ADC (pedvide/ADC) con config POR-ADC —
    // NO se llaman las globales del core (serían redundantes y podrían confundir el delta del
    // solo_mux vs los pares). El plan (puro) decide el apareo {0,1},{2,3} desde la cantidad de
    // muxes (fail-safe ante placa degradada). averaging=1 = sin promedio HW; el ALS-PT19 ya se
    // filtra temporalmente aguas abajo (line_filters / dm_update). Barrido ~717µs → ~126µs.
    g_scan_plan = adc_plan_build(DOWN_NUM_MUXES_CONNECTED);
    g_adc.adc0->setResolution(10);
    g_adc.adc1->setResolution(10);
    g_adc.adc0->setAveraging(1);
    g_adc.adc1->setAveraging(1);
#elif defined(DOWN_ADC_FAST)
    // F1 Capa 1 (§3.2): averaging=1 (el default del core es 4) sobre el path analogRead() del
    // core. Baja ~20.9µs → ~4.9µs por lectura (barrido ~717µs → ~205µs). El ruido sube √4=2×
    // por lectura cruda pero el filtro temporal aguas abajo lo absorbe. Reversible con el flag.
    analogReadResolution(10);
    analogReadAveraging(1);
#else
    analogReadResolution(10);  // 10-bit (0-1023). Teensy 4.0 soporta hasta 12-bit.
#endif
}

void line_ring_tick() {
    const uint32_t t_start = micros();

    // 1. Lectura hardware: 32 sensores via muxes.
    //    SIEMPRE corre (con o sin LINE_RING_PROCESS): comm_central_send_line_urgent
    //    re-lee estos crudos via line_ring_get_raw y los procesa en dm_update()
    //    (DownModel) para armar el LineStatusV2 que va a CENTRAL. El gate de abajo
    //    solo apaga el pipeline procesado MUERTO (sus salidas no las consume nadie
    //    en competencia — ver config_down.h / LINE_RING_PROCESS).
    sample_all_sensors_hardware();

#if defined(DOWN_DEBUG_SERIAL) || defined(LINE_RING_PROCESS)
    // Pipeline procesado (g_angle_deg / g_depth / g_imminent_exit / g_lifted /
    // g_sensor_white_validated). Lectores: SOLO los diags de banco
    // (main_diag_down, diag_down_calibracion). En competencia esto está muerto
    // (LineStatusV2 sale de dm_update, no de acá); se compila con LINE_RING_PROCESS
    // ON por default para ser byte-idéntico hasta que se pase -DDOWN_LEAN_LINE_PIPELINE.

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
#endif  // pipeline procesado

    g_last_tick_us = micros() - t_start;   // duración del tick (diagnóstico)
    g_last_sample_us = micros();           // timestamp del muestreo (freshness) — SIEMPRE
    g_tick_count++;                        // SIEMPRE (dm_update/sample_age no dependen del pipeline)
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

uint32_t line_ring_get_tick_count()     { return g_tick_count; }
uint32_t line_ring_get_last_tick_us()   { return g_last_tick_us; }
uint32_t line_ring_get_last_sample_us() { return g_last_sample_us; }

}  // namespace iitasoccer
