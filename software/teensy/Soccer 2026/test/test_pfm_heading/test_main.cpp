// test_pfm_heading — tests del control PI+PFM de rumbo para actuador con zona
// muerta (src/shared/pfm_heading.h; banco María 2026-06-12).
// Corre en host con: bash scripts/run-host-tests.sh test_pfm_heading
//
// Contrato:
//   • |err| < deadband → salida 0 SIEMPRE (anti-temblequeo).
//   • err inválido → salida 0 e integrador congelado.
//   • La salida es SIEMPRE 0 o ±omega_on (pulsos de magnitud fija — PFM).
//   • La FRACCIÓN de ticks ON en una ventana ≈ duty = (kp·err + integ)/omega_on.
//   • El integrador APRENDE una perturbación sostenida (err constante → integ
//     crece → duty sube) y respeta el anti-windup (clamp + no integrar saturado).

#include <unity.h>
#include <cmath>
#include "pfm_heading.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

static PfmHeadingCfg cfg_test() {
    PfmHeadingCfg c = pfm_heading_default_cfg();
    c.kp = 2.0f; c.ki = 0.4f; c.deadband_deg = 5.0f;
    c.omega_on_degps = 100.0f; c.integ_max_degps = 100.0f; c.window_ms = 160;
    return c;
}

// Corre N ticks de 10 ms con err constante; devuelve fracción de ticks ON.
static float run_duty(PfmHeadingState& s, const PfmHeadingCfg& c,
                      float err, int n_ticks, uint32_t t_start = 1000) {
    int on = 0;
    for (int i = 0; i < n_ticks; ++i) {
        const float w = pfm_heading_tick(s, c, err, true,
                                         t_start + static_cast<uint32_t>(i) * 10, 0.01f);
        if (w != 0.0f) ++on;
    }
    return static_cast<float>(on) / static_cast<float>(n_ticks);
}

void test_deadband_zero_output(void){
    PfmHeadingState s; pfm_heading_reset(s);
    const PfmHeadingCfg c = cfg_test();
    for (int i = 0; i < 50; ++i) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
            pfm_heading_tick(s, c, 3.0f, true, 1000 + i * 10, 0.01f));
    }
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, s.integ_degps);  // tampoco integra
}

void test_invalid_err_zero_and_frozen(void){
    PfmHeadingState s; pfm_heading_reset(s);
    const PfmHeadingCfg c = cfg_test();
    s.integ_degps = 30.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
        pfm_heading_tick(s, c, 50.0f, false, 1000, 0.01f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.0f, s.integ_degps);  // congelado
}

void test_output_is_pulse_of_fixed_magnitude(void){
    PfmHeadingState s; pfm_heading_reset(s);
    const PfmHeadingCfg c = cfg_test();
    for (int i = 0; i < 100; ++i) {
        const float w = pfm_heading_tick(s, c, 20.0f, true, 1000 + i * 10, 0.01f);
        const bool ok = (w == 0.0f) || (std::fabs(std::fabs(w) - c.omega_on_degps) < 0.001f);
        TEST_ASSERT_TRUE(ok);   // PFM: solo 0 o ±omega_on, nunca intermedio
    }
}

void test_duty_scales_with_error(void){
    // err=10 → u≈20 → duty≈0.2 ; err=30 → u≈60+integ → duty>0.5.
    PfmHeadingState s1; pfm_heading_reset(s1);
    PfmHeadingState s2; pfm_heading_reset(s2);
    const PfmHeadingCfg c = cfg_test();
    const float d_small = run_duty(s1, c, 10.0f, 160);   // 10 ventanas
    const float d_big   = run_duty(s2, c, 30.0f, 160);
    TEST_ASSERT_TRUE(d_small > 0.05f && d_small < 0.45f);  // ~0.2 (+integ creciendo)
    TEST_ASSERT_TRUE(d_big > d_small);                     // más error → más duty
}

void test_negative_error_negative_pulses(void){
    PfmHeadingState s; pfm_heading_reset(s);
    const PfmHeadingCfg c = cfg_test();
    bool saw_neg = false;
    for (int i = 0; i < 50; ++i) {
        const float w = pfm_heading_tick(s, c, -20.0f, true, 1000 + i * 10, 0.01f);
        TEST_ASSERT_TRUE(w <= 0.001f);          // jamás corrige al revés
        if (w < -1.0f) saw_neg = true;
    }
    TEST_ASSERT_TRUE(saw_neg);
}

void test_integrator_learns_sustained_disturbance(void){
    // Perturbación sostenida (err constante 10°): el integrador (feedforward
    // automático) crece → el duty del final supera al del principio.
    PfmHeadingState s; pfm_heading_reset(s);
    const PfmHeadingCfg c = cfg_test();
    const float d_first = run_duty(s, c, 10.0f, 160, 1000);     // primeras 10 ventanas
    (void)run_duty(s, c, 10.0f, 1600, 3000);                    // 16 s de aprendizaje
    const float d_later = run_duty(s, c, 10.0f, 160, 20000);
    TEST_ASSERT_TRUE(s.integ_degps > 5.0f);                     // aprendió
    TEST_ASSERT_TRUE(d_later > d_first);                        // y se nota en el duty
}

