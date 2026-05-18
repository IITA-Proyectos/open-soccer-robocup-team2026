#include "surface_monitor.h"
namespace iitasoccer {
bool sm_update(SurfaceMonitor& s, const uint16_t* f, const uint16_t* c, int n,
                uint32_t now, uint32_t debounce, int min_s, uint16_t db){
    int below=0;
    for(int i=0;i<n;++i){ if((int)f[i] < (int)c[i] - (int)db) ++below; }
    bool cand = (below >= min_s);
    if(cand){
        if(!s.cand){ s.cand=true; s.cand_since_ms=now; }
        return (now - s.cand_since_ms) >= debounce;
    }
    s.cand=false;
    return false;
}
uint8_t sm_data_valid(bool lifted, bool suspect){
    return (lifted || suspect) ? 0 : 1;
}
}  // namespace iitasoccer
