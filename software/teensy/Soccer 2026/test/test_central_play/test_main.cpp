// pio test -e test_native -f test_central_play
#include <unity.h>
#include "play_decision.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

void test_let_circulate_when_to_opp_goal(void){
    PlayIn in{}; in.in_reach=true; in.moving=true; in.kind=BT_TO_OPP_GOAL;
    TEST_ASSERT_EQUAL_INT(PLAY_LET_CIRCULATE, pd_decide(in).action);
}
void test_intercept_when_to_own_goal(void){
    PlayIn in{}; in.in_reach=true; in.moving=true; in.kind=BT_TO_OWN_GOAL;
    TEST_ASSERT_EQUAL_INT(PLAY_INTERCEPT, pd_decide(in).action);
}
void test_deflect_when_other(void){
    PlayIn in{}; in.in_reach=true; in.moving=true; in.kind=BT_OTHER;
    TEST_ASSERT_EQUAL_INT(PLAY_DEFLECT_TO_OPP, pd_decide(in).action);
}
void test_none_when_not_in_reach(void){
    PlayIn in{}; in.in_reach=false; in.moving=true; in.kind=BT_OTHER;
    TEST_ASSERT_EQUAL_INT(PLAY_NONE, pd_decide(in).action);
}
void test_none_when_still(void){
    PlayIn in{}; in.in_reach=true; in.moving=false; in.kind=BT_STILL;
    TEST_ASSERT_EQUAL_INT(PLAY_NONE, pd_decide(in).action);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_let_circulate_when_to_opp_goal);
    RUN_TEST(test_intercept_when_to_own_goal);
    RUN_TEST(test_deflect_when_other);
    RUN_TEST(test_none_when_not_in_reach);
    RUN_TEST(test_none_when_still);
    return UNITY_END();
}
