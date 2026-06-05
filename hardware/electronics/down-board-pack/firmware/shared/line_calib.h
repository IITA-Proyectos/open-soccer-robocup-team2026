// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#pragma once
#include <stdint.h>
namespace iitasoccer {
struct SensorCalib { uint16_t carpet; uint16_t white; uint16_t threshold; };
// Calibración estática: umbral = punto medio carpet/white.
void lc_set_static(SensorCalib& c, uint16_t carpet, uint16_t white);
// Adaptación on-the-fly del baseline de carpet. Solo adapta si el sensor NO
// está sobre línea (gate anti-deriva). alpha ∈ (0,1] = velocidad de adaptación.
void lc_adapt_carpet(SensorCalib& c, uint16_t filtered, bool sensor_on_line,
                      float alpha);
// true si algún sensor no separa piso/blanco (|white-carpet| < min_margin).
bool lc_is_suspect(const SensorCalib* cs, int n, uint16_t min_margin);
}  // namespace iitasoccer
