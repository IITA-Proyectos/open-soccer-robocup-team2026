// test_down_calib — pio test -e test_native -f test_down_calib
#include <unity.h>
#include "line_calib.h"
using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

void test_static_threshold_is_midpoint(void){
    SensorCalib c{}; lc_set_static(c, 200, 800);
    TEST_ASSERT_EQUAL_UINT16(500, c.threshold);
}

void test_adapt_carpet_moves_toward_reading_when_off_line(void){
    SensorCalib c{}; lc_set_static(c, 200, 800);
    for(int i=0;i<200;++i) lc_adapt_carpet(c, 260, /*on_line=*/false, 0.05f);
    TEST_ASSERT_UINT16_WITHIN(15, 260, c.carpet);
    TEST_ASSERT_UINT16_WITHIN(15, (260+800)/2, c.threshold);
}

void test_adapt_does_not_drift_when_on_line(void){
    SensorCalib c{}; lc_set_static(c, 200, 800);
    for(int i=0;i<200;++i) lc_adapt_carpet(c, 790, /*on_line=*/true, 0.05f);
    TEST_ASSERT_EQUAL_UINT16(200, c.carpet);
}

void test_suspect_when_margin_too_small(void){
    SensorCalib cs[2]; lc_set_static(cs[0],400,900); lc_set_static(cs[1],500,560);
    TEST_ASSERT_TRUE(lc_is_suspect(cs,2,/*min_margin=*/100));
    SensorCalib ok[1]; lc_set_static(ok[0],200,800);
    TEST_ASSERT_FALSE(lc_is_suspect(ok,1,100));
}

void test_adapt_alpha_gt1_clamps_to_1(void){
    SensorCalib c{}; lc_set_static(c, 200, 800);
    lc_adapt_carpet(c, 260, false, 2.0f);  // alpha clamped a 1 → un paso a 260
    TEST_ASSERT_EQUAL_UINT16(260, c.carpet);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_static_threshold_is_midpoint);
    RUN_TEST(test_adapt_carpet_moves_toward_reading_when_off_line);
    RUN_TEST(test_adapt_does_not_drift_when_on_line);
    RUN_TEST(test_suspect_when_margin_too_small);
    RUN_TEST(test_adapt_alpha_gt1_clamps_to_1);
    return UNITY_END();
}
