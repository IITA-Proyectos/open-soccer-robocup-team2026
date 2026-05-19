// pio test -e test_native -f test_central_motion
#include <unity.h>
#include "motion_target.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

void test_escape_moves_along_escape_angle(void){
    MotionIn in{}; in.intent=MI_ESCAPE; in.escape_angle_centideg=9000;
    in.max_speed_mm_s=500;
    MotionCmd c = mt_compute(in);
    TEST_ASSERT_TRUE(c.vx_mm_s > 300);
    TEST_ASSERT_INT_WITHIN(80, 0, c.vy_mm_s);
    TEST_ASSERT_EQUAL_UINT8(0, c.kicker);
}
void test_goto_ball_moves_toward_ball(void){
    MotionIn in{}; in.intent=MI_GOTO_BALL; in.ball_x_mm=0; in.ball_y_mm=400;
    in.max_speed_mm_s=500;
    MotionCmd c = mt_compute(in);
    TEST_ASSERT_TRUE(c.vy_mm_s > 300);
}
void test_kick_sets_kicker(void){
    MotionIn in{}; in.intent=MI_KICK; in.max_speed_mm_s=500;
    MotionCmd c = mt_compute(in);
    TEST_ASSERT_EQUAL_UINT8(1, c.kicker);
}
void test_stop_is_zero(void){
    MotionIn in{}; in.intent=MI_STOP;
    MotionCmd c = mt_compute(in);
    TEST_ASSERT_EQUAL_INT16(0, c.vx_mm_s);
    TEST_ASSERT_EQUAL_INT16(0, c.vy_mm_s);
    TEST_ASSERT_EQUAL_UINT8(0, c.kicker);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_escape_moves_along_escape_angle);
    RUN_TEST(test_goto_ball_moves_toward_ball);
    RUN_TEST(test_kick_sets_kicker);
    RUN_TEST(test_stop_is_zero);
    return UNITY_END();
}
