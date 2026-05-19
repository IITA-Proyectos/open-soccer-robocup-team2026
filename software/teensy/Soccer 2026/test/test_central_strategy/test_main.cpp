// pio test -e test_native -f test_central_strategy
#include <unity.h>
#include "strategy_core.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

static ScWorld w(bool run, bool ball, float bx, float by){
    ScWorld s{}; s.match_running=run; s.ball_visible=ball;
    s.ball_x_mm=bx; s.ball_y_mm=by; s.goal_opp_visible=true;
    s.goal_opp_angle_centideg=0; return s;
}
void test_wait_until_match_runs(void){
    StrategyCore sc{}; sc_init(sc, ROLE_FIELD);
    ScOut o = sc_tick(sc, w(false,false,0,0), 0);
    TEST_ASSERT_EQUAL_INT(SC_FP_WAIT, o.state);
}
void test_field_rush_on_start_when_ball_seen(void){
    StrategyCore sc{}; sc_init(sc, ROLE_FIELD);
    ScOut o = sc_tick(sc, w(true,true,100,300), 0);
    TEST_ASSERT_EQUAL_INT(SC_FP_RUSH, o.state);
}
void test_field_seek_on_start_when_no_ball(void){
    StrategyCore sc{}; sc_init(sc, ROLE_FIELD);
    ScOut o = sc_tick(sc, w(true,false,0,0), 0);
    TEST_ASSERT_EQUAL_INT(SC_FP_SEEK, o.state);
}
void test_field_defend_when_ball_behind(void){
    StrategyCore sc{}; sc_init(sc, ROLE_FIELD);
    ScOut o = sc_tick(sc, w(true,true,0,-600), 0);
    TEST_ASSERT_EQUAL_INT(SC_FP_DEFEND, o.state);
}
void test_gk_wait_then_patrol(void){
    StrategyCore sc{}; sc_init(sc, ROLE_GOALKEEPER);
    TEST_ASSERT_EQUAL_INT(SC_GK_WAIT, sc_tick(sc, w(false,false,0,0),0).state);
    TEST_ASSERT_EQUAL_INT(SC_GK_PATROL, sc_tick(sc, w(true,false,0,0),10).state);
}
void test_gk_intercept_when_ball_seen(void){
    StrategyCore sc{}; sc_init(sc, ROLE_GOALKEEPER);
    sc_tick(sc, w(true,false,0,0), 0);
    ScOut o = sc_tick(sc, w(true,true,50,200), 10);
    TEST_ASSERT_EQUAL_INT(SC_GK_INTERCEPT, o.state);
}

void test_field_defend_precedes_drive_when_both_trigger(void){
    StrategyCore sc{}; sc_init(sc, ROLE_FIELD);
    ScOut o = sc_tick(sc, w(true,true,0,-420), 0);   // y<=-400 ⇒ DEFEND antes que DRIVE
    TEST_ASSERT_EQUAL_INT(SC_FP_DEFEND, o.state);
}

void test_field_drive_when_close_and_not_behind(void){
    StrategyCore sc{}; sc_init(sc, ROLE_FIELD);
    ScOut o = sc_tick(sc, w(true,true,0,100), 0);     // dist=100<=150, y>-400 ⇒ DRIVE
    TEST_ASSERT_EQUAL_INT(SC_FP_DRIVE, o.state);
}

void test_gk_clear_when_ball_very_close(void){
    StrategyCore sc{}; sc_init(sc, ROLE_GOALKEEPER);
    ScOut o = sc_tick(sc, w(true,true,0,100), 0);     // dist=100<=150 ⇒ GK_CLEAR
    TEST_ASSERT_EQUAL_INT(SC_GK_CLEAR, o.state);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_wait_until_match_runs);
    RUN_TEST(test_field_rush_on_start_when_ball_seen);
    RUN_TEST(test_field_seek_on_start_when_no_ball);
    RUN_TEST(test_field_defend_when_ball_behind);
    RUN_TEST(test_gk_wait_then_patrol);
    RUN_TEST(test_gk_intercept_when_ball_seen);
    RUN_TEST(test_field_defend_precedes_drive_when_both_trigger);
    RUN_TEST(test_field_drive_when_close_and_not_behind);
    RUN_TEST(test_gk_clear_when_ball_very_close);
    return UNITY_END();
}
