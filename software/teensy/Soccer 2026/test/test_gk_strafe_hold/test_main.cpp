// test_gk_strafe_hold — PI de rumbo del arquero → trim de la rueda trasera (puro).
// Corre con: bash scripts/run-host-tests.sh test_gk_strafe_hold

#include <unity.h>
#include "gk_strafe_hold.h"

using namespace iitasoccer;

// Config base de prueba (valores de arranque; el banco los tunea).
static GkStrafeHoldCfg cfg() {
    GkStrafeHoldCfg c;
    c.kp = 2.0f;          // 2 PWM/grado
    c.ki = 1.0f;          // 1 PWM/(grado·s)
    c.deadband_deg = 2.0f;
    c.band_deg = 20.0f;
    c.trim_max_pwm = 30;  // pequeña modulación
    c.i_max_pwm = 15.0f;
    return c;
}

void setUp(void) {}
void tearDown(void) {}

void test_deadband_no_trim(void) {
    GkStrafeHoldState st{0.0f};
    GkStrafeHoldOut o = gk_strafe_hold_step(st, cfg(), 1.0f, 0.01f);  // |1| < deadband 2
    TEST_ASSERT_EQUAL_INT(0, o.trim_pwm);
    TEST_ASSERT_FALSE(o.out_of_band);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, st.integ);  // no integra en zona muerta
}

void test_proportional_sign_and_value(void) {
    GkStrafeHoldState st{0.0f};
    // error +5°, kp=2 → P=10; primer tick I≈5*1*0.01=0.05 → ~10
    GkStrafeHoldOut o = gk_strafe_hold_step(st, cfg(), 5.0f, 0.01f);
    TEST_ASSERT_INT_WITHIN(1, 10, o.trim_pwm);
    TEST_ASSERT_FALSE(o.out_of_band);
    // error negativo → trim negativo (signo del error)
    GkStrafeHoldState st2{0.0f};
    GkStrafeHoldOut o2 = gk_strafe_hold_step(st2, cfg(), -5.0f, 0.01f);
    TEST_ASSERT_TRUE(o2.trim_pwm < 0);
}

void test_trim_clamped_small(void) {
    GkStrafeHoldState st{0.0f};
    // error 18° (dentro de banda 20), kp=2 → P=36 > trim_max 30 → clamp 30
    GkStrafeHoldOut o = gk_strafe_hold_step(st, cfg(), 18.0f, 0.01f);
    TEST_ASSERT_EQUAL_INT(30, o.trim_pwm);
    TEST_ASSERT_FALSE(o.out_of_band);
}

void test_out_of_band_flag(void) {
    GkStrafeHoldState st{0.0f};
    GkStrafeHoldOut o = gk_strafe_hold_step(st, cfg(), 25.0f, 0.01f);  // > band 20
    TEST_ASSERT_TRUE(o.out_of_band);   // el caller dispara el impulso
}

void test_integral_cancels_sustained_drift(void) {
    // Error sostenido chico (5°): el término I acumula y sube el trim con el tiempo.
    GkStrafeHoldState st{0.0f};
    GkStrafeHoldCfg c = cfg();
    int first = gk_strafe_hold_step(st, c, 5.0f, 0.1f).trim_pwm;
    for (int i = 0; i < 50; ++i) gk_strafe_hold_step(st, c, 5.0f, 0.1f);
    int later = gk_strafe_hold_step(st, c, 5.0f, 0.1f).trim_pwm;
    TEST_ASSERT_TRUE(later >= first);          // I aporta más con el tiempo
    TEST_ASSERT_TRUE(st.integ <= c.i_max_pwm); // anti-windup: I acotado
}

void test_antiwindup_clamps_integral(void) {
    GkStrafeHoldState st{0.0f};
    GkStrafeHoldCfg c = cfg();
    for (int i = 0; i < 1000; ++i) gk_strafe_hold_step(st, c, 10.0f, 0.1f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, c.i_max_pwm, st.integ);  // pegado al tope, no infinito
}

void test_reset_clears_integral(void) {
    GkStrafeHoldState st{0.0f};
    GkStrafeHoldCfg c = cfg();
    for (int i = 0; i < 20; ++i) gk_strafe_hold_step(st, c, 8.0f, 0.1f);
    TEST_ASSERT_TRUE(st.integ > 0.0f);
    gk_strafe_hold_reset(st);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, st.integ);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_deadband_no_trim);
    RUN_TEST(test_proportional_sign_and_value);
    RUN_TEST(test_trim_clamped_small);
    RUN_TEST(test_out_of_band_flag);
    RUN_TEST(test_integral_cancels_sustained_drift);
    RUN_TEST(test_antiwindup_clamps_integral);
    RUN_TEST(test_reset_clears_integral);
    return UNITY_END();
}
