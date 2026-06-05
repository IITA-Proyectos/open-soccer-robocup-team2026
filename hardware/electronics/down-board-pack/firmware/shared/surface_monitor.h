// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#pragma once
#include <stdint.h>
namespace iitasoccer {
struct SurfaceMonitor { bool cand; uint32_t cand_since_ms; };
// Detecta robot levantado: ≥ min_sensors leen filtered < carpet - delta_below
// de forma sostenida ≥ debounce_ms. Devuelve true mientras "levantado".
bool sm_update(SurfaceMonitor& s, const uint16_t* filtered,
                const uint16_t* carpet, int n, uint32_t now_ms,
                uint32_t debounce_ms, int min_sensors, uint16_t delta_below);
// Compuerta maestra del contrato: 1 solo si NO lifted y NO calib suspect.
uint8_t sm_data_valid(bool lifted, bool calib_suspect);
}  // namespace iitasoccer
