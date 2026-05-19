#pragma once
#include <stdint.h>
namespace iitasoccer {
enum ScRole { ROLE_GOALKEEPER=0, ROLE_FIELD=1 };
enum ScState {
    SC_FP_WAIT=0, SC_FP_RUSH, SC_FP_SEEK, SC_FP_DRIVE, SC_FP_DEFEND,
    SC_GK_WAIT, SC_GK_PATROL, SC_GK_INTERCEPT, SC_GK_CLEAR
};
struct ScWorld {
    bool     match_running;
    bool     ball_visible;
    float    ball_x_mm;
    float    ball_y_mm;
    bool     goal_opp_visible;
    int16_t  goal_opp_angle_centideg;
};
struct StrategyCore { ScRole role; ScState state; };
struct ScOut { ScState state; };
void  sc_init(StrategyCore& sc, ScRole role);
ScOut sc_tick(StrategyCore& sc, const ScWorld& w, uint32_t now_ms);
constexpr float SC_DEFEND_BEHIND_MM   = 400.0f;
constexpr float SC_DRIVE_CLOSE_MM     = 150.0f;
constexpr float SC_GK_CLEAR_MM        = 150.0f;
constexpr float SC_GK_INTERCEPT_MM    = 600.0f;
}  // namespace iitasoccer
