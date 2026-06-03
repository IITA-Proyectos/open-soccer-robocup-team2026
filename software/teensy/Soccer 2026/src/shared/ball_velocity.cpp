#include "ball_velocity.h"

namespace iitasoccer {

namespace {
constexpr float    DEFAULT_EMA_ALPHA  = 0.4f;   // 0.3–0.5 según el análisis (§4)
constexpr uint32_t DEFAULT_MAX_GAP_MS = 200;    // ~6 packets a 30 Hz

inline int16_t clamp_to_i16(float v) {
    if (v >= 32767.0f)  return 32767;
    if (v <= -32768.0f) return -32768;
    return static_cast<int16_t>(v >= 0.0f ? v + 0.5f : v - 0.5f);  // redondeo simétrico
}
}  // namespace

BallVelocityParams ball_velocity_default_params() {
    return BallVelocityParams{DEFAULT_EMA_ALPHA, DEFAULT_MAX_GAP_MS};
}

void ball_velocity_reset(BallVelocityState& s) {
    s.has_point = false;
    s.valid     = false;
    s.last_x_mm = 0.0f;
    s.last_y_mm = 0.0f;
    s.last_ms   = 0;
    s.vx_mm_s   = 0.0f;
    s.vy_mm_s   = 0.0f;
}

void ball_velocity_update(BallVelocityState& s,
                          const BallVelocityParams& p,
                          int16_t x_mm, int16_t y_mm,
                          bool visible, uint32_t sample_ms) {
    // Sin pelota → reset (al reaparecer se descarta el primer frame).
    if (!visible) {
        ball_velocity_reset(s);
        return;
    }

    // Primera muestra desde que la vemos: sólo sembrar el punto de referencia.
    if (!s.has_point) {
        s.has_point = true;
        s.valid     = false;
        s.last_x_mm = static_cast<float>(x_mm);
        s.last_y_mm = static_cast<float>(y_mm);
        s.last_ms   = sample_ms;
        s.vx_mm_s   = 0.0f;
        s.vy_mm_s   = 0.0f;
        return;
    }

    // Sin dato nuevo (el packet no avanzó): mantener la última estimación.
    if (sample_ms <= s.last_ms) {
        return;
    }

    const uint32_t dt_ms = sample_ms - s.last_ms;

    // Gap demasiado largo: no confiamos en la derivada → re-sembrar.
    if (dt_ms > p.max_gap_ms) {
        s.valid     = false;
        s.last_x_mm = static_cast<float>(x_mm);
        s.last_y_mm = static_cast<float>(y_mm);
        s.last_ms   = sample_ms;
        s.vx_mm_s   = 0.0f;
        s.vy_mm_s   = 0.0f;
        return;
    }

    const float inv_dt_s = 1000.0f / static_cast<float>(dt_ms);   // 1/seg
    const float inst_vx = (static_cast<float>(x_mm) - s.last_x_mm) * inv_dt_s;
    const float inst_vy = (static_cast<float>(y_mm) - s.last_y_mm) * inv_dt_s;

    if (s.valid) {
        const float a = p.ema_alpha;
        s.vx_mm_s = a * inst_vx + (1.0f - a) * s.vx_mm_s;
        s.vy_mm_s = a * inst_vy + (1.0f - a) * s.vy_mm_s;
    } else {
        // Primera derivada: siembra el EMA sin mezclar.
        s.vx_mm_s = inst_vx;
        s.vy_mm_s = inst_vy;
        s.valid   = true;
    }

    s.last_x_mm = static_cast<float>(x_mm);
    s.last_y_mm = static_cast<float>(y_mm);
    s.last_ms   = sample_ms;
}

int16_t ball_velocity_vx_mm_s(const BallVelocityState& s) {
    return s.valid ? clamp_to_i16(s.vx_mm_s) : 0;
}
int16_t ball_velocity_vy_mm_s(const BallVelocityState& s) {
    return s.valid ? clamp_to_i16(s.vy_mm_s) : 0;
}

}  // namespace iitasoccer
