// test_central_contract — pio test -e test_native -f test_central_contract
#include <unity.h>
#include "types.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

void test_worldsnapshot_has_ball_velocity_fields(void){
    WorldSnapshot w{};
    w.ball_vx_mm_s = -1234;
    w.ball_vy_mm_s = 567;
    TEST_ASSERT_EQUAL_INT16(-1234, w.ball_vx_mm_s);
    TEST_ASSERT_EQUAL_INT16(567, w.ball_vy_mm_s);
}
void test_worldsnapshot_size_is_31(void){
    TEST_ASSERT_EQUAL_UINT32(31, sizeof(WorldSnapshot));
}
void test_worldsnapshot_has_goal_own_fields(void){
    WorldSnapshot w{};
    w.goal_own_visible = 1;
    w.goal_own_angle_centideg = -4321;
    w.goal_own_distance_mm = 1234;
    TEST_ASSERT_EQUAL_UINT8(1, w.goal_own_visible);
    TEST_ASSERT_EQUAL_INT16(-4321, w.goal_own_angle_centideg);
    TEST_ASSERT_EQUAL_INT16(1234, w.goal_own_distance_mm);
}
void test_worldsnapshot_heading_valid_flag_bit4(void){
    WorldSnapshot w{};
    w.flags = 0x10;  // bit 4 = heading_valid
    TEST_ASSERT_TRUE((w.flags & 0x10) != 0);
    TEST_ASSERT_EQUAL_UINT8(0, (w.flags & 0xE0));  // bits 5-7 reservados = 0
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_worldsnapshot_has_ball_velocity_fields);
    RUN_TEST(test_worldsnapshot_size_is_31);
    RUN_TEST(test_worldsnapshot_has_goal_own_fields);
    RUN_TEST(test_worldsnapshot_heading_valid_flag_bit4);
    return UNITY_END();
}
