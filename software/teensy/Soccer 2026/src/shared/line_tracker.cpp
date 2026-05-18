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
