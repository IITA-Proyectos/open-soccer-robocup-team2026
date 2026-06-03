// test_otos_fusion — fusión PURA dual-OTOS (audit 2026-06-03 #7 heading, #20 slip).
// Corre host con: bash scripts/run-host-tests.sh test_otos_fusion
//
// #7  fuse_dual_heading_deg: promedio vectorial de los headings de IMU.
//     El cálculo viejo atan2(dy_acumulado, separación) saturaba en ±90° y un
//     promedio aritmético crudo rompería el wraparound (170 + -170 → 0, mal).
// #20 otos_slip_estimate: sobre VELOCIDADES, restando |ω|·separación, clamp a 0.
//     El viejo |x_der − x_izq| crecía monótono por drift sin que hubiera slip.

#include <unity.h>
#include <cmath>
#include "otos_fusion.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// #7 — fuse_dual_heading_deg
// ---------------------------------------------------------------------------

void test_heading_both_zero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, fuse_dual_heading_deg(0.0f, 0.0f));
}

void test_heading_both_equal_nonzero(void) {
    // Ambas IMUs en 120° → 120°. El cálculo viejo (atan2 acotado a ±90°) NO
    // podía representar esto; este test es la regresión que demuestra el fix.
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 120.0f, fuse_dual_heading_deg(120.0f, 120.0f));
}

void test_heading_average_of_two(void) {
    // 10° y 30° → 20° (promedio vectorial == aritmético lejos del wrap).
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 20.0f, fuse_dual_heading_deg(10.0f, 30.0f));
}

void test_heading_symmetric_about_zero(void) {
    // +10 y -10 → 0.
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, fuse_dual_heading_deg(10.0f, -10.0f));
}

void test_heading_wraparound_near_180(void) {
    // 170 y -170: el promedio físico correcto es ±180, NO 0 (que daría el crudo).
    float h = fuse_dual_heading_deg(170.0f, -170.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 180.0f, std::fabs(h));
}

void test_heading_wraparound_both_near_180(void) {
    // 175 y 185 (=-175 normalizado): promedio = 180.
    float h = fuse_dual_heading_deg(175.0f, -175.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 180.0f, std::fabs(h));
}

void test_heading_ninety(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 90.0f, fuse_dual_heading_deg(90.0f, 90.0f));
}

void test_heading_negative_quadrant(void) {
    // -120 y -120 → -120 (otro caso fuera del rango ±90 del cálculo viejo).
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -120.0f, fuse_dual_heading_deg(-120.0f, -120.0f));
}

void test_heading_output_in_range(void) {
    // El resultado siempre está en [-180, 180].
    for (float a = -180.0f; a <= 180.0f; a += 37.0f) {
        for (float b = -180.0f; b <= 180.0f; b += 53.0f) {
            float h = fuse_dual_heading_deg(a, b);
            TEST_ASSERT_TRUE(h >= -180.05f && h <= 180.05f);
        }
    }
}

// ---------------------------------------------------------------------------
// #20 — otos_slip_estimate
// ---------------------------------------------------------------------------

void test_slip_pure_translation_is_zero(void) {
    // Ambos OTOS con la misma vx, sin rotación → slip 0.
    float s = otos_slip_estimate(/*vL*/300.0f, /*vR*/300.0f, /*omega*/0.0f, /*half_sep*/100.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, s);
}

void test_slip_pure_rotation_cancels(void) {
    // Rotación pura: la diferencia esperada de vx = ω·sep. Con sep=200 (half=100),
    // ω=2 rad/s → esperado = 2*200 = 400. Si la diferencia observada es exactamente
    // 400, el exceso (slip real) es 0.
    float s = otos_slip_estimate(/*vL*/-200.0f, /*vR*/200.0f, /*omega*/2.0f, /*half_sep*/100.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, s);
}

void test_slip_real_patinazo_is_excess(void) {
    // Diferencia observada MAYOR a la esperada por rotación → slip = exceso.
    // observado = |500 - (-100)| = 600 ; esperado = |1.0|*200 = 200 ; slip = 400.
    float s = otos_slip_estimate(/*vL*/-100.0f, /*vR*/500.0f, /*omega*/1.0f, /*half_sep*/100.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 400.0f, s);
}

void test_slip_never_negative(void) {
    // Esperado por rotación MAYOR que el observado (ruido) → clamp a 0, no negativo.
    float s = otos_slip_estimate(/*vL*/0.0f, /*vR*/50.0f, /*omega*/5.0f, /*half_sep*/100.0f);
    TEST_ASSERT_TRUE(s >= 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, s);
}

void test_slip_static_no_drift(void) {
    // Con velocidades 0 sostenidas, slip == 0 (prueba que YA NO usa posiciones
    // acumuladas que crecían por drift — el bug del audit #20).
    float s = otos_slip_estimate(0.0f, 0.0f, 0.0f, 100.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, s);
}

int main(int, char**) {
    UNITY_BEGIN();
    // heading (#7)
    RUN_TEST(test_heading_both_zero);
    RUN_TEST(test_heading_both_equal_nonzero);
    RUN_TEST(test_heading_average_of_two);
    RUN_TEST(test_heading_symmetric_about_zero);
    RUN_TEST(test_heading_wraparound_near_180);
    RUN_TEST(test_heading_wraparound_both_near_180);
    RUN_TEST(test_heading_ninety);
    RUN_TEST(test_heading_negative_quadrant);
    RUN_TEST(test_heading_output_in_range);
    // slip (#20)
    RUN_TEST(test_slip_pure_translation_is_zero);
    RUN_TEST(test_slip_pure_rotation_cancels);
    RUN_TEST(test_slip_real_patinazo_is_excess);
    RUN_TEST(test_slip_never_negative);
    RUN_TEST(test_slip_static_no_drift);
    return UNITY_END();
}
