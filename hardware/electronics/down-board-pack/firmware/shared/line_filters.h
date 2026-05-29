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

// === MuxWatchdog (TEMA 1 P0 — 2026-05-29) ===
// Constantes default; cada llamador puede pasar valores propios.
constexpr int      MW_MAX_MUXES            = 4;     // CD4051 en la placa DOWN
constexpr int      MW_SENSORS_PER_MUX      = 8;     // canales por CD4051
constexpr uint32_t MW_STUCK_DEBOUNCE_MS    = 100;   // ms con valores idénticos para diagnosticar muerto
constexpr uint16_t MW_EXTREME_LOW          = 50;    // ADC pegado a GND
constexpr uint16_t MW_EXTREME_HIGH         = 970;   // ADC pegado a Vref (10-bit, 1023 max)

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

// === MuxWatchdog: detector de mux CD4051 muerto/colgado (TEMA 1 P0) ===
//
// Cada mux CD4051 controla 8 sensores. Si un mux falla (chip quemado, líneas
// SEL flotantes, soldadura intermitente, ESD), su salida COM típicamente queda
// pegada a Vcc o GND. El watchdog detecta esa condición monitoreando:
//
//   1. Si los 8 sensores de un mux dan EXACTAMENTE el mismo valor durante
//      >= MW_STUCK_DEBOUNCE_MS ms consecutivos, Y
//   2. ese valor está en uno de los dos extremos (≤ MW_EXTREME_LOW o
//      ≥ MW_EXTREME_HIGH).
//
// → marca el mux como muerto. El bit del bitmap se setea en EV_MUX_DEAD del
// LineStatusV2 y DOWN reporta `dead_bitmap` para que CENTRAL sepa cuál mux.
//
// Auto-recovery: si los valores vuelven a variar, dead[m] retorna a false
// automáticamente (no hace falta reset manual).
//
// El valor "extremo" filtra falsos positivos donde 8 sensores coinciden en
// un valor medio por azar (raro, pero posible bajo iluminación uniforme).
struct MuxWatchdog {
    uint32_t stuck_start_ms[MW_MAX_MUXES];  // ms en que arrancó la "racha igual"
    uint16_t last_common_value[MW_MAX_MUXES];  // valor que tenían los 8 (si stuck)
    bool     stuck_active[MW_MAX_MUXES];   // hay racha viva
    bool     dead[MW_MAX_MUXES];           // muxe[m] confirmado muerto
};

// Inicializa todos los campos a estado limpio (todo en 0/false).
void mw_init(MuxWatchdog& w);

// Update por tick. `raw` = array de n_sensors lecturas RAW del ADC (NO
// filtered — queremos detectar mux atascado a nivel hardware, antes del
// suavizado temporal). El mux m cubre raw[m*sensors_per_mux .. +sensors_per_mux-1].
// `n_muxes` = cuántos muxes hay físicamente (1..MW_MAX_MUXES).
// `now_ms` debe ser monótonamente creciente.
void mw_update(MuxWatchdog& w,
               const uint16_t* raw,
               int n_muxes,
               int sensors_per_mux,
               uint32_t now_ms,
               uint32_t stuck_debounce_ms = MW_STUCK_DEBOUNCE_MS,
               uint16_t extreme_low = MW_EXTREME_LOW,
               uint16_t extreme_high = MW_EXTREME_HIGH);

// True si el mux m (0-based) está muerto. Out-of-range → false.
bool mw_is_dead(const MuxWatchdog& w, int mux_idx);

// Bitmap: bit m = mux m está muerto. Solo bits 0..MW_MAX_MUXES-1 usados.
uint8_t mw_get_dead_bitmap(const MuxWatchdog& w);

// True si CUALQUIER mux está marcado muerto (para flag global EV_MUX_DEAD).
bool mw_any_dead(const MuxWatchdog& w);

}  // namespace iitasoccer
