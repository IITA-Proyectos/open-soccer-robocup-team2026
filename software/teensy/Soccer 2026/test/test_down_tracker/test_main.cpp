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

void test_line_end_rearms_after_second_sustained(void){
    LineTracker t{};
    lt_update(t,true,0,200);
    lt_update(t,true,250,200);
    lt_update(t,false,260,200);   // primer LINE_END — consumido
    TEST_ASSERT_FALSE(lt_update(t,true,300,200));
    TEST_ASSERT_FALSE(lt_update(t,true,510,200));  // 210ms ≥ 200 → had_sustained
    TEST_ASSERT_TRUE (lt_update(t,false,520,200)); // segundo LINE_END debe disparar
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_line_end_fires_after_sustained_then_lost);
    RUN_TEST(test_no_line_end_if_blip_too_short);
    RUN_TEST(test_line_end_rearms_after_second_sustained);
    return UNITY_END();
}
