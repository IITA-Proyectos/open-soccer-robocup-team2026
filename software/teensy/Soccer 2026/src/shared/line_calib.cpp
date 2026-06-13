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
    // NO-OP EXACTO con sens=0: reproducir la aritmética entera histórica mid()
    // (TRUNCACIÓN), NO lroundf (redondeo) — difieren en 1 para carpet+white impar.
    // Sin esto, ~la mitad de los sensores arrancaban con umbral +1 vs competencia
    // (bug cazado por revisión adversarial 2026-06-13; antes invisible porque los
    // tests usaban sumas pares). El redondeo float queda solo para sens!=0.
    if(total == 0) return mid(carpet, white);
    const float midp = ((float)carpet + (float)white) * 0.5f;
    float th;
    if(total < 0){
        // MÁS sensible: interpola punto medio → 0. A -100 el umbral es 0, así que
        // TODOS los sensores leen blanco (raw >= 0 siempre). Satura.
        th = midp * (float)(100 + total) / 100.0f;
    } else {
        // MENOS sensible: interpola punto medio → (2*white - carpet), que supera
        // 'white' por un margen completo. A +100 NINGÚN sensor llega al umbral
        // (las lecturas de blanco ~white < 2*white-carpet). Satura.
        const float hi = 2.0f * (float)white - (float)carpet;
        th = midp + ((float)total / 100.0f) * (hi - midp);
    }
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
