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
