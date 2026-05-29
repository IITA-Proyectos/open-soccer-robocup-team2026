// sensor_health.cpp — implementación. Ver sensor_health.h para contrato.

#include "sensor_health.h"
#include <cstring>

namespace iitasoccer {

void sh_init(SensorHealth& s) {
    std::memset(&s, 0, sizeof(s));
    // unhealthy[] queda en false (memset 0 = todos sanos al inicio).
    // window_initialized queda en false → se inicializa en el primer sh_update.
}

void sh_update(SensorHealth& s,
               const uint16_t* raw,
               const bool* is_white,
               int n_sensors,
               uint32_t now_ms,
               uint32_t window_ms,
               uint16_t max_transitions,
               uint32_t max_stuck_ms) {
    if (raw == nullptr || is_white == nullptr) return;
    if (n_sensors <= 0) return;
    if (n_sensors > SH_MAX_SENSORS) n_sensors = SH_MAX_SENSORS;

    // Inicializar ventana en el primer tick.
    if (!s.window_initialized) {
        s.window_start_ms = now_ms;
        s.window_initialized = true;
        for (int i = 0; i < n_sensors; ++i) {
            s.was_white[i] = is_white[i];
            s.last_raw[i]  = raw[i];
            s.same_raw_start_ms[i] = now_ms;
            s.same_raw_active[i]   = true;
        }
        return;  // No evaluamos nada todavía — necesitamos al menos 1 ventana.
    }

    // === Update por sensor (cada tick) ===
    for (int i = 0; i < n_sensors; ++i) {
        // Transiciones blanco↔no-blanco
        if (is_white[i] != s.was_white[i]) {
            // Saturar a 65535 para evitar wrap (improbable pero defensivo).
            if (s.transition_count[i] < 0xFFFF) ++s.transition_count[i];
            s.was_white[i] = is_white[i];
        }
        // Stuck: tiempo con el mismo raw EXACTO
        if (raw[i] != s.last_raw[i]) {
            s.last_raw[i]          = raw[i];
            s.same_raw_start_ms[i] = now_ms;
            s.same_raw_active[i]   = true;
        }
        // (Si no cambió, same_raw_start_ms[i] queda y la "racha" sigue
        // creciendo hasta el próximo cambio.)
    }

    // === Evaluación al cierre de la ventana ===
    // Wrap-safe: si now_ms < window_start_ms (millis() wrapeó tras ~49.7 días
    // o el caller pasó tiempo no-monótono), reiniciamos la ventana en lugar de
    // disparar evaluación inmediata por elapsed gigantesco.
    const int32_t window_elapsed_signed = (int32_t)(now_ms - s.window_start_ms);
    if (window_elapsed_signed < 0) {
        s.window_start_ms = now_ms;
        for (int i = 0; i < n_sensors; ++i) {
            s.transition_count[i] = 0;
            // No reseteamos same_raw_start_ms: si el sensor sigue stuck con
            // el MISMO valor pre y post wrap, el próximo tick lo retomará
            // ajustado naturalmente.
        }
        return;
    }
    if ((uint32_t)window_elapsed_signed >= window_ms) {
        for (int i = 0; i < n_sensors; ++i) {
            const bool too_noisy = s.transition_count[i] > max_transitions;
            // Wrap-safe stuck duration.
            uint32_t stuck_dur = 0;
            if (s.same_raw_active[i]) {
                const int32_t sd_signed =
                    (int32_t)(now_ms - s.same_raw_start_ms[i]);
                if (sd_signed >= 0) stuck_dur = (uint32_t)sd_signed;
                else                s.same_raw_start_ms[i] = now_ms;  // post-wrap reseat
            }
            const bool stuck = stuck_dur >= max_stuck_ms;

            // Auto-recovery: si el sensor cumple AMBAS condiciones de buena
            // salud en esta ventana, vuelve a healthy.
            s.unhealthy[i] = too_noisy || stuck;

            // Reset del contador de transiciones para la próxima ventana.
            s.transition_count[i] = 0;
        }
        s.window_start_ms = now_ms;
    }
}

bool sh_is_healthy(const SensorHealth& s, int sensor_idx) {
    // Fix audit 2026-05-29: OOB → false (unhealthy = exclusión defensiva).
    // Antes: OOB → true. Cambio: si un bug upstream pasa i+1 sin bounds check,
    // el sensor "fantasma" se excluye del centroide en lugar de contaminarlo.
    // El llamador down_model ya filtra con `i < n` antes de llamar, así que
    // este cambio sólo afecta llamadores defectuosos — y los hace más seguros.
    if (sensor_idx < 0 || sensor_idx >= SH_MAX_SENSORS) return false;
    return !s.unhealthy[sensor_idx];
}

bool sh_any_unhealthy(const SensorHealth& s, int n_sensors) {
    if (n_sensors > SH_MAX_SENSORS) n_sensors = SH_MAX_SENSORS;
    for (int i = 0; i < n_sensors; ++i) {
        if (s.unhealthy[i]) return true;
    }
    return false;
}

uint32_t sh_get_unhealthy_bitmap(const SensorHealth& s, int n_sensors) {
    if (n_sensors > SH_MAX_SENSORS) n_sensors = SH_MAX_SENSORS;
    uint32_t bm = 0;
    for (int i = 0; i < n_sensors; ++i) {
        if (s.unhealthy[i]) bm |= (uint32_t)(1u << i);
    }
    return bm;
}

uint8_t sh_healthy_count(const SensorHealth& s, int n_sensors) {
    if (n_sensors > SH_MAX_SENSORS) n_sensors = SH_MAX_SENSORS;
    uint8_t count = 0;
    for (int i = 0; i < n_sensors; ++i) {
        if (!s.unhealthy[i]) ++count;
    }
    return count;
}

}  // namespace iitasoccer
