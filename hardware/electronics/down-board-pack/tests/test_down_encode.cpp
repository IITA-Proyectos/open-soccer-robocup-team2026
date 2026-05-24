// test_down_encode — corre con: pio test -e test_native -f test_down_encode
#include <unity.h>
#include <cstring>
#include "types.h"
#include "down_encode.h"
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
// Ejemplo B del contrato (docs/firmware/CONTRATO-DATOS-DOWN.md §3.6), SEQ=0x01.
void test_encode_matches_contract_example_B(void){
    LineStatusV2 s{};
    s.schema_version=2; s.data_valid=1;
    s.line_angle_centideg=4500; s.escape_angle_centideg=-13500;
    s.penetration_mm=15; s.cross_track_mm=-8;
    s.line_present=1; s.sensors_on_line=4; s.event_flags=0;
    s.quality=88; s.sample_age_ms=1; s.reserved=0;
    uint8_t out[64];
    size_t n = down_encode_line(s, 0x01, out, sizeof(out));
    const uint8_t exp[] = {
      0xAA,0x10,0x10,0x01, 0x02,0x01,0x94,0x11,0x44,0xCB,0x0F,0x00,
      0xF8,0xFF,0x01,0x04,0x00,0x58,0x01,0x00, 0xDF,0xBF,0x55 };
    TEST_ASSERT_EQUAL_UINT32(sizeof(exp), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(exp, out, sizeof(exp));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_linestatusv2_is_16_bytes);
    RUN_TEST(test_linestatusv2_constants);
    RUN_TEST(test_encode_matches_contract_example_B);
    return UNITY_END();
}
