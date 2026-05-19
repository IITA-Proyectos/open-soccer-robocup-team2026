#include "play_decision.h"
namespace iitasoccer {
PlayOut pd_decide(const PlayIn& in){
    PlayOut o{}; o.action = PLAY_NONE;
    if (!in.in_reach || !in.moving) return o;
    switch (in.kind){
        case BT_TO_OPP_GOAL: o.action = PLAY_LET_CIRCULATE;  break;
        case BT_TO_OWN_GOAL: o.action = PLAY_INTERCEPT;       break;
        case BT_OTHER:       o.action = PLAY_DEFLECT_TO_OPP;  break;
        case BT_STILL:       o.action = PLAY_NONE;            break;
    }
    return o;
}
}  // namespace iitasoccer
