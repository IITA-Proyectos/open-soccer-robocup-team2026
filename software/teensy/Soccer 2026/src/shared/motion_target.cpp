// ⚠️ CÓDIGO MUERTO (dead-code) — mt_compute() NO tiene callers en producción.
// Solo lo ejercita test_central_motion; strategy.cpp NO lo invoca (usa behind_ball
// + drive_straight + ball_predict). Conservado como esqueleto forward-looking; ver
// el banner de motion_target.h. Eliminar junto a su test cuando se confirme que no
// se usará. (P2-DEADCODE-CLEANUP / P2-DOC-VARIOS — solo comentario, sin cambio de
// comportamiento.)
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
    }
    // MI_STOP / MI_HOLD: c queda en cero (placeholder; HOLD == STOP hasta Plan 2).
    return c;
}
}  // namespace iitasoccer
