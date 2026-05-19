#pragma once
#include <stdint.h>
namespace iitasoccer {
enum BallTrajKind { BT_STILL=0, BT_TO_OPP_GOAL=1, BT_TO_OWN_GOAL=2, BT_OTHER=3 };
struct BallTrajIn {
    int16_t ball_vx_mm_s;
    int16_t ball_vy_mm_s;
    int16_t goal_opp_angle_centideg;
    int16_t goal_own_angle_centideg;
    int16_t ball_speed_min_mm_s;
    int16_t toward_tol_centideg;       // debe ser > 0; 0 deshabilita la clasificacion de arcos
    int16_t ball_dist_mm;
    int16_t reach_mm;
};
struct BallTraj {
    BallTrajKind kind;
    int16_t      heading_centideg;
    int16_t      speed_mm_s;
    bool         in_reach;
};
BallTraj bt_classify(const BallTrajIn& in);
}  // namespace iitasoccer
