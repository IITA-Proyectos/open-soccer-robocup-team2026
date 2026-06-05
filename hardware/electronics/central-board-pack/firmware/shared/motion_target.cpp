// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#include "motion_target.h"
#include <cmath>
namespace iitasoccer {
MotionCmd mt_compute(const MotionIn& in){
    MotionCmd c{};
    float sp = (float)in.max_speed_mm_s;
    if (in.intent == MI_ESCAPE){
        float a = (in.escape_angle_centideg / 100.0f) * (float)M_PI / 180.0f;
        c.vx_mm_s = (int16_t)lroundf(sp * sinf(a));
        c.vy_mm_s = (int16_t)lroundf(sp * cosf(a));
    } else if (in.intent == MI_GOTO_BALL){
        float n = sqrtf(in.ball_x_mm*in.ball_x_mm + in.ball_y_mm*in.ball_y_mm);
        if (n > 1.0f){
            c.vx_mm_s = (int16_t)lroundf(sp * in.ball_x_mm / n);
            c.vy_mm_s = (int16_t)lroundf(sp * in.ball_y_mm / n);
        }
    } else if (in.intent == MI_KICK){
        c.kicker = 1;
    }
    // MI_STOP / MI_HOLD: c queda en cero (placeholder; HOLD == STOP hasta Plan 2).
    return c;
}
}  // namespace iitasoccer
