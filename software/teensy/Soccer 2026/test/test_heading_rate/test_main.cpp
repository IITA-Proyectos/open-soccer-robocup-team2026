// test_heading_rate — tests del estimador de velocidad angular robusto a la
// cadencia lenta del sensor (src/shared/heading_rate.h; coach 2026-06-14).
// Corre en host con: bash scripts/run-host-tests.sh test_heading_rate
//
// Contrato:
//   • Primera muestra → velocidad 0 (se ceba, no inventa).
//   • Rumbo que crece linealmente → velocidad ≈ Δrumbo/Δt (CCW+ positivo).
//   • Rumbo REPETIDO entre muestras → MANTIENE la última velocidad (no la pone
//     en 0 cada tick: justo lo que evita el ruido de cuantización a 100 Hz).
//   • Rumbo quieto por > stale_ms → velocidad decae a 0.
//   • Jitter < min_change_deg → se ignora (no genera velocidad fantasma).
//   • Cruce de ±180 → diferencia envuelta (no salto de 360°).
//   • Clamp defensivo a ±max_abs_degps.

#include <unity.h>
#include <cmath>
#include "heading_rate.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

void test_first_sample_zero(void){
    HeadingRateState s; heading_rate_reset(s);
    const HeadingRateCfg c = heading_rate_default_cfg();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, heading_rate_update(s, 10.0f, 1000, c));
}

void test_linear_increase_gives_rate(void){
    // +5° cada 50 ms = 100°/s (cadencia real del BNO a 20 Hz).
    HeadingRateState s; heading_rate_reset(s);
    const HeadingRateCfg c = heading_rate_default_cfg();
    heading_rate_update(s, 0.0f, 1000, c);            // ceba
    float r = 0.0f;
    for (int i = 1; i <= 5; ++i) {
        r = heading_rate_update(s, 5.0f * i, 1000 + 50u * i, c);
    }
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 100.0f, r);        // ≈ +100°/s
}

void test_holds_rate_between_repeats(void){
    // El dato del BNO llega repetido 4 de cada 5 ticks (100 Hz vs 20 Hz):
    // entre repeticiones la velocidad se MANTIENE (no cae a 0).
    HeadingRateState s; heading_rate_reset(s);
    const HeadingRateCfg c = heading_rate_default_cfg();
    heading_rate_update(s, 0.0f, 1000, c);
    heading_rate_update(s, 5.0f, 1050, c);            // muestra nueva → ~100°/s
    // 4 ticks de 10 ms con el MISMO valor (repetido): mantiene la velocidad.
    float r = 0.0f;
    for (int i = 1; i <= 4; ++i) r = heading_rate_update(s, 5.0f, 1050 + 10u * i, c);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 100.0f, r);        // sigue ~100, no 0
}

void test_still_decays_to_zero(void){
    // Tras stale_ms sin cambio → velocidad 0 (robot quieto en rumbo).
    HeadingRateState s; heading_rate_reset(s);
    const HeadingRateCfg c = heading_rate_default_cfg();
    heading_rate_update(s, 0.0f, 1000, c);
    heading_rate_update(s, 5.0f, 1050, c);            // venía girando
    const float r = heading_rate_update(s, 5.0f, 1050 + c.stale_ms + 10u, c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, r);        // quieto hace rato → 0
}

void test_jitter_ignored(void){
    // Jitter del BNO quieto (< min_change_deg) no debe generar velocidad.
    HeadingRateState s; heading_rate_reset(s);
    const HeadingRateCfg c = heading_rate_default_cfg();
    heading_rate_update(s, 30.0f, 1000, c);
    float r = 0.0f;
    for (int i = 1; i <= 5; ++i) {
        const float jit = (i % 2 == 0) ? 0.03f : -0.03f;  // ±0,03° < 0,1°
        r = heading_rate_update(s, 30.0f + jit, 1000 + 10u * i, c);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, r);        // jitter ignorado → 0
}

void test_wraps_across_180(void){
    // 179° → -179° es +2° (CCW), no -358°.
    HeadingRateState s; heading_rate_reset(s);
    const HeadingRateCfg c = heading_rate_default_cfg();
    heading_rate_update(s, 179.0f, 1000, c);
    const float r = heading_rate_update(s, -179.0f, 1050, c);  // +2° en 50 ms = 40°/s
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 40.0f, r);
}

void test_negative_rate_for_cw(void){
    // Girar CW (rumbo decreciente) → velocidad negativa.
    HeadingRateState s; heading_rate_reset(s);
    const HeadingRateCfg c = heading_rate_default_cfg();
    heading_rate_update(s, 0.0f, 1000, c);
    const float r = heading_rate_update(s, -5.0f, 1050, c);
    TEST_ASSERT_TRUE(r < -50.0f);                     // ~ -100°/s
}

void test_clamp(void){
    // Salto enorme en poco tiempo → clamp a ±max_abs_degps.
    HeadingRateState s; heading_rate_reset(s);
    const HeadingRateCfg c = heading_rate_default_cfg();
    heading_rate_update(s, 0.0f, 1000, c);
    const float r = heading_rate_update(s, 170.0f, 1010, c);  // 170°/10ms = 17000°/s
    TEST_ASSERT_FLOAT_WITHIN(0.001f, c.max_abs_degps, r);     // clampeado
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_first_sample_zero);
    RUN_TEST(test_linear_increase_gives_rate);
    RUN_TEST(test_holds_rate_between_repeats);
    RUN_TEST(test_still_decays_to_zero);
    RUN_TEST(test_jitter_ignored);
    RUN_TEST(test_wraps_across_180);
    RUN_TEST(test_negative_rate_for_cw);
    RUN_TEST(test_clamp);
    return UNITY_END();
}
