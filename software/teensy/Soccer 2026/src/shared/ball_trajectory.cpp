#include "ball_trajectory.h"
#include <cmath>
namespace iitasoccer {
static int16_t to_cd(float deg){
    while (deg > 180.0f)   deg -= 360.0f;
    while (deg <= -180.0f) deg += 360.0f;
    return (int16_t)lroundf(deg * 100.0f);
}
static int ang_diff_cd(int16_t a, int16_t b){
    int d = (int)a - (int)b;
    while (d > 18000)  d -= 36000;
    while (d <= -18000) d += 36000;
    return d < 0 ? -d : d;
}
BallTraj bt_classify(const BallTrajIn& in){
    BallTraj t{};
    float sp = sqrtf((float)in.ball_vx_mm_s*in.ball_vx_mm_s
                   + (float)in.ball_vy_mm_s*in.ball_vy_mm_s);
    t.speed_mm_s = (int16_t)(sp > 32767.0f ? 32767 : sp);
    t.in_reach = (in.reach_mm > 0) && (in.ball_dist_mm <= in.reach_mm);
    if (sp < (float)in.ball_speed_min_mm_s){
        t.kind = BT_STILL; t.heading_centideg = 0; return t;
    }
    float hdg = atan2f((float)in.ball_vy_mm_s, (float)in.ball_vx_mm_s) * 180.0f / (float)M_PI;
    t.heading_centideg = to_cd(90.0f - hdg);
    int d_opp = ang_diff_cd(t.heading_centideg, in.goal_opp_angle_centideg);
    int d_own = ang_diff_cd(t.heading_centideg, in.goal_own_angle_centideg);
    if (d_opp <= in.toward_tol_centideg && d_opp <= d_own) t.kind = BT_TO_OPP_GOAL;
    else if (d_own <= in.toward_tol_centideg)               t.kind = BT_TO_OWN_GOAL;
    else                                                    t.kind = BT_OTHER;
    return t;
}
}  // namespace iitasoccer
