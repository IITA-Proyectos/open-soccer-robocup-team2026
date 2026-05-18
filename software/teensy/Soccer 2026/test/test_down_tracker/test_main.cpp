// test_down_tracker — pio test -e test_native -f test_down_tracker
#include <unity.h>
#include "line_tracker.h"
using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

void test_line_end_fires_after_sustained_then_lost(void){
    LineTracker t{};
    TEST_ASSERT_FALSE(lt_update(t,true,0,200));
    TEST_ASSERT_FALSE(lt_update(t,true,150,200));
    TEST_ASSERT_FALSE(lt_update(t,true,250,200));
    TEST_ASSERT_TRUE (lt_update(t,false,260,200));
    TEST_ASSERT_FALSE(lt_update(t,false,300,200));
}

void test_no_line_end_if_blip_too_short(void){
    LineTracker t{};
    TEST_ASSERT_FALSE(lt_update(t,true,0,200));
    TEST_ASSERT_FALSE(lt_update(t,false,50,200));
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_line_end_fires_after_sustained_then_lost);
    RUN_TEST(test_no_line_end_if_blip_too_short);
    return UNITY_END();
}
