// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#pragma once
#include <stdint.h>
namespace iitasoccer {
struct LineTracker {
    bool     present_prev;
    bool     had_sustained;
    uint32_t present_since_ms;
    bool     since_valid;
};
// Devuelve true UNA vez en la transición presente→ausente,
// si la línea estuvo presente de forma continua ≥ min_track_ms.
bool lt_update(LineTracker& t, bool present_now, uint32_t now_ms,
                uint32_t min_track_ms);
}  // namespace iitasoccer
