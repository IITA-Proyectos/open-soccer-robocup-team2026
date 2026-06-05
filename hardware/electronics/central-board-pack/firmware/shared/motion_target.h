// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#pragma once
#include <stdint.h>
namespace iitasoccer {
enum MotionIntent { MI_STOP=0, MI_ESCAPE=1, MI_GOTO_BALL=2, MI_KICK=3, MI_HOLD=4 };
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
    uint8_t kicker;
};
MotionCmd mt_compute(const MotionIn& in);
}  // namespace iitasoccer
