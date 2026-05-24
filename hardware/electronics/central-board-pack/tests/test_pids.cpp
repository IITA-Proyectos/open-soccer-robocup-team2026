// test_pids — tests unitarios para los PIDs de la placa CENTRAL.
// Corre en host con: pio test -e test_native -f test_pids

#include <unity.h>
#include <cmath>
#include "pids.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// wrap_diff_deg
// ============================================================================

void test_wrap_diff_zero_diff(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, wrap_diff_deg(0.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, wrap_diff_deg(45.0f, 45.0f));
}

void test_wrap_diff_small_positive(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.0f, wrap_diff_deg(45.0f, 15.0f));
}

void test_wrap_diff_small_negative(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -30.0f, wrap_diff_deg(15.0f, 45.0f));
}

void test_wrap_diff_wraps_across_180(void) {
    // setpoint=170, current=-170 → diff "corta" es +20° (no -340°).
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -20.0f, wrap_diff_deg(170.0f, -170.0f));
    // setpoint=-170, current=170 → diff "corta" es -20° (no +340°).
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, wrap_diff_deg(-170.0f, 170.0f));
}

void test_wrap_diff_exact_180(void) {
    // Caso edge: diff = ±180. El algoritmo elige uno consistente.
    float r = wrap_diff_deg(180.0f, 0.0f);
    TEST_ASSERT_TRUE(std::abs(r) <= 180.0f);
}

// ============================================================================
// HeadingPID
// ============================================================================

void test_heading_pid_zero_error_zero_output(void) {
    HeadingPID pid;
    pid.setpoint_deg = 0.0f;
    float out = heading_pid_tick(pid, 0.0f, 0);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, out);
}

void test_heading_pid_positive_error_positive_output(void) {
    HeadingPID pid;
    pid.setpoint_deg = 90.0f;
    float out = heading_pid_tick(pid, 0.0f, 0);
    // Error = +90, Kp=3 → output esperado ≈ 270 grados/s (sin integral/derivada en primer tick).
    TEST_ASSERT_TRUE(out > 100.0f);   // dirección correcta
    TEST_ASSERT_TRUE(out > 200.0f);
}

void test_heading_pid_output_clamped(void) {
    HeadingPID pid;
    pid.output_clamp = 360.0f;
    pid.setpoint_deg = 180.0f;
    float out = heading_pid_tick(pid, -180.0f, 0);
    // Error = ±180 (wrapped), Kp=3 → tendería a 540 deg/s, pero clamp = 360.
    TEST_ASSERT_TRUE(out <= 361.0f);   // tolerancia
    TEST_ASSERT_TRUE(out >= -361.0f);
}

void test_heading_pid_reset_zeros_state(void) {
    HeadingPID pid;
    pid.setpoint_deg = 90.0f;
    // Acumulamos integral con varios ticks.
    heading_pid_tick(pid, 0.0f, 0);
    heading_pid_tick(pid, 0.0f, 10);
    heading_pid_tick(pid, 0.0f, 20);
    TEST_ASSERT_TRUE(pid.integral != 0.0f);
    TEST_ASSERT_TRUE(pid.primed);

    heading_pid_reset(pid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid.integral);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid.prev_error);
    TEST_ASSERT_FALSE(pid.primed);
}

void test_heading_pid_handles_wrap_around(void) {
    HeadingPID pid;
    pid.setpoint_deg = 170.0f;
    // Current = -170 → error "corto" debe ser -20° (no +340°).
    float out = heading_pid_tick(pid, -170.0f, 0);
    // Output negativo (porque error es negativo según wrap_diff_deg(170, -170) = -20).
    TEST_ASSERT_TRUE(out < 0.0f);
    TEST_ASSERT_TRUE(std::abs(out) < 100.0f);  // no es 1000+ por interpretar mal
}

