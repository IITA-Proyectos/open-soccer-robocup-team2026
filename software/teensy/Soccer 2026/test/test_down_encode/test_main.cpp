// test_down_encode — corre con: pio test -e test_native -f test_down_encode
#include <unity.h>
#include <cstring>
#include "types.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

void test_linestatusv2_is_16_bytes(void){
    TEST_ASSERT_EQUAL_UINT32(16, sizeof(LineStatusV2));
}
void test_linestatusv2_constants(void){
    TEST_ASSERT_EQUAL_UINT8(2, LSV2_SCHEMA);
    TEST_ASSERT_EQUAL_INT16(-32768, LSV2_NA_I16);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, LSV2_NA_U16);
    TEST_ASSERT_EQUAL_UINT8(0x01, EV_IMMINENT_EXIT);
    TEST_ASSERT_EQUAL_UINT8(0x02, EV_CORNER);
    TEST_ASSERT_EQUAL_UINT8(0x04, EV_LINE_END);
    TEST_ASSERT_EQUAL_UINT8(0x08, EV_LIFTED);
    TEST_ASSERT_EQUAL_UINT8(0x10, EV_CALIB_SUSPECT);
    TEST_ASSERT_EQUAL_UINT8(0x20, EV_MUX_DEAD);
    TEST_ASSERT_EQUAL_UINT8(0x40, EV_DEGRADED_GEOMETRY);
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_linestatusv2_is_16_bytes);
    RUN_TEST(test_linestatusv2_constants);
    return UNITY_END();
}
