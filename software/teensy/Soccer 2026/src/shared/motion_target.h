// motion_target.h — ⚠️ MÓDULO PLACEHOLDER (Plan 1), SIN callers en producción.
// mt_compute() sólo lo usa test_central_motion; strategy.cpp NO lo invoca (usa
// behind_ball + drive_straight + ball_predict). Conservado como esqueleto
// forward-looking; eliminar junto a su test cuando se confirme que no se usará.
#pragma once
#include <stdint.h>
namespace iitasoccer {
enum MotionIntent { MI_STOP=0, MI_ESCAPE=1, MI_GOTO_BALL=2, MI_HOLD=3 };
struct MotionIn {
    MotionIntent intent;
    int16_t      escape_angle_centideg;
    float        ball_x_mm;
    float        ball_y_mm;
    int16_t      max_speed_mm_s;  // debe ser >= 0
};
struct MotionCmd {
    int16_t vx_mm_s;
    int16_t vy_mm_s;
    int16_t omega_centideg_s;   // placeholder: 0 (rotacion la maneja la capa superior / Plan 2)
};
MotionCmd mt_compute(const MotionIn& in);
}  // namespace iitasoccer
