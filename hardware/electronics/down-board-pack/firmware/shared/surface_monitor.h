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
