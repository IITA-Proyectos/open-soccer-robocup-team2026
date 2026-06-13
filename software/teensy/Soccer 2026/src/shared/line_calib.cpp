#include "line_calib.h"
#include <cmath>
namespace iitasoccer {
static uint16_t mid(uint16_t a, uint16_t b){ return (uint16_t)(((uint32_t)a+b)/2); }
void lc_set_static(SensorCalib& c, uint16_t carpet, uint16_t white){
    c.carpet=carpet; c.white=white; c.threshold=mid(carpet,white);
    c.enabled=1; c.sensitivity=0;
}
uint16_t lc_threshold_with_sens(uint16_t carpet, uint16_t white,
                                int global_sens, int per_sensor_sens){
    int total = global_sens + per_sensor_sens;
    if(total >  100) total =  100;
    if(total < -100) total = -100;
    const float midp = ((float)carpet + (float)white) * 0.5f;
    const float half = ((float)white - (float)carpet) * 0.5f;   // signed
    float th = midp + ((float)total / 100.0f) * half;            // +% → hacia white
    // Clampear al rango físico del sensor (robusto ante calib invertida white<carpet).
    float lo = (carpet < white) ? (float)carpet : (float)white;
    float hi = (carpet < white) ? (float)white  : (float)carpet;
    if(th < lo) th = lo;
    if(th > hi) th = hi;
    if(th < 0.0f)     th = 0.0f;
    if(th > 65535.0f) th = 65535.0f;
    return (uint16_t)lroundf(th);
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
int lc_count_weak(const SensorCalib* cs, int n, uint16_t min_margin,
                  uint32_t* weak_mask_out){
    if(weak_mask_out) *weak_mask_out = 0;
    int count = 0;
    for(int i=0;i<n;++i){
        int m = (int)cs[i].white - (int)cs[i].carpet; if(m<0)m=-m;
        if(m < (int)min_margin){
            ++count;
            if(weak_mask_out && i < 32) *weak_mask_out |= (1u << i);
        }
    }
    return count;
}
}  // namespace iitasoccer
