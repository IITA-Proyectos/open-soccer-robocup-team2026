// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#include "line_calib.h"
#include <cmath>
namespace iitasoccer {
static uint16_t mid(uint16_t a, uint16_t b){ return (uint16_t)(((uint32_t)a+b)/2); }
void lc_set_static(SensorCalib& c, uint16_t carpet, uint16_t white){
    c.carpet=carpet; c.white=white; c.threshold=mid(carpet,white);
}
void lc_adapt_carpet(SensorCalib& c, uint16_t filtered, bool on_line, float alpha){
    if(on_line) return;
    if(alpha<=0.0f) return;
    if(alpha>1.0f) alpha=1.0f;
    float nc = (1.0f-alpha)*(float)c.carpet + alpha*(float)filtered;
    c.carpet = (uint16_t)lroundf(nc);
    c.threshold = mid(c.carpet, c.white);
}
bool lc_is_suspect(const SensorCalib* cs, int n, uint16_t min_margin){
    for(int i=0;i<n;++i){
        int m = (int)cs[i].white - (int)cs[i].carpet; if(m<0)m=-m;
        if(m < (int)min_margin) return true;
    }
    return false;
}
}  // namespace iitasoccer
