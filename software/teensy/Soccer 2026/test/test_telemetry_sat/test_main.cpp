// test_telemetry_sat — saturación float→int16 de telemetría (audit 2026-06-03 #14).
// Corre host con: bash scripts/run-host-tests.sh test_telemetry_sat
//
// El bug: comm_top.cpp casteaba omega (rad/s → centideg/s) a int16 SIN clamp.
// Factor 18000/π ≈ 5729.58 → umbral de overflow ~5.72 rad/s. Un omni rota a
// 8-15 rad/s al alinear; el cast crudo wrappeaba de signo (gira a la derecha →
// reporta izquierda). El test clave (+6 rad/s) demuestra que ahora SATURA a
// +32767 (positivo), no wrappea a negativo.

#include <unity.h>
#include <cmath>
#include "telemetry_sat.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// --- sat_i16 ---------------------------------------------------------------

void test_sat_zero(void) {
    TEST_ASSERT_EQUAL_INT16(0, sat_i16(0.0f));
}

void test_sat_in_range_rounds(void) {
    TEST_ASSERT_EQUAL_INT16(100, sat_i16(100.4f));
    TEST_ASSERT_EQUAL_INT16(101, sat_i16(100.6f));
    TEST_ASSERT_EQUAL_INT16(-100, sat_i16(-100.4f));
    TEST_ASSERT_EQUAL_INT16(-101, sat_i16(-100.6f));
}

void test_sat_clamps_positive(void) {
    TEST_ASSERT_EQUAL_INT16(32767, sat_i16(32767.0f));
    TEST_ASSERT_EQUAL_INT16(32767, sat_i16(50000.0f));
    TEST_ASSERT_EQUAL_INT16(32767, sat_i16(1.0e9f));
}

void test_sat_clamps_negative(void) {
    TEST_ASSERT_EQUAL_INT16(-32768, sat_i16(-32768.0f));
    TEST_ASSERT_EQUAL_INT16(-32768, sat_i16(-50000.0f));
    TEST_ASSERT_EQUAL_INT16(-32768, sat_i16(-1.0e9f));
}

void test_sat_nan_inf_safe(void) {
    TEST_ASSERT_EQUAL_INT16(0, sat_i16(std::nanf("")));
    TEST_ASSERT_EQUAL_INT16(32767, sat_i16(INFINITY));
    TEST_ASSERT_EQUAL_INT16(-32768, sat_i16(-INFINITY));
}

// --- sat_u8 (DC-2) ---------------------------------------------------------

void test_sat_u8_zero_and_range(void) {
    TEST_ASSERT_EQUAL_UINT8(0, sat_u8(0.0f));
    TEST_ASSERT_EQUAL_UINT8(100, sat_u8(100.4f));
    TEST_ASSERT_EQUAL_UINT8(101, sat_u8(100.6f));
}

void test_sat_u8_clamps_and_negative(void) {
    TEST_ASSERT_EQUAL_UINT8(0, sat_u8(-1.0f));        // negativo -> 0 (NO wrap a 255)
    TEST_ASSERT_EQUAL_UINT8(0, sat_u8(-1000.0f));
    TEST_ASSERT_EQUAL_UINT8(255, sat_u8(255.0f));
    TEST_ASSERT_EQUAL_UINT8(255, sat_u8(50000.0f));
}

void test_sat_u8_nan_inf_safe(void) {
    TEST_ASSERT_EQUAL_UINT8(0, sat_u8(std::nanf("")));
    TEST_ASSERT_EQUAL_UINT8(255, sat_u8(INFINITY));
    TEST_ASSERT_EQUAL_UINT8(0, sat_u8(-INFINITY));
}

// --- omega_rad_s_to_centideg_s_sat -----------------------------------------

void test_omega_zero(void) {
    TEST_ASSERT_EQUAL_INT16(0, omega_rad_s_to_centideg_s_sat(0.0f));
}

void test_omega_one_rad_s(void) {
    // 1 rad/s = 18000/π ≈ 5729.58 → 5730 con redondeo.
    TEST_ASSERT_INT16_WITHIN(1, 5730, omega_rad_s_to_centideg_s_sat(1.0f));
    TEST_ASSERT_INT16_WITHIN(1, -5730, omega_rad_s_to_centideg_s_sat(-1.0f));
}

void test_omega_in_range_no_saturation(void) {
    // 5.0 rad/s = ~28648, dentro de int16 → sin saturar.
    int16_t v = omega_rad_s_to_centideg_s_sat(5.0f);
    TEST_ASSERT_INT16_WITHIN(2, 28648, v);
    TEST_ASSERT_TRUE(v > 0);
}

// EL TEST CLAVE del audit #14: +6 rad/s desborda el int16 crudo y wrappearía a
// NEGATIVO; con clamp debe quedar en +32767 (positivo).
void test_omega_overflow_saturates_positive_not_wrapped(void) {
    int16_t v = omega_rad_s_to_centideg_s_sat(6.0f);
    TEST_ASSERT_EQUAL_INT16(32767, v);
    TEST_ASSERT_TRUE(v > 0);   // NO se invierte el signo
}

void test_omega_overflow_saturates_negative(void) {
    int16_t v = omega_rad_s_to_centideg_s_sat(-6.0f);
    TEST_ASSERT_EQUAL_INT16(-32768, v);
    TEST_ASSERT_TRUE(v < 0);
}

void test_omega_extreme_spin(void) {
    // Un omni a 100 rad/s (irreal pero defensivo) satura, no wrappea.
    TEST_ASSERT_EQUAL_INT16(32767, omega_rad_s_to_centideg_s_sat(100.0f));
    TEST_ASSERT_EQUAL_INT16(-32768, omega_rad_s_to_centideg_s_sat(-100.0f));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sat_zero);
    RUN_TEST(test_sat_in_range_rounds);
    RUN_TEST(test_sat_clamps_positive);
    RUN_TEST(test_sat_clamps_negative);
    RUN_TEST(test_sat_nan_inf_safe);
    RUN_TEST(test_sat_u8_zero_and_range);
    RUN_TEST(test_sat_u8_clamps_and_negative);
    RUN_TEST(test_sat_u8_nan_inf_safe);
    RUN_TEST(test_omega_zero);
    RUN_TEST(test_omega_one_rad_s);
    RUN_TEST(test_omega_in_range_no_saturation);
    RUN_TEST(test_omega_overflow_saturates_positive_not_wrapped);
    RUN_TEST(test_omega_overflow_saturates_negative);
    RUN_TEST(test_omega_extreme_spin);
    return UNITY_END();
}
