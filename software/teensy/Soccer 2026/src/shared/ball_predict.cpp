#include "ball_predict.h"

namespace iitasoccer {

namespace {
constexpr float DEFAULT_LOOKAHEAD_S = 0.2f;
constexpr float DEFAULT_MAX_LEAD_MM = 400.0f;

inline float clamp_lead(float lead, float max_lead_mm) {
    if (lead >  max_lead_mm) return  max_lead_mm;
    if (lead < -max_lead_mm) return -max_lead_mm;
    return lead;
}
}  // namespace

BallPredictParams ball_predict_default_params() {
    return BallPredictParams{ DEFAULT_LOOKAHEAD_S, DEFAULT_MAX_LEAD_MM };
}

BallPredictOut ball_predict(int16_t ball_x_mm, int16_t ball_y_mm,
                            int16_t ball_vx_mm_s, int16_t ball_vy_mm_s,
                            const BallPredictParams& p) {
    // lead = clamp(v * lookahead, -max_lead, +max_lead).
    // Con vx=vy=0 (pelota quieta o velocidad N/A) el lead es 0 → p = pos:
    // conducta IDÉNTICA a perseguir la posición actual (fallback automático).
    const float lead_x = clamp_lead(static_cast<float>(ball_vx_mm_s) * p.lookahead_s, p.max_lead_mm);
    const float lead_y = clamp_lead(static_cast<float>(ball_vy_mm_s) * p.lookahead_s, p.max_lead_mm);

    BallPredictOut out;
    out.px_mm = static_cast<float>(ball_x_mm) + lead_x;
    out.py_mm = static_cast<float>(ball_y_mm) + lead_y;
    return out;
}

}  // namespace iitasoccer
