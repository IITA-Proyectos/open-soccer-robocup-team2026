#include "line_tracker.h"
namespace iitasoccer {
bool lt_update(LineTracker& t, bool present, uint32_t now, uint32_t min_ms){
    bool fired=false;
    if(present){
        if(!t.since_valid){ t.present_since_ms=now; t.since_valid=true; }
        if(now - t.present_since_ms >= min_ms) t.had_sustained=true;
    } else {
        if(t.present_prev && t.had_sustained) fired=true;
        t.had_sustained=false; t.since_valid=false;
    }
    t.present_prev=present;
    return fired;
}
}  // namespace iitasoccer
