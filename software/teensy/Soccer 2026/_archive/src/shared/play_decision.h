#pragma once
#include <stdint.h>
#include "ball_trajectory.h"
namespace iitasoccer {
enum PlayAction { PLAY_NONE=0, PLAY_LET_CIRCULATE=1, PLAY_INTERCEPT=2, PLAY_DEFLECT_TO_OPP=3 };
struct PlayIn {
    bool         in_reach;
    bool         moving;
    BallTrajKind kind;
};
struct PlayOut { PlayAction action; };
PlayOut pd_decide(const PlayIn& in);
}  // namespace iitasoccer
