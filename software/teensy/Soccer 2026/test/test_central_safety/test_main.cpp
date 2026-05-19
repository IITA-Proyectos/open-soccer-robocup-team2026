// pio test -e test_native -f test_central_safety
#include <unity.h>
#include "field_safety.h"
#include "types.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

static LineStatusV2 mk(uint8_t valid, uint8_t present, int16_t esc, uint8_t ev){
    LineStatusV2 s{}; s.schema_version=LSV2_SCHEMA; s.data_valid=valid;
    s.line_present=present; s.escape_angle_centideg=esc; s.event_flags=ev;
    s.line_angle_centideg = present? 0 : LSV2_NA_I16;
    return s;
}
void test_escape_on_imminent_exit(void){
    LineStatusV2 s = mk(1,1, 9000, EV_IMMINENT_EXIT);
    FieldSafety fs = fs_eval(s);
    TEST_ASSERT_TRUE(fs.preempt);
    TEST_ASSERT_EQUAL_INT16(9000, fs.escape_angle_centideg);
}
void test_conservative_when_invalid(void){
    LineStatusV2 s = mk(0,0, LSV2_NA_I16, EV_LIFTED);
    FieldSafety fs = fs_eval(s);
    TEST_ASSERT_FALSE(fs.preempt);
    TEST_ASSERT_TRUE(fs.conservative);
}
void test_no_action_when_clear(void){
    LineStatusV2 s = mk(1,0, LSV2_NA_I16, 0);
    FieldSafety fs = fs_eval(s);
    TEST_ASSERT_FALSE(fs.preempt);
    TEST_ASSERT_FALSE(fs.conservative);
}
void test_escape_when_line_present_no_flag(void){
    LineStatusV2 s = mk(1,1, -4500, 0);
    FieldSafety fs = fs_eval(s);
    TEST_ASSERT_TRUE(fs.preempt);
    TEST_ASSERT_EQUAL_INT16(-4500, fs.escape_angle_centideg);
}

void test_imminent_but_no_valid_escape(void){
    // EV_IMMINENT_EXIT dispara antes de resolver geometria: escape aun NA.
    LineStatusV2 s = mk(1, 0, LSV2_NA_I16, EV_IMMINENT_EXIT);
    FieldSafety fs = fs_eval(s);
    TEST_ASSERT_FALSE(fs.preempt);                          // sin heading seguro => no preempt
    TEST_ASSERT_EQUAL_INT16(LSV2_NA_I16, fs.escape_angle_centideg);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_escape_on_imminent_exit);
    RUN_TEST(test_conservative_when_invalid);
    RUN_TEST(test_no_action_when_clear);
    RUN_TEST(test_escape_when_line_present_no_flag);
    RUN_TEST(test_imminent_but_no_valid_escape);
    return UNITY_END();
}
