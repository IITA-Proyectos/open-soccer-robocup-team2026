// test_tof_distance_hold — tests unitarios para el módulo tof_distance_hold.
// Corre en host con: pio test -e test_native -f test_tof_distance_hold
//                o:  bash scripts/run-host-tests.sh test_tof_distance_hold
//
// Contrato (definido por estos tests, TDD-first):
//   Controlador P para ACERCARSE/MANTENER una distancia objetivo a una pared
//   usando UNA lectura de ToF (mm). Backup para moverse sin cámaras ni sensores
//   de luz. Módulo PURO host-testeable (solo <stdint.h>).
//
//   Convención de signo: v_mm_s > 0 = avanzar HACIA la pared (reduce distancia).
//     error = cur_dist - target_dist
//       • cur > target (estoy lejos)  -> error > 0 -> v > 0 (avanzo, me acerco).
//       • cur < target (estoy cerca)  -> error < 0 -> v < 0 (retrocedo).
//
//   Lógica:
//     • !dist_valid && stop_if_invalid -> v=0, arrived=false, valid=false (seguro).
//     • |error| <= deadband           -> arrived=true,  v=0,  valid=true.
//     • si no                          -> v = clamp(-max, max, kp*error),
//                                          arrived=false, valid=true.

#include <unity.h>
#include "tof_distance_hold.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Config de referencia para los tests (ganancias != 0 para ver el signo).
static TofHoldParams make_params() {
    TofHoldParams p;
    p.kp = 2.0f;
    p.max_speed_mm_s = 400.0f;
    p.deadband_mm = 15.0f;
    p.stop_if_invalid = true;
    return p;
}

// ============================================================================
// (a) cur > target -> avanzar HACIA la pared (v > 0), sin arrived
// ============================================================================

void test_far_from_wall_advances(void) {
    TofHoldParams p = make_params();
    // cur=500, target=200 -> error=+300, pero kp*300=600 > max=400 -> clamp a 400.
    TofHoldCmd cmd = tof_hold_update(500.0f, 200.0f, true, p);
    TEST_ASSERT_TRUE(cmd.valid);
    TEST_ASSERT_FALSE(cmd.arrived);
    TEST_ASSERT_TRUE(cmd.v_mm_s > 0.0f);   // avanza hacia la pared
}

// ============================================================================
// (b) cur < target -> retroceder (v < 0)
// ============================================================================

void test_close_to_wall_retreats(void) {
    TofHoldParams p = make_params();
    // cur=150, target=200 -> error=-50 -> v = kp*-50 = -100.
    TofHoldCmd cmd = tof_hold_update(150.0f, 200.0f, true, p);
    TEST_ASSERT_TRUE(cmd.valid);
    TEST_ASSERT_FALSE(cmd.arrived);
    TEST_ASSERT_TRUE(cmd.v_mm_s < 0.0f);   // retrocede
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -100.0f, cmd.v_mm_s);
}

// ============================================================================
// (c) |error| <= deadband -> arrived, v=0
// ============================================================================

void test_within_deadband_arrived_zero_speed(void) {
    TofHoldParams p = make_params();   // deadband=15
    // cur=210, target=200 -> error=+10 (<=15) -> arrived, v=0.
    TofHoldCmd cmd = tof_hold_update(210.0f, 200.0f, true, p);
    TEST_ASSERT_TRUE(cmd.valid);
    TEST_ASSERT_TRUE(cmd.arrived);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmd.v_mm_s);
}

void test_deadband_boundary_is_inclusive(void) {
    TofHoldParams p = make_params();   // deadband=15
    // error == -15 (límite exacto) -> sigue contando como arrived (<=).
    TofHoldCmd cmd = tof_hold_update(185.0f, 200.0f, true, p);
    TEST_ASSERT_TRUE(cmd.valid);
    TEST_ASSERT_TRUE(cmd.arrived);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmd.v_mm_s);
}

// ============================================================================
// (d) error grande -> v clampeado a max (ambos signos)
// ============================================================================

void test_large_positive_error_clamped_to_max(void) {
    TofHoldParams p = make_params();   // kp=2, max=400
    // cur=1000, target=200 -> error=+800 -> kp*800=1600 -> clamp a +400.
    TofHoldCmd cmd = tof_hold_update(1000.0f, 200.0f, true, p);
    TEST_ASSERT_TRUE(cmd.valid);
    TEST_ASSERT_FALSE(cmd.arrived);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 400.0f, cmd.v_mm_s);
}

void test_large_negative_error_clamped_to_neg_max(void) {
    TofHoldParams p = make_params();   // kp=2, max=400
    // cur=50, target=900 -> error=-850 -> kp*-850=-1700 -> clamp a -400.
    TofHoldCmd cmd = tof_hold_update(50.0f, 900.0f, true, p);
    TEST_ASSERT_TRUE(cmd.valid);
    TEST_ASSERT_FALSE(cmd.arrived);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -400.0f, cmd.v_mm_s);
}

// ============================================================================
// (e) dist_valid=false -> v=0, valid=false (con stop_if_invalid)
// ============================================================================

void test_invalid_reading_stops_safe(void) {
    TofHoldParams p = make_params();   // stop_if_invalid=true
    TofHoldCmd cmd = tof_hold_update(500.0f, 200.0f, false, p);
    TEST_ASSERT_FALSE(cmd.valid);
    TEST_ASSERT_FALSE(cmd.arrived);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmd.v_mm_s);
}

// ============================================================================
// (f) default params sanos: kp>0, max>0, deadband>0
// ============================================================================

void test_default_params_are_sane(void) {
    TofHoldParams p = tof_hold_default_params();
    TEST_ASSERT_TRUE(p.kp > 0.0f);
    TEST_ASSERT_TRUE(p.max_speed_mm_s > 0.0f);
    TEST_ASSERT_TRUE(p.deadband_mm > 0.0f);
    TEST_ASSERT_TRUE(p.stop_if_invalid);   // backup seguro por defecto
}

// Sanidad extra: con defaults, una lectura clara lejos de la pared manda avanzar.
void test_default_params_produce_motion_toward_wall(void) {
    TofHoldParams p = tof_hold_default_params();
    TofHoldCmd cmd = tof_hold_update(600.0f, 250.0f, true, p);
    TEST_ASSERT_TRUE(cmd.valid);
    TEST_ASSERT_FALSE(cmd.arrived);
    TEST_ASSERT_TRUE(cmd.v_mm_s > 0.0f);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_far_from_wall_advances);
    RUN_TEST(test_close_to_wall_retreats);

    RUN_TEST(test_within_deadband_arrived_zero_speed);
    RUN_TEST(test_deadband_boundary_is_inclusive);

    RUN_TEST(test_large_positive_error_clamped_to_max);
    RUN_TEST(test_large_negative_error_clamped_to_neg_max);

    RUN_TEST(test_invalid_reading_stops_safe);

    RUN_TEST(test_default_params_are_sane);
    RUN_TEST(test_default_params_produce_motion_toward_wall);

    return UNITY_END();
}
