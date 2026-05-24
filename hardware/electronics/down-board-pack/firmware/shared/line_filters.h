// line_filters.h — Procesamiento de señal del anillo de sensores de línea.
//
// Funciones PURAS (sin Arduino). Compilable como native para testear en host.
// La placa DOWN llama a estas funciones desde line_ring.cpp después de leer
// el hardware. Separación de hardware/algoritmo permite:
//   - Tests unitarios con datos sintéticos.
//   - Reutilización en otros contextos (simulador, post-procesado).
//   - Razonamiento aislado sobre el algoritmo.
//
// Cuatro técnicas aplicadas (en orden de pipeline):
//   1) Temporal — moving average de últimas N muestras por sensor.
//   2) Hysteresis — banda de ±H counts alrededor del threshold para evitar flicker.
//   3) Espacial — un sensor sólo cuenta como "blanco" si tiene un vecino activo.
//   4) Detección de "robot levantado" — si suficientes sensores están abajo del
//      carpet típico durante > debounce_ms, el robot está en el aire.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// === Constantes de tuning (default — sobreescribibles en config) ===
constexpr int LF_FILTER_BUFFER_SIZE        = 4;     // muestras del moving average
constexpr int LF_HYSTERESIS_BAND           = 20;    // counts
constexpr int LF_LIFTED_MIN_SENSORS        = 28;    // 28/32 = 87 % en aire
constexpr uint16_t LF_LIFTED_DELTA_BELOW   = 50;    // raw < carpet - 50 = aire
constexpr uint32_t LF_LIFTED_DEBOUNCE_MS   = 100;   // anti-glitch

// === Filtro temporal — moving average ===

struct FilterBuffer {
    uint16_t samples[LF_FILTER_BUFFER_SIZE];
    uint8_t  head;
    bool     primed;     // false hasta que se llenaron N samples
};

// Aplica un nuevo sample. Retorna el promedio actual del buffer.
// Antes de tener N samples, retorna el sample crudo (no espera N para producir output).
uint16_t lf_temporal_update(FilterBuffer& buf, uint16_t new_sample);

// === Hysteresis sobre threshold ===

// Decide si un sensor está "en blanco" considerando el estado anterior.
// Si estaba en blanco, sigue en blanco mientras filtered >= threshold - band.
// Si estaba en carpet, pasa a blanco solo cuando filtered >= threshold + band.
// Evita flickeo cuando la lectura oscila exactamente sobre el threshold.
bool lf_hysteresis_on_white(uint16_t filtered,
                             uint16_t threshold,
                             bool was_on_white);

// === Filtro espacial — vecinos en el anillo ===

// Marca cada sensor como "validado en blanco" solo si:
//   - is_white[i] == true, Y
//   - al menos un vecino directo (i-1 o i+1, anillo cerrado) también es blanco.
// Esto descarta sensores aislados (típicamente ruido o falsos positivos).
//
// `is_white` y `validated_out` deben ser buffers de tamaño n_sensors.
// Pueden ser el MISMO buffer (modificación in-place).
void lf_spatial_filter(const bool* is_white,
                        bool* validated_out,
                        int n_sensors);

// === Cálculo de ángulo (centroide ponderado) ===

struct AngleResult {
    float angle_rad;     // -π a +π. 0 = frente.
    float angle_deg;     // -180 a +180.
    int   depth;         // cantidad de sensores validados en blanco.
    bool  valid;         // false si no hay sensores activos.
};

// Algoritmo de centroide angular ponderado:
//   Para cada sensor validado: peso = (filtered - threshold) / (white_avg - threshold).
//   sum_x += weight × cos(θ_i),  sum_y += weight × sin(θ_i).
//   angle = atan2(sum_y, sum_x).
//
// `validated_white`, `filtered_raw`, `threshold`, `white_avg` son arrays de n_sensors.
// El sensor i está en ángulo físico θ_i = i × 360°/n_sensors.
AngleResult lf_compute_centroid_angle(const bool* validated_white,
                                       const uint16_t* filtered_raw,
                                       const uint16_t* threshold,
                                       const uint16_t* white_avg,
                                       int n_sensors);

// === Detección de robot levantado ===

struct LiftedDetector {
    uint32_t candidate_start_ms;   // válido solo si candidate_active
    bool     candidate_active;     // true si hay candidato vigente
    bool     is_lifted;            // true si el candidato cumplió el debounce
};

// Actualiza el estado del detector. Si N sensores están abajo del carpet por
// más de LF_LIFTED_DEBOUNCE_MS, marca is_lifted=true.
// Cuando los sensores vuelven a normal, is_lifted=false inmediatamente.
// `now_ms` debe ser monótonamente creciente (pasar millis() en hardware,
// timestamp sintético en tests).
void lf_lifted_update(LiftedDetector& state,
                       uint32_t now_ms,
                       const uint16_t* filtered_raw,
                       const uint16_t* carpet_avg,
                       int n_sensors);

}  // namespace iitasoccer