void test_antiwindup_clamps_integrator(void){
    PfmHeadingState s; pfm_heading_reset(s);
    const PfmHeadingCfg c = cfg_test();
    // 60 s de error grande: el integrador debe quedar clampeado, no infinito.
    for (int i = 0; i < 6000; ++i) {
        pfm_heading_tick(s, c, 40.0f, true, 1000 + i * 10, 0.01f);
    }
    TEST_ASSERT_TRUE(s.integ_degps <= c.integ_max_degps + 0.001f);
}

void test_reset_clears_state(void){
    PfmHeadingState s; pfm_heading_reset(s);
    const PfmHeadingCfg c = cfg_test();
    run_duty(s, c, 30.0f, 320);
    pfm_heading_reset(s);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, s.integ_degps);
    TEST_ASSERT_EQUAL_UINT32(0, s.win_t0_ms);
}

// --- Amortiguación por velocidad de giro (kd_rate, la "D" de un PD) ---

void test_rate_damping_off_by_default(void){
    // Con el cfg default (kd_rate=0) la velocidad de giro NO debe cambiar NADA:
    // el comportamiento es byte-idéntico al PI previo (no-regresión).
    const PfmHeadingCfg c = cfg_test();   // kd_rate=0 (default)
    PfmHeadingState s1; pfm_heading_reset(s1);
    PfmHeadingState s2; pfm_heading_reset(s2);
    for (int i = 0; i < 50; ++i) {
        const uint32_t t = 1000 + i * 10;
        const float a = pfm_heading_tick(s1, c, 20.0f, true, t, 0.01f, 0.0f);
        const float b = pfm_heading_tick(s2, c, 20.0f, true, t, 0.01f, 200.0f);  // rate grande
        TEST_ASSERT_FLOAT_WITHIN(0.001f, a, b);   // sin kd_rate, el rate es inerte
    }
}

void test_rate_damping_reduces_duty(void){
    // Con kd_rate>0 y el robot girando HACIA el target (rate>0, err>0), la
    // corrección deseada baja (u = kp·err − kd·rate + integ) → menos ticks ON.
    PfmHeadingCfg c = cfg_test();
    c.kd_rate = 0.4f;
    PfmHeadingState s_norate; pfm_heading_reset(s_norate);
    PfmHeadingState s_rate;   pfm_heading_reset(s_rate);
    int on_norate = 0, on_rate = 0;
    for (int i = 0; i < 16; ++i) {           // una ventana (160 ms)
        const uint32_t t = 1000 + i * 10;
        if (pfm_heading_tick(s_norate, c, 20.0f, true, t, 0.01f, 0.0f)  != 0.0f) ++on_norate;
        if (pfm_heading_tick(s_rate,   c, 20.0f, true, t, 0.01f, 40.0f) != 0.0f) ++on_rate;
    }
    TEST_ASSERT_TRUE(on_rate < on_norate);   // amortigua: recorta el duty
}

void test_rate_damping_against_motion_can_boost(void){
    // Girando EN CONTRA del target (rate<0, err>0) la amortiguación SUMA
    // corrección (−kd·(−rate) = +): u sube. Coherencia de signo del término D.
    PfmHeadingCfg c = cfg_test();
    c.kd_rate = 0.4f;
    PfmHeadingState s_norate; pfm_heading_reset(s_norate);
    PfmHeadingState s_rate;   pfm_heading_reset(s_rate);
    int on_norate = 0, on_rate = 0;
    for (int i = 0; i < 16; ++i) {
        const uint32_t t = 1000 + i * 10;
        if (pfm_heading_tick(s_norate, c, 10.0f, true, t, 0.01f,  0.0f)  != 0.0f) ++on_norate;
        if (pfm_heading_tick(s_rate,   c, 10.0f, true, t, 0.01f, -40.0f) != 0.0f) ++on_rate;
    }
    TEST_ASSERT_TRUE(on_rate >= on_norate);  // contra el movimiento → más corrección
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_deadband_zero_output);
    RUN_TEST(test_invalid_err_zero_and_frozen);
    RUN_TEST(test_output_is_pulse_of_fixed_magnitude);
    RUN_TEST(test_duty_scales_with_error);
    RUN_TEST(test_negative_error_negative_pulses);
    RUN_TEST(test_integrator_learns_sustained_disturbance);
    RUN_TEST(test_antiwindup_clamps_integrator);
    RUN_TEST(test_reset_clears_state);
    RUN_TEST(test_rate_damping_off_by_default);
    RUN_TEST(test_rate_damping_reduces_duty);
    RUN_TEST(test_rate_damping_against_motion_can_boost);
    return UNITY_END();
}
