#pragma once
#include <stdint.h>
namespace iitasoccer {
enum MotionIntent { MI_STOP=0, MI_ESCAPE=1, MI_GOTO_BALL=2, MI_KICK=3, MI_HOLD=4 };
struct MotionIn {
    MotionIntent intent;
    int16_t      escape_angle_centideg;
    float        ball_x_mm;
    float        ball_y_mm;
    int16_t      max_speed_mm_s;
};
struct MotionCmd {
    int16_t vx_mm_s;
    int16_t vy_mm_s;
    int16_t omega_centideg_s;
    uint8_t kicker;
};
MotionCmd mt_compute(const MotionIn& in);
}  // namespace iitasoccer
