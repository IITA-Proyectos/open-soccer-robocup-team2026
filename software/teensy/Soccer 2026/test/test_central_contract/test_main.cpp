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
void test_worldsnapshot_size_is_27(void){
    TEST_ASSERT_EQUAL_UINT32(27, sizeof(WorldSnapshot));
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_worldsnapshot_has_ball_velocity_fields);
    RUN_TEST(test_worldsnapshot_size_is_27);
    return UNITY_END();
}
