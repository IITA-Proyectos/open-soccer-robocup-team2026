// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
// sensor_health.h — Tracking per-sensor de "salud" runtime.
//
// Tema 4 P1 del research doc 2026-05-24-deteccion-linea-down.
//
// Detecta dos modos de falla individuales en los 32 sensores del anillo:
//
//   1. RUIDOSO crónico: sensor que oscila blanco↔no-blanco más de
//      SH_MAX_TRANSITIONS_PER_WINDOW veces por ventana de 1 segundo.
//      Causa típica: soldadura intermitente, ruido eléctrico local, LED
//      desencajado parcialmente del lente del fototransistor.
//
//   2. STUCK: sensor que devuelve EXACTAMENTE el mismo valor RAW durante
//      más de SH_MAX_STUCK_MS milisegundos consecutivos. Causa típica:
//      fototransistor muerto, traza partida, sensor desoldado.
//
// Cuando un sensor entra en cualquiera de esos modos, el down_model lo
// FILTRA del cálculo del centroide (lo trata como NO blanco). Esto
// previene que un sensor enfermo contamine la geometría.
//
// Auto-recovery: si en la siguiente ventana el sensor vuelve a comportarse
// normal (< límites), retorna automáticamente a HEALTHY. No requiere reset.
//
// Diferencias clave con MuxWatchdog:
//   - MuxWatchdog detecta mux ENTERO muerto (8 sensores al mismo valor).
//   - sensor_health detecta UN sensor individual ruidoso/stuck.
//   - Funcionan en paralelo, complementarios.

#pragma once
#include <stdint.h>

namespace iitasoccer {

constexpr int      SH_MAX_SENSORS                = 32;
constexpr uint32_t SH_WINDOW_MS                  = 1000;  // ventana de evaluación
constexpr uint16_t SH_MAX_TRANSITIONS_PER_WINDOW = 20;    // >20 transiciones/seg = ruido
constexpr uint32_t SH_MAX_STUCK_MS               = 5000;  // mismo raw por >5s = stuck

struct SensorHealth {
    // Tracking de transiciones blanco↔no-blanco DURANTE la ventana actual.
    uint16_t transition_count[SH_MAX_SENSORS];
    bool     was_white[SH_MAX_SENSORS];
    // Tracking de stuck-on-same-raw.
    uint16_t last_raw[SH_MAX_SENSORS];
    uint32_t same_raw_start_ms[SH_MAX_SENSORS];
    bool     same_raw_active[SH_MAX_SENSORS];
    // Resultado actualizado al cierre de cada ventana.
    bool     unhealthy[SH_MAX_SENSORS];
    // Bookkeeping de la ventana.
    uint32_t window_start_ms;
    bool     window_initialized;
};

// Inicializa todos los campos a estado limpio (todo healthy, ventana sin iniciar).
void sh_init(SensorHealth& s);

// Update por tick. Tiene que llamarse cada vez que line_ring tickea (1 kHz).
//   raw       = array de n_sensors valores ADC crudos.
//   is_white  = array de n_sensors flags post-hysteresis.
//   n_sensors = cuántos sensores hay (≤ SH_MAX_SENSORS).
//   now_ms    = timestamp monotónico (millis() en hardware).
//
// Internamente:
//   - Cuenta transiciones (is_white != was_white) por sensor.
//   - Mide tiempo con el MISMO raw exacto por sensor.
//   - Cada SH_WINDOW_MS (1s), evalúa si cada sensor excede sus límites,
//     actualiza `unhealthy[i]` y resetea contadores.
void sh_update(SensorHealth& s,
               const uint16_t* raw,
               const bool* is_white,
               int n_sensors,
               uint32_t now_ms,
               uint32_t window_ms = SH_WINDOW_MS,
               uint16_t max_transitions = SH_MAX_TRANSITIONS_PER_WINDOW,
               uint32_t max_stuck_ms = SH_MAX_STUCK_MS);

// True si el sensor i está sano. Out-of-range → false (asume UNHEALTHY como
// defensa: si un bug upstream pasa i+1, el sensor "fantasma" se excluye en
// lugar de contaminar el centroide). Fix de audit 2026-05-29.
bool sh_is_healthy(const SensorHealth& s, int sensor_idx);

// True si CUALQUIER sensor está unhealthy (para flag global EV_SENSOR_NOISY).
bool sh_any_unhealthy(const SensorHealth& s, int n_sensors);

// Bitmap completo: bit i = sensor i unhealthy. uint32_t cubre los 32 sensores.
// (No cabe en LineStatusV2; expuesto para debug y para LineStatusV3 futuro.)
uint32_t sh_get_unhealthy_bitmap(const SensorHealth& s, int n_sensors);

// Cuántos sensores SANOS hay actualmente.
uint8_t sh_healthy_count(const SensorHealth& s, int n_sensors);

}  // namespace iitasoccer
