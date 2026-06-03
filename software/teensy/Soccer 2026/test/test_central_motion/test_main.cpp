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
}
void test_goto_ball_moves_toward_ball(void){
    MotionIn in{}; in.intent=MI_GOTO_BALL; in.ball_x_mm=0; in.ball_y_mm=400;
    in.max_speed_mm_s=500;
    MotionCmd c = mt_compute(in);
    TEST_ASSERT_TRUE(c.vy_mm_s > 300);
}
void test_stop_is_zero(void){
    MotionIn in{}; in.intent=MI_STOP;
    MotionCmd c = mt_compute(in);
    TEST_ASSERT_EQUAL_INT16(0, c.vx_mm_s);
    TEST_ASSERT_EQUAL_INT16(0, c.vy_mm_s);
}

void test_escape_forward_is_vy_only(void){
    MotionIn in{}; in.intent=MI_ESCAPE; in.escape_angle_centideg=0;   // 0°=frente
    in.max_speed_mm_s=500;
    MotionCmd c = mt_compute(in);
    TEST_ASSERT_INT_WITHIN(5, 0, c.vx_mm_s);
    TEST_ASSERT_TRUE(c.vy_mm_s > 480);                                 // ~+sp
}

void test_escape_backward_is_negative_vy(void){
    MotionIn in{}; in.intent=MI_ESCAPE; in.escape_angle_centideg=18000; // 180°=atras
    in.max_speed_mm_s=500;
    MotionCmd c = mt_compute(in);
    TEST_ASSERT_INT_WITHIN(5, 0, c.vx_mm_s);
    TEST_ASSERT_TRUE(c.vy_mm_s < -480);                                 // ~-sp
}

void test_goto_ball_diagonal_normalized(void){
    MotionIn in{}; in.intent=MI_GOTO_BALL; in.ball_x_mm=300; in.ball_y_mm=400;
    in.max_speed_mm_s=500;                                              // n=500
    MotionCmd c = mt_compute(in);
    TEST_ASSERT_INT_WITHIN(20, 300, c.vx_mm_s);                         // 500*0.6
    TEST_ASSERT_INT_WITHIN(20, 400, c.vy_mm_s);                         // 500*0.8
}

void test_goto_ball_at_robot_is_zero(void){
    MotionIn in{}; in.intent=MI_GOTO_BALL; in.ball_x_mm=0; in.ball_y_mm=0;
    in.max_speed_mm_s=500;
    MotionCmd c = mt_compute(in);
    TEST_ASSERT_EQUAL_INT16(0, c.vx_mm_s);
    TEST_ASSERT_EQUAL_INT16(0, c.vy_mm_s);
}

void test_hold_is_zero(void){
    MotionIn in{}; in.intent=MI_HOLD; in.max_speed_mm_s=500;
    MotionCmd c = mt_compute(in);
    TEST_ASSERT_EQUAL_INT16(0, c.vx_mm_s);
    TEST_ASSERT_EQUAL_INT16(0, c.vy_mm_s);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_escape_moves_along_escape_angle);
    RUN_TEST(test_goto_ball_moves_toward_ball);
    RUN_TEST(test_stop_is_zero);
    RUN_TEST(test_escape_forward_is_vy_only);
    RUN_TEST(test_escape_backward_is_negative_vy);
    RUN_TEST(test_goto_ball_diagonal_normalized);
    RUN_TEST(test_goto_ball_at_robot_is_zero);
    RUN_TEST(test_hold_is_zero);
    return UNITY_END();
}
