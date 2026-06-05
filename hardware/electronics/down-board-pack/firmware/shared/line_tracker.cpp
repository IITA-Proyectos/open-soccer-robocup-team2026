// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#include "line_tracker.h"
namespace iitasoccer {
bool lt_update(LineTracker& t, bool present_now, uint32_t now_ms, uint32_t min_track_ms){
    bool fired=false;
    if(present_now){
        if(!t.since_valid){ t.present_since_ms=now_ms; t.since_valid=true; }
        if(now_ms - t.present_since_ms >= min_track_ms) t.had_sustained=true;
    } else {
        if(t.present_prev && t.had_sustained) fired=true;
        t.had_sustained=false; t.since_valid=false;
    }
    t.present_prev=present_now;
    return fired;
}
}  // namespace iitasoccer
