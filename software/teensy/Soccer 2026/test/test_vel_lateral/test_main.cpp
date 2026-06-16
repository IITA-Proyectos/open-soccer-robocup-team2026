// test_vel_lateral — des-rotación de la velocidad para el gate anti-strafe (TASK-213).
// Corre host: bash scripts/run-host-tests.sh test_vel_lateral

#include <unity.h>
#include "vel_lateral.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// heading=0 → IDENTIDAD: lateral==vy, forward==vx.
void test_heading_cero_identidad(void) {
    VelLateral v = vel_lateral_from_otos(/*vx*/120, /*vy*/-45, /*heading*/0);
    TEST_ASSERT_INT16_WITHIN(1, -45, v.lateral_mm_s);
    TEST_ASSERT_INT16_WITHIN(1, 120, v.forward_mm_s);
}

// heading=+90° (9000 cdeg), (vx=100,vy=0) → lateral=-100, forward=0.
void test_heading_90_vx(void) {
    VelLateral v = vel_lateral_from_otos(100, 0, 9000);
    TEST_ASSERT_INT16_WITHIN(1, -100, v.lateral_mm_s);
    TEST_ASSERT_INT16_WITHIN(1, 0, v.forward_mm_s);
}

// heading=+90°, (vx=0,vy=100) → lateral=0, forward=100.
void test_heading_90_vy(void) {
    VelLateral v = vel_lateral_from_otos(0, 100, 9000);
    TEST_ASSERT_INT16_WITHIN(1, 0, v.lateral_mm_s);
    TEST_ASSERT_INT16_WITHIN(1, 100, v.forward_mm_s);
}

// heading=-90°: simetría — (vx=100,vy=0) → lateral=+100 (opuesto a +90).
void test_heading_neg90_simetria(void) {
    VelLateral v = vel_lateral_from_otos(100, 0, -9000);
    TEST_ASSERT_INT16_WITHIN(1, 100, v.lateral_mm_s);
    TEST_ASSERT_INT16_WITHIN(1, 0, v.forward_mm_s);
}

// is_strafing: corte estricto >.
void test_is_strafing(void) {
    TEST_ASSERT_TRUE(vel_lateral_is_strafing(200, 50));
    TEST_ASSERT_TRUE(vel_lateral_is_strafing(-200, 50));   // magnitud
    TEST_ASSERT_FALSE(vel_lateral_is_strafing(10, 50));
    TEST_ASSERT_FALSE(vel_lateral_is_strafing(50, 50));    // borde == NO es strafe (estricto)
}

// Saturación: velocidades y heading que exceden int16 → satura sin UB.
void test_saturacion_int16(void) {
    VelLateral v = vel_lateral_from_otos(30000, 30000, 4500);   // 45°: forward ~42420 → satura
    TEST_ASSERT_TRUE(v.forward_mm_s <= 32767 && v.forward_mm_s >= -32768);
    TEST_ASSERT_TRUE(v.lateral_mm_s <= 32767 && v.lateral_mm_s >= -32768);
    TEST_ASSERT_EQUAL_INT16(32767, v.forward_mm_s);   // satura al máximo
    TEST_ASSERT_INT16_WITHIN(2, 0, v.lateral_mm_s);   // a 45° con vx=vy las laterales se cancelan
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_heading_cero_identidad);
    RUN_TEST(test_heading_90_vx);
    RUN_TEST(test_heading_90_vy);
    RUN_TEST(test_heading_neg90_simetria);
    RUN_TEST(test_is_strafing);
    RUN_TEST(test_saturacion_int16);
    return UNITY_END();
}
