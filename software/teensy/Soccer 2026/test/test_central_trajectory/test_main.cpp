// pio test -e test_native -f test_central_trajectory
#include <unity.h>
#include "ball_trajectory.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

void test_no_motion_is_still(void){
    BallTrajIn in{}; in.ball_vx_mm_s=0; in.ball_vy_mm_s=0;
    in.ball_speed_min_mm_s=80;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_EQUAL_INT(BT_STILL, t.kind);
}
void test_toward_opponent_goal(void){
    BallTrajIn in{};
    in.ball_vx_mm_s=0; in.ball_vy_mm_s=500;
    in.goal_opp_angle_centideg=0;
    in.goal_own_angle_centideg=18000;
    in.ball_speed_min_mm_s=80; in.toward_tol_centideg=4500;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_EQUAL_INT(BT_TO_OPP_GOAL, t.kind);
}
void test_toward_own_goal(void){
    BallTrajIn in{};
    in.ball_vx_mm_s=0; in.ball_vy_mm_s=-500;
    in.goal_opp_angle_centideg=0;
    in.goal_own_angle_centideg=18000;
    in.ball_speed_min_mm_s=80; in.toward_tol_centideg=4500;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_EQUAL_INT(BT_TO_OWN_GOAL, t.kind);
}
void test_sideways_is_other(void){
    BallTrajIn in{};
    in.ball_vx_mm_s=500; in.ball_vy_mm_s=0;
    in.goal_opp_angle_centideg=0;
    in.goal_own_angle_centideg=18000;
    in.ball_speed_min_mm_s=80; in.toward_tol_centideg=4500;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_EQUAL_INT(BT_OTHER, t.kind);
}
void test_in_reach_flag(void){
    BallTrajIn in{};
    in.ball_vx_mm_s=0; in.ball_vy_mm_s=300; in.ball_speed_min_mm_s=80;
    in.ball_dist_mm=250; in.reach_mm=400;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_TRUE(t.in_reach);
    in.ball_dist_mm=900;
    t = bt_classify(in);
    TEST_ASSERT_FALSE(t.in_reach);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_no_motion_is_still);
    RUN_TEST(test_toward_opponent_goal);
    RUN_TEST(test_toward_own_goal);
    RUN_TEST(test_sideways_is_other);
    RUN_TEST(test_in_reach_flag);
    return UNITY_END();
}
