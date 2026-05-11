#include "line_ring.h"
#include <Arduino.h>
#include <cmath>

namespace iitasoccer {

namespace {

// Lecturas crudas más recientes (uint16_t para soportar 10-12 bit ADC).
uint16_t g_raw[NUM_LINE_SENSORS] = {0};

// Umbrales por sensor (uno por sensor — ajustable con calibración).
uint16_t g_threshold[NUM_LINE_SENSORS];

// Promedio en carpet y en blanco (capturado por calibrate_*).
uint16_t g_carpet_avg[NUM_LINE_SENSORS];
uint16_t g_white_avg[NUM_LINE_SENSORS];

// Outputs procesados.
float   g_angle_deg = 0.0f;
uint8_t g_depth = 0;
bool    g_imminent_exit = false;

uint32_t g_tick_count = 0;
uint32_t g_last_tick_us = 0;

constexpr float DEG_PER_SENSOR = 360.0f / static_cast<float>(NUM_LINE_SENSORS);
constexpr uint8_t IMMINENT_EXIT_DEPTH = 3;  // si >= 3 sensores ven blanco simultáneamente

// Seteo los pines A, B, C para seleccionar el canal i (0..7) en TODOS los muxes
// simultáneamente. Los 4 muxes leen su canal i en paralelo, luego nosotros leemos
// O1..O4 (las 4 entradas analógicas correspondientes).
void select_mux_channel(uint8_t ch) {
    digitalWrite(PIN_MUX_SEL_A, (ch & 0x01) ? HIGH : LOW);
    digitalWrite(PIN_MUX_SEL_B, (ch & 0x02) ? HIGH : LOW);
    digitalWrite(PIN_MUX_SEL_C, (ch & 0x04) ? HIGH : LOW);
}

}  // namespace

void line_ring_init() {
    pinMode(PIN_MUX_SEL_A, OUTPUT);
    pinMode(PIN_MUX_SEL_B, OUTPUT);
    pinMode(PIN_MUX_SEL_C, OUTPUT);

    // Habilitar todos los muxes conectados (INH activo bajo).
    for (int m = 0; m < DOWN_NUM_MUXES_CONNECTED; ++m) {
        pinMode(PIN_MUX_INH[m], OUTPUT);
        digitalWrite(PIN_MUX_INH[m], LOW);  // 0 = habilitado
    }

    // Inicializar umbrales con default. Después se ajustan vía calibración.
    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        g_threshold[i] = LINE_DEFAULT_THRESHOLD;
        g_carpet_avg[i] = 0;
        g_white_avg[i] = 1023;
    }

    analogReadResolution(10);  // 10-bit (0-1023) — Teensy 4.0 soporta hasta 12-bit
}

void line_ring_tick() {
    const uint32_t t_start = micros();

    // Iteramos los 8 canales del mux.
    for (uint8_t ch = 0; ch < NUM_SENSORS_PER_MUX; ++ch) {
        select_mux_channel(ch);
        // Settle time del CD4051 ~ 2-3 µs a 5V; con 3.3V puede ser más.
        delayMicroseconds(5);

        // Leemos las salidas de los muxes habilitados. La indexación es:
        //   sensor index = mux_idx * NUM_SENSORS_PER_MUX + ch
        for (int m = 0; m < DOWN_NUM_MUXES_CONNECTED; ++m) {
            uint8_t idx = m * NUM_SENSORS_PER_MUX + ch;
            g_raw[idx] = static_cast<uint16_t>(analogRead(PIN_MUX_OUT[m]));
        }
    }

    // Compute ángulo + profundidad.
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    uint8_t depth = 0;

    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        if (g_raw[i] >= g_threshold[i]) {
            depth++;
            const float theta_rad = i * DEG_PER_SENSOR * (M_PI / 180.0f);
            sum_x += std::cos(theta_rad);
            sum_y += std::sin(theta_rad);
        }
    }

    g_depth = depth;
    g_imminent_exit = (depth >= IMMINENT_EXIT_DEPTH);

    if (depth > 0) {
        g_angle_deg = std::atan2(sum_y, sum_x) * (180.0f / M_PI);
    } else {
        g_angle_deg = 0.0f;  // sin línea detectada
    }

    g_last_tick_us = micros() - t_start;
    g_tick_count++;
}

float   line_ring_get_angle_deg()    { return g_angle_deg; }
uint8_t line_ring_get_depth()        { return g_depth; }
bool    line_ring_get_imminent_exit(){ return g_imminent_exit; }

uint16_t line_ring_get_raw(uint8_t sensor_idx) {
    if (sensor_idx >= NUM_LINE_SENSORS) return 0;
    return g_raw[sensor_idx];
}

bool line_ring_get_white(uint8_t sensor_idx) {
    if (sensor_idx >= NUM_LINE_SENSORS) return false;
    return g_raw[sensor_idx] >= g_threshold[sensor_idx];
}

void line_ring_calibrate_carpet() {
    // Promedio de 32 muestras por sensor.
    uint32_t accum[NUM_LINE_SENSORS] = {0};
    constexpr int N_SAMPLES = 32;
    for (int s = 0; s < N_SAMPLES; ++s) {
        line_ring_tick();
        for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
            accum[i] += g_raw[i];
        }
        delay(10);
    }
    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        g_carpet_avg[i] = static_cast<uint16_t>(accum[i] / N_SAMPLES);
        g_threshold[i] = static_cast<uint16_t>((g_carpet_avg[i] + g_white_avg[i]) / 2);
    }
}

void line_ring_calibrate_white() {
    uint32_t accum[NUM_LINE_SENSORS] = {0};
    constexpr int N_SAMPLES = 32;
    for (int s = 0; s < N_SAMPLES; ++s) {
        line_ring_tick();
        for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
            accum[i] += g_raw[i];
        }
        delay(10);
    }
    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        g_white_avg[i] = static_cast<uint16_t>(accum[i] / N_SAMPLES);
        g_threshold[i] = static_cast<uint16_t>((g_carpet_avg[i] + g_white_avg[i]) / 2);
    }
}

uint32_t line_ring_get_tick_count()  { return g_tick_count; }
uint32_t line_ring_get_last_tick_us(){ return g_last_tick_us; }

}  // namespace iitasoccer
