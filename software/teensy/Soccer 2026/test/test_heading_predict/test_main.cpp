// test_heading_predict — tests unitarios para src/shared/heading_predict.h
// Corre en host con: bash scripts/run-host-tests.sh test_heading_predict
//                    pio test -e test_native -f test_heading_predict
//
// El módulo extrapola el rumbo (predict step): heading_estimado = ancla + ω·Δt,
// con las 3 reglas duras (ver heading_predict.h):
//   (a) ω MEDIDA de entrada (clamp defensivo).
//   (b) CAP de Δt (max_extrap_ms) — nunca extrapolar lejos.
//   (c) RESET por evento: re-anclar al cambiar el heading fusionado + deadband quieto.

#include <unity.h>
#include "heading_predict.h"

using namespace iitasoccer;

static HeadingPredictCfg C;
static HeadingPredictState s;

void setUp(void) {
    C = heading_predict_default_cfg();   // cap 60 ms, clamp ±600°/s, deadband 2°/s
    heading_predict_reset(s);
}
void tearDown(void) {}

// --- cebado: primera muestra → no hay extrapolación (age 0) -------------------
void test_prime_returns_anchor(void) {
    heading_predict_on_sample(s, /*heading_cd=*/0, /*gyro=*/100.0f, /*valid=*/true, /*now=*/1000);
    TEST_ASSERT_EQUAL_INT16(0, heading_predict_value(s, 1000, C));
}

// --- rotando CCW: ancla + ω·Δt -----------------------------------------------
void test_extrapolates_positive(void) {
    heading_predict_on_sample(s, 0, 100.0f, true, 0);
    // ω=100°/s, age=50 ms → 100·100·0.05 = 500 cd (5°)
    TEST_ASSERT_EQUAL_INT16(500, heading_predict_value(s, 50, C));
}

// --- rotando CW: signo negativo ----------------------------------------------
void test_extrapolates_negative(void) {
    heading_predict_on_sample(s, 0, -100.0f, true, 0);
    TEST_ASSERT_EQUAL_INT16(-500, heading_predict_value(s, 50, C));
}

// --- (b) CAP: edad > max_extrap_ms se satura ---------------------------------
void test_cap_saturates_age(void) {
    heading_predict_on_sample(s, 0, 100.0f, true, 0);
    // age real 200 ms, pero cap 60 ms → 100·100·0.06 = 600 cd. NO 2000.
    TEST_ASSERT_EQUAL_INT16(600, heading_predict_value(s, 200, C));
}

// --- (c) deadband: robot quieto (|ω|<deadband) → hold ------------------------
void test_deadband_holds(void) {
    heading_predict_on_sample(s, 1234, 1.0f, true, 0);   // 1°/s < 2°/s deadband
    TEST_ASSERT_EQUAL_INT16(1234, heading_predict_value(s, 50, C));
}

// --- (a) clamp de ω: una ω absurda se acota a max_gyro_dps -------------------
void test_gyro_clamp(void) {
    heading_predict_on_sample(s, 0, 10000.0f, true, 0);   // clamp a 600°/s
    // 600·100·0.06 = 3600 cd
    TEST_ASSERT_EQUAL_INT16(3600, heading_predict_value(s, 60, C));
}

// --- (c) re-anclaje al cambiar el heading fusionado (muestra fresca) ---------
void test_reanchor_on_heading_change(void) {
    heading_predict_on_sample(s, 0, 100.0f, true, 0);
    heading_predict_on_sample(s, 1000, 100.0f, true, 100);   // heading cambió → re-ancla en t=100
    // age desde t=100 = 10 ms → 1000 + 100·100·0.01 = 1100
    TEST_ASSERT_EQUAL_INT16(1100, heading_predict_value(s, 110, C));
}

// --- freeze-bridge: heading congelado (no cambia) pero gyro vivo → puentea ---
void test_frozen_heading_live_gyro_bridges(void) {
    heading_predict_on_sample(s, 0, 100.0f, true, 0);
    heading_predict_on_sample(s, 0, 100.0f, true, 30);   // MISMO heading (congelado), gyro vivo
    // ancla sigue en t=0; age=50 → 0 + 100·100·0.05 = 500 (avanza con el gyro)
    TEST_ASSERT_EQUAL_INT16(500, heading_predict_value(s, 50, C));
}

// --- heading inválido → des-ceba y devuelve el ancla pasada ------------------
void test_invalid_resets(void) {
    heading_predict_on_sample(s, 500, 100.0f, true, 0);
    heading_predict_on_sample(s, 0, 100.0f, /*valid=*/false, 50);
    TEST_ASSERT_FALSE(s.primed);
    TEST_ASSERT_EQUAL_INT16(0, heading_predict_value(s, 60, C));
}

// --- wrap positivo: cruza +180 ----------------------------------------------
void test_wrap_positive(void) {
    heading_predict_on_sample(s, 17900, 600.0f, true, 0);   // 179°
    // +3600 cd (36°) → 21500 → wrap → -14500
    TEST_ASSERT_EQUAL_INT16(-14500, heading_predict_value(s, 60, C));
}

// --- wrap negativo: cruza -180 ----------------------------------------------
void test_wrap_negative(void) {
    heading_predict_on_sample(s, -17900, -600.0f, true, 0);
    // -3600 cd → -21500 → wrap → 14500
    TEST_ASSERT_EQUAL_INT16(14500, heading_predict_value(s, 60, C));
}

// --- usa Δt REAL, no asume período fijo --------------------------------------
void test_uses_real_elapsed(void) {
    heading_predict_on_sample(s, 0, 200.0f, true, 1000);
    // age = 30 ms → 200·100·0.03 = 600
    TEST_ASSERT_EQUAL_INT16(600, heading_predict_value(s, 1030, C));
}

// --- reset limpia el estado --------------------------------------------------
void test_reset_clears(void) {
    heading_predict_on_sample(s, 500, 100.0f, true, 0);
    heading_predict_reset(s);
    TEST_ASSERT_FALSE(s.primed);
    TEST_ASSERT_EQUAL_INT16(0, heading_predict_value(s, 50, C));
}

// --- defaults sanos ----------------------------------------------------------
void test_default_cfg_sane(void) {
    HeadingPredictCfg d = heading_predict_default_cfg();
    TEST_ASSERT_TRUE(d.max_extrap_ms >= 40u && d.max_extrap_ms <= 100u);  // cap << 500
    TEST_ASSERT_TRUE(d.max_gyro_dps > 0.0f);
    TEST_ASSERT_TRUE(d.deadband_dps > 0.0f && d.deadband_dps < d.max_gyro_dps);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_prime_returns_anchor);
    RUN_TEST(test_extrapolates_positive);
    RUN_TEST(test_extrapolates_negative);
    RUN_TEST(test_cap_saturates_age);
    RUN_TEST(test_deadband_holds);
    RUN_TEST(test_gyro_clamp);
    RUN_TEST(test_reanchor_on_heading_change);
    RUN_TEST(test_frozen_heading_live_gyro_bridges);
    RUN_TEST(test_invalid_resets);
    RUN_TEST(test_wrap_positive);
    RUN_TEST(test_wrap_negative);
    RUN_TEST(test_uses_real_elapsed);
    RUN_TEST(test_reset_clears);
    RUN_TEST(test_default_cfg_sane);
    return UNITY_END();
}