void test_heading_pid_first_tick_no_derivative_kick(void) {
    HeadingPID pid;
    pid.kd = 100.0f;  // ganancia derivativa muy alta
    pid.setpoint_deg = 90.0f;
    // Primer tick: prev_error = 0 default, error = 90. Sin la guarda primed,
    // derivada = (90-0)/dt = enorme. Con primed=false en init, no se usa.
    float out = heading_pid_tick(pid, 0.0f, 0);
    // El output debe estar dominado por kp*error = 3*90 = 270, no por derivativa.
    TEST_ASSERT_TRUE(out < 360.0f);  // si entra derivativa, salta a miles
    TEST_ASSERT_TRUE(out > 200.0f);
}

// ============================================================================
// LateralPID
// ============================================================================

void test_lateral_pid_zero_error_zero_output(void) {
    LateralPID pid;
    pid.setpoint = 1.0f;
    float out = lateral_pid_tick(pid, 1.0f, 0);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, out);
}

void test_lateral_pid_negative_error_negative_output(void) {
    LateralPID pid;
    pid.setpoint = 1.0f;
    // measurement = 3 → error = setpoint - measurement = -2 → output negativo.
    float out = lateral_pid_tick(pid, 3.0f, 0);
    TEST_ASSERT_TRUE(out < 0.0f);
}

void test_lateral_pid_output_clamped(void) {
    LateralPID pid;
    pid.setpoint = 1.0f;
    pid.output_clamp = 500.0f;
    pid.kp = 1000.0f;  // ganancia muy alta para forzar saturación
    float out = lateral_pid_tick(pid, 100.0f, 0);
    TEST_ASSERT_TRUE(out >= -501.0f);
    TEST_ASSERT_TRUE(out <= +501.0f);
}

// ============================================================================
// Approach velocity profile
// ============================================================================

void test_approach_velocity_zero_at_close(void) {
    float v = approach_velocity(30.0f, 50.0f, 500.0f, 600.0f, 200.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, v);
}

void test_approach_velocity_max_at_far(void) {
    float v = approach_velocity(800.0f, 50.0f, 500.0f, 600.0f, 200.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 600.0f, v);
}

void test_approach_velocity_min_at_close_threshold(void) {
    float v = approach_velocity(50.0f, 50.0f, 500.0f, 600.0f, 200.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 200.0f, v);
}

void test_approach_velocity_mid_interpolation(void) {
    // distance = 275 = mitad entre close=50 y far=500 → velocidad mitad entre min y max.
    float v = approach_velocity(275.0f, 50.0f, 500.0f, 600.0f, 200.0f);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 400.0f, v);  // (200+600)/2 = 400
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();

    // wrap_diff_deg
    RUN_TEST(test_wrap_diff_zero_diff);
    RUN_TEST(test_wrap_diff_small_positive);
    RUN_TEST(test_wrap_diff_small_negative);
    RUN_TEST(test_wrap_diff_wraps_across_180);
    RUN_TEST(test_wrap_diff_exact_180);

    // HeadingPID
    RUN_TEST(test_heading_pid_zero_error_zero_output);
    RUN_TEST(test_heading_pid_positive_error_positive_output);
    RUN_TEST(test_heading_pid_output_clamped);
    RUN_TEST(test_heading_pid_reset_zeros_state);
    RUN_TEST(test_heading_pid_handles_wrap_around);
    RUN_TEST(test_heading_pid_first_tick_no_derivative_kick);

    // LateralPID
    RUN_TEST(test_lateral_pid_zero_error_zero_output);
    RUN_TEST(test_lateral_pid_negative_error_negative_output);
    RUN_TEST(test_lateral_pid_output_clamped);

    // Approach
    RUN_TEST(test_approach_velocity_zero_at_close);
    RUN_TEST(test_approach_velocity_max_at_far);
    RUN_TEST(test_approach_velocity_min_at_close_threshold);
    RUN_TEST(test_approach_velocity_mid_interpolation);

    return UNITY_END();
}
