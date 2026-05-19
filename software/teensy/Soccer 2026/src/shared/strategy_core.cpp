#include "strategy_core.h"
#include <cmath>
namespace iitasoccer {
void sc_init(StrategyCore& sc, ScRole role){
    sc.role = role;
    sc.state = (role==ROLE_GOALKEEPER) ? SC_GK_WAIT : SC_FP_WAIT;
}
static ScState field_tick(StrategyCore& /*sc*/, const ScWorld& w){
    if (!w.match_running) return SC_FP_WAIT;
    if (!w.ball_visible)  return SC_FP_SEEK;
    float dist = sqrtf(w.ball_x_mm*w.ball_x_mm + w.ball_y_mm*w.ball_y_mm);
    if (w.ball_y_mm <= -SC_DEFEND_BEHIND_MM) return SC_FP_DEFEND;
    if (dist <= SC_DRIVE_CLOSE_MM)            return SC_FP_DRIVE;
    return SC_FP_RUSH;
}
static ScState gk_tick(StrategyCore& /*sc*/, const ScWorld& w){
    if (!w.match_running) return SC_GK_WAIT;
    if (!w.ball_visible)  return SC_GK_PATROL;
    float dist = sqrtf(w.ball_x_mm*w.ball_x_mm + w.ball_y_mm*w.ball_y_mm);
    if (dist <= 150.0f) return SC_GK_CLEAR;
    if (dist <= SC_GK_INTERCEPT_MM) return SC_GK_INTERCEPT;
    return SC_GK_PATROL;
}
ScOut sc_tick(StrategyCore& sc, const ScWorld& w, uint32_t /*now_ms*/){
    sc.state = (sc.role==ROLE_GOALKEEPER) ? gk_tick(sc,w) : field_tick(sc,w);
    ScOut o{}; o.state = sc.state; return o;
}
}  // namespace iitasoccer
