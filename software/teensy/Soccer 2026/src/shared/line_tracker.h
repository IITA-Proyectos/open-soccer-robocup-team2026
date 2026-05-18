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
