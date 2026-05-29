#include "line_filters.h"
#include <cmath>
#include <cstring>

namespace iitasoccer {

namespace {
constexpr float PI_F = 3.14159265358979323846f;
}

// === Temporal ===

uint16_t lf_temporal_update(FilterBuffer& buf, uint16_t new_sample) {
    buf.samples[buf.head] = new_sample;
    buf.head = (buf.head + 1) % LF_FILTER_BUFFER_SIZE;
    if (buf.head == 0) buf.primed = true;  // dimos la vuelta al buffer

    if (!buf.primed) {
        // Aún no llenamos el buffer — promediar solo las muestras que tenemos.
        uint32_t sum = 0;
        for (int i = 0; i < buf.head; ++i) sum += buf.samples[i];
        return static_cast<uint16_t>(buf.head == 0 ? new_sample : sum / buf.head);
    }

    uint32_t sum = 0;
    for (int i = 0; i < LF_FILTER_BUFFER_SIZE; ++i) sum += buf.samples[i];
    return static_cast<uint16_t>(sum / LF_FILTER_BUFFER_SIZE);
}

// === Hysteresis ===

bool lf_hysteresis_on_white(uint16_t filtered, uint16_t threshold, bool was_on_white) {
    if (was_on_white) {
        // Para "salir" de blanco, hay que bajar a threshold - band.
        return filtered >= (threshold > LF_HYSTERESIS_BAND
                            ? threshold - LF_HYSTERESIS_BAND
                            : 0);
    } else {
        // Para "entrar" a blanco, hay que subir a threshold + band.
        return filtered >= (threshold + LF_HYSTERESIS_BAND);
    }
}

// === Espacial ===

void lf_spatial_filter(const bool* is_white, bool* validated_out, int n_sensors) {
    if (n_sensors <= 0) return;

    // Tomar copia del input por si validated_out == is_white (in-place).
    // Buffer máximo razonable: 64 sensores. Si crece más, usar heap.
    constexpr int MAX_SENSORS = 64;
    bool copy[MAX_SENSORS];
    int n = n_sensors < MAX_SENSORS ? n_sensors : MAX_SENSORS;
    for (int i = 0; i < n; ++i) copy[i] = is_white[i];

    for (int i = 0; i < n; ++i) {
        if (!copy[i]) {
            validated_out[i] = false;
            continue;
        }
        int prev = (i - 1 + n) % n;
        int next = (i + 1) % n;
        validated_out[i] = copy[prev] || copy[next];
    }
}

// === Centroide angular ===

AngleResult lf_compute_centroid_angle(const bool* validated_white,
                                       const uint16_t* filtered_raw,
                                       const uint16_t* threshold,
                                       const uint16_t* white_avg,
                                       int n_sensors) {
    AngleResult r{};
    if (n_sensors <= 0) { r.valid = false; return r; }

    const float deg_per_sensor = 360.0f / static_cast<float>(n_sensors);
    float sum_x = 0.0f, sum_y = 0.0f;
    int depth = 0;

    for (int i = 0; i < n_sensors; ++i) {
        if (!validated_white[i]) continue;
        depth++;

        // Peso por intensidad relativa (cuán "blanco" es).
        float weight = 1.0f;
        if (white_avg[i] > threshold[i]) {
            const int delta = filtered_raw[i] - threshold[i];
            const int range = white_avg[i] - threshold[i];
            weight = static_cast<float>(delta) / static_cast<float>(range);
            if (weight < 0.0f) weight = 0.0f;
            if (weight > 1.0f) weight = 1.0f;
        }

        const float theta_rad = i * deg_per_sensor * (PI_F / 180.0f);
        sum_x += weight * std::cos(theta_rad);
        sum_y += weight * std::sin(theta_rad);
    }

    r.depth = depth;
    r.valid = (depth > 0);
    if (r.valid) {
        r.angle_rad = std::atan2(sum_y, sum_x);
        r.angle_deg = r.angle_rad * (180.0f / PI_F);
    } else {
        r.angle_rad = 0.0f;
        r.angle_deg = 0.0f;
    }
    return r;
}

// === Lifted detector ===

void lf_lifted_update(LiftedDetector& state,
                      uint32_t now_ms,
                      const uint16_t* filtered_raw,
                      const uint16_t* carpet_avg,
                      int n_sensors) {
    int low_count = 0;
    for (int i = 0; i < n_sensors; ++i) {
        // Un sensor está "en aire" si su lectura está claramente debajo del carpet.
        const uint16_t threshold_air = (carpet_avg[i] > LF_LIFTED_DELTA_BELOW)
                                       ? (carpet_avg[i] - LF_LIFTED_DELTA_BELOW)
                                       : 0;
        if (filtered_raw[i] < threshold_air) {
            low_count++;
        }
    }

    const bool candidate = (low_count >= LF_LIFTED_MIN_SENSORS);

    if (candidate) {
        if (!state.candidate_active) {
            // Arranca el candidato — guardar el timestamp incluso si es 0.
            state.candidate_active = true;
            state.candidate_start_ms = now_ms;
        }
        // Activar lifted cuando el candidato pasa el debounce.
        if (now_ms - state.candidate_start_ms >= LF_LIFTED_DEBOUNCE_MS) {
            state.is_lifted = true;
        }
    } else {
        // Sin candidato → desactivar inmediato (el robot volvió al piso).
        state.candidate_active = false;
        state.candidate_start_ms = 0;
        state.is_lifted = false;
    }
}

// === MuxWatchdog (TEMA 1 P0 — 2026-05-29) ===

void mw_init(MuxWatchdog& w) {
    std::memset(&w, 0, sizeof(w));
}

void mw_update(MuxWatchdog& w,
               const uint16_t* raw,
               int n_muxes,
               int sensors_per_mux,
               uint32_t now_ms,
               uint32_t stuck_debounce_ms,
               uint16_t extreme_low,
               uint16_t extreme_high) {
    if (raw == nullptr || n_muxes <= 0 || sensors_per_mux <= 0) return;
    if (n_muxes > MW_MAX_MUXES) n_muxes = MW_MAX_MUXES;

    for (int m = 0; m < n_muxes; ++m) {
        const int base = m * sensors_per_mux;
        const uint16_t first = raw[base];
        // ¿Los 8 canales del mux dan EXACTAMENTE el mismo valor?
        bool all_equal = true;
        for (int k = 1; k < sensors_per_mux; ++k) {
            if (raw[base + k] != first) { all_equal = false; break; }
        }

        if (all_equal) {
            // Es una "racha igual". Si ya estaba activa con MISMO valor, contamos
            // tiempo. Si cambió el valor común, reiniciamos el reloj (puede ser
            // que el mux esté oscilando entre distintos "stuck values").
            if (!w.stuck_active[m] || w.last_common_value[m] != first) {
                w.stuck_active[m] = true;
                w.stuck_start_ms[m] = now_ms;
                w.last_common_value[m] = first;
            }
            const uint32_t elapsed = now_ms - w.stuck_start_ms[m];
            const bool extreme = (first <= extreme_low) || (first >= extreme_high);
            // Confirmamos muerto sólo si: racha >= debounce Y valor extremo.
            if (elapsed >= stuck_debounce_ms && extreme) {
                w.dead[m] = true;
            }
            // Si la racha es larga pero valor no es extremo, no confirmamos.
            // (Caso: 8 sensores casualmente coinciden en un valor medio — raro
            // pero no imposible bajo iluminación uniforme.)
        } else {
            // Los valores volvieron a variar. Reseteamos racha y auto-recovery.
            w.stuck_active[m] = false;
            w.stuck_start_ms[m] = 0;
            w.last_common_value[m] = 0;
            w.dead[m] = false;
        }
    }
}

bool mw_is_dead(const MuxWatchdog& w, int mux_idx) {
    if (mux_idx < 0 || mux_idx >= MW_MAX_MUXES) return false;
    return w.dead[mux_idx];
}

uint8_t mw_get_dead_bitmap(const MuxWatchdog& w) {
    uint8_t bm = 0;
    for (int m = 0; m < MW_MAX_MUXES; ++m) {
        if (w.dead[m]) bm |= (uint8_t)(1u << m);
    }
    return bm;
}

bool mw_any_dead(const MuxWatchdog& w) {
    for (int m = 0; m < MW_MAX_MUXES; ++m) {
        if (w.dead[m]) return true;
    }
    return false;
}

}  // namespace iitasoccer
