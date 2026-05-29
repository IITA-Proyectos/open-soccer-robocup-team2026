// test_line_filters — tests unitarios para src/shared/line_filters.cpp
// Corre en host con: pio test -e test_native -f test_line_filters

#include <unity.h>
#include <cmath>
#include <cstring>
#include "line_filters.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// FILTRO TEMPORAL (moving average)
// ============================================================================

void test_temporal_steady_state_returns_same_value(void) {
    FilterBuffer buf{};
    // Llenar buffer con el mismo valor.
    for (int i = 0; i < 10; ++i) {
        uint16_t out = lf_temporal_update(buf, 500);
        TEST_ASSERT_UINT16_WITHIN(2, 500, out);  // tolerancia rounding
    }
}

void test_temporal_smooths_single_spike(void) {
    FilterBuffer buf{};
    // 4 muestras a 100 → buffer lleno con 100s.
    for (int i = 0; i < LF_FILTER_BUFFER_SIZE; ++i) lf_temporal_update(buf, 100);
    // Una muestra a 200 — el promedio debería ser (3*100 + 200) / 4 = 125.
    uint16_t out = lf_temporal_update(buf, 200);
    TEST_ASSERT_UINT16_WITHIN(2, 125, out);
}

void test_temporal_converges_to_new_value(void) {
    FilterBuffer buf{};
    // 4 muestras a 0 → output ~0.
    for (int i = 0; i < LF_FILTER_BUFFER_SIZE; ++i) lf_temporal_update(buf, 0);
    // 4 muestras consecutivas a 1000 → output debería converger a 1000.
    uint16_t out = 0;
    for (int i = 0; i < LF_FILTER_BUFFER_SIZE; ++i) out = lf_temporal_update(buf, 1000);
    TEST_ASSERT_UINT16_WITHIN(5, 1000, out);
}

void test_temporal_first_sample_returns_itself(void) {
    FilterBuffer buf{};
    uint16_t out = lf_temporal_update(buf, 600);
    TEST_ASSERT_EQUAL_UINT16(600, out);
}

// ============================================================================
// HYSTERESIS
// ============================================================================

void test_hysteresis_enters_white_only_above_high_band(void) {
    // threshold=500, band=20 → entrar a blanco requiere >= 520.
    TEST_ASSERT_FALSE(lf_hysteresis_on_white(519, 500, false));
    TEST_ASSERT_TRUE(lf_hysteresis_on_white(520, 500, false));
    TEST_ASSERT_TRUE(lf_hysteresis_on_white(550, 500, false));
}

void test_hysteresis_exits_white_only_below_low_band(void) {
    // threshold=500, band=20 → salir de blanco requiere < 480.
    TEST_ASSERT_TRUE(lf_hysteresis_on_white(480, 500, true));
    TEST_ASSERT_TRUE(lf_hysteresis_on_white(490, 500, true));
    TEST_ASSERT_FALSE(lf_hysteresis_on_white(479, 500, true));
}

void test_hysteresis_no_flicker_in_band(void) {
    // En la banda 480-519 (estado anterior=false), nunca pasa a true.
    for (int v = 480; v <= 519; ++v) {
        TEST_ASSERT_FALSE(lf_hysteresis_on_white(v, 500, false));
    }
    // En la banda 480-519 (estado anterior=true), nunca pasa a false.
    for (int v = 480; v <= 519; ++v) {
        TEST_ASSERT_TRUE(lf_hysteresis_on_white(v, 500, true));
    }
}

// ============================================================================
// FILTRO ESPACIAL
// ============================================================================

void test_spatial_rejects_isolated_sensor(void) {
    constexpr int N = 8;
    bool in[N]  = {false, false, false, true, false, false, false, false};
    bool out[N] = {};
    lf_spatial_filter(in, out, N);
    // Sensor 3 estaba aislado (vecinos 2 y 4 son false) → debe descartarse.
    TEST_ASSERT_FALSE(out[3]);
    for (int i = 0; i < N; ++i) TEST_ASSERT_FALSE(out[i]);
}

void test_spatial_keeps_consecutive_sensors(void) {
    constexpr int N = 8;
    bool in[N]  = {false, false, true, true, true, false, false, false};
    bool out[N] = {};
    lf_spatial_filter(in, out, N);
    // Sensores 2, 3, 4 tienen vecinos blancos → todos validados.
    TEST_ASSERT_TRUE(out[2]);
    TEST_ASSERT_TRUE(out[3]);
    TEST_ASSERT_TRUE(out[4]);
    TEST_ASSERT_FALSE(out[1]);
    TEST_ASSERT_FALSE(out[5]);
}

void test_spatial_anillo_wraparound(void) {
    constexpr int N = 8;
    // Sensores 7 y 0 son vecinos (anillo cerrado).
    bool in[N]  = {true, false, false, false, false, false, false, true};
    bool out[N] = {};
    lf_spatial_filter(in, out, N);
    // Sensor 0 tiene vecino 7 (true) y vecino 1 (false) → validado.
    TEST_ASSERT_TRUE(out[0]);
    // Sensor 7 tiene vecino 6 (false) y vecino 0 (true) → validado.
    TEST_ASSERT_TRUE(out[7]);
}

void test_spatial_all_white_all_validated(void) {
    constexpr int N = 8;
    bool in[N];
    bool out[N] = {};
    for (int i = 0; i < N; ++i) in[i] = true;
    lf_spatial_filter(in, out, N);
    for (int i = 0; i < N; ++i) TEST_ASSERT_TRUE(out[i]);
}

void test_spatial_inplace(void) {
    constexpr int N = 8;
    bool buf[N] = {false, true, true, false, false, true, true, false};
    lf_spatial_filter(buf, buf, N);
    TEST_ASSERT_TRUE(buf[1]);
    TEST_ASSERT_TRUE(buf[2]);
    TEST_ASSERT_TRUE(buf[5]);
    TEST_ASSERT_TRUE(buf[6]);
    TEST_ASSERT_FALSE(buf[0]);
    TEST_ASSERT_FALSE(buf[3]);
    TEST_ASSERT_FALSE(buf[4]);
    TEST_ASSERT_FALSE(buf[7]);
}

// ============================================================================
// SATURACIÓN "TODO BLANCO" (TEMA P1.5 — 2026-05-29)
// ============================================================================

void test_all_white_full_ring_is_saturated(void) {
    constexpr int N = 8;
    bool w[N]; for (int i = 0; i < N; ++i) w[i] = true;
    TEST_ASSERT_TRUE(lf_all_white(w, N, (N * 7) / 8));  // 8 >= 7
}

void test_all_white_below_threshold_not_saturated(void) {
    constexpr int N = 8;
    // 6 de 8 blancos: línea fuerte pero NO saturación (umbral 7/8 = 7).
    bool w[N] = {true, true, true, true, true, true, false, false};
    TEST_ASSERT_FALSE(lf_all_white(w, N, (N * 7) / 8));  // 6 < 7
}

void test_all_white_exact_threshold_is_saturated(void) {
    constexpr int N = 8;
    // Exactamente el umbral cuenta como saturado (>=, no >).
    bool w[N] = {true, true, true, true, true, true, true, false};
    TEST_ASSERT_TRUE(lf_all_white(w, N, 7));  // 7 >= 7
}

void test_all_white_none_white_not_saturated(void) {
    constexpr int N = 8;
    bool w[N] = {};  // todos false (carpet)
    TEST_ASSERT_FALSE(lf_all_white(w, N, 1));
}

void test_all_white_guards_invalid_args(void) {
    bool w[8]; for (int i = 0; i < 8; ++i) w[i] = true;
    TEST_ASSERT_FALSE(lf_all_white(w, 0, 1));        // n <= 0
    TEST_ASSERT_FALSE(lf_all_white(nullptr, 8, 1));  // ptr nulo
}

void test_all_white_32_ring_thresholds(void) {
    constexpr int N = 32;
    bool w[N];
    // 28/32 blancos (umbral 32*7/8 = 28) → saturado.
    for (int i = 0; i < N; ++i) w[i] = (i < 28);
    TEST_ASSERT_TRUE(lf_all_white(w, N, (N * 7) / 8));
    // 27/32 → NO saturado (línea grande en esquina, pero plausible).
    w[27] = false;
    TEST_ASSERT_FALSE(lf_all_white(w, N, (N * 7) / 8));
}

// ============================================================================
// CENTROIDE ANGULAR
// ============================================================================

void test_angle_single_sensor_at_0_returns_0_deg(void) {
    constexpr int N = 32;
    bool validated[N] = {};
    uint16_t raw[N] = {}, thr[N], white[N];
    for (int i = 0; i < N; ++i) { thr[i] = 500; white[i] = 800; raw[i] = 800; }
    validated[0] = true;  // sensor 0 en frente (0°)
    auto r = lf_compute_centroid_angle(validated, raw, thr, white, N);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_INT(1, r.depth);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, r.angle_deg);
}

void test_angle_single_sensor_at_90_deg(void) {
    constexpr int N = 32;
    bool validated[N] = {};
    uint16_t raw[N] = {}, thr[N], white[N];
    for (int i = 0; i < N; ++i) { thr[i] = 500; white[i] = 800; raw[i] = 800; }
    // Sensor en índice 8 (32 sensores × 11.25° = 90°).
    validated[8] = true;
    auto r = lf_compute_centroid_angle(validated, raw, thr, white, N);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 90.0f, r.angle_deg);
}

void test_angle_single_sensor_at_180_deg(void) {
    constexpr int N = 32;
    bool validated[N] = {};
    uint16_t raw[N] = {}, thr[N], white[N];
    for (int i = 0; i < N; ++i) { thr[i] = 500; white[i] = 800; raw[i] = 800; }
    validated[16] = true;  // 16 × 11.25 = 180°
    auto r = lf_compute_centroid_angle(validated, raw, thr, white, N);
    TEST_ASSERT_TRUE(r.valid);
    // 180° puede venir como ±180 según atan2 — aceptamos ambos.
    float deg = r.angle_deg;
    if (deg < 0) deg += 360.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 180.0f, deg);
}

void test_angle_3_consecutive_sensors_centroid(void) {
    constexpr int N = 32;
    bool validated[N] = {};
    uint16_t raw[N] = {}, thr[N], white[N];
    for (int i = 0; i < N; ++i) { thr[i] = 500; white[i] = 800; raw[i] = 800; }
    // Sensores 0, 1, 2 prendidos — centroide debería estar cerca del sensor 1 (11.25°).
    validated[0] = validated[1] = validated[2] = true;
    auto r = lf_compute_centroid_angle(validated, raw, thr, white, N);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_INT(3, r.depth);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 11.25f, r.angle_deg);
}

void test_angle_no_sensors_invalid(void) {
    constexpr int N = 32;
    bool validated[N] = {};
    uint16_t raw[N] = {}, thr[N] = {}, white[N];
    for (int i = 0; i < N; ++i) white[i] = 800;
    auto r = lf_compute_centroid_angle(validated, raw, thr, white, N);
    TEST_ASSERT_FALSE(r.valid);
    TEST_ASSERT_EQUAL_INT(0, r.depth);
}

void test_angle_intensity_weights_centroid(void) {
    constexpr int N = 32;
    bool validated[N] = {};
    uint16_t raw[N] = {}, thr[N], white[N];
    for (int i = 0; i < N; ++i) { thr[i] = 500; white[i] = 800; raw[i] = 500; }
    // Sensor 0 con peso pleno (raw=800), sensor 4 con peso parcial (raw=600 sobre threshold).
    validated[0] = true; raw[0] = 800;
    validated[4] = true; raw[4] = 600;  // intensidad 100/300 = 0.33
    auto r = lf_compute_centroid_angle(validated, raw, thr, white, N);
    TEST_ASSERT_TRUE(r.valid);
    // Centroide debería estar más cerca de 0° que del promedio 22.5° (mid de 0 y 45)
    // porque sensor 0 tiene mucho más peso.
    TEST_ASSERT_TRUE(r.angle_deg < 22.5f);
}

// ============================================================================
// LIFTED DETECTOR
// ============================================================================

void test_lifted_below_threshold_count_triggers(void) {
    constexpr int N = 32;
    uint16_t carpet[N], filtered[N];
    for (int i = 0; i < N; ++i) { carpet[i] = 300; filtered[i] = 100; }  // todos en aire

    LiftedDetector state{};
    lf_lifted_update(state, 0, filtered, carpet, N);
    TEST_ASSERT_FALSE(state.is_lifted);  // todavía en debounce
    lf_lifted_update(state, LF_LIFTED_DEBOUNCE_MS + 1, filtered, carpet, N);
    TEST_ASSERT_TRUE(state.is_lifted);
}

void test_lifted_few_sensors_low_does_not_trigger(void) {
    constexpr int N = 32;
    uint16_t carpet[N], filtered[N];
    for (int i = 0; i < N; ++i) { carpet[i] = 300; filtered[i] = 300; }
    // Solo 5 sensores en aire — menos que LF_LIFTED_MIN_SENSORS=28.
    for (int i = 0; i < 5; ++i) filtered[i] = 100;

    LiftedDetector state{};
    lf_lifted_update(state, 0, filtered, carpet, N);
    lf_lifted_update(state, 500, filtered, carpet, N);
    TEST_ASSERT_FALSE(state.is_lifted);
}

void test_lifted_debounce_anti_glitch(void) {
    constexpr int N = 32;
    uint16_t carpet[N], filtered[N];
    for (int i = 0; i < N; ++i) carpet[i] = 300;

    LiftedDetector state{};

    // Glitch corto (50 ms) — todos en aire pero solo por poco tiempo.
    for (int i = 0; i < N; ++i) filtered[i] = 100;
    lf_lifted_update(state, 0, filtered, carpet, N);
    lf_lifted_update(state, 50, filtered, carpet, N);
    TEST_ASSERT_FALSE(state.is_lifted);  // todavía no, debounce 100ms

    // Vuelven a normal antes del debounce → reset.
    for (int i = 0; i < N; ++i) filtered[i] = 300;
    lf_lifted_update(state, 60, filtered, carpet, N);
    TEST_ASSERT_FALSE(state.is_lifted);
    TEST_ASSERT_FALSE(state.candidate_active);
}

void test_lifted_recovers_when_back_on_floor(void) {
    constexpr int N = 32;
    uint16_t carpet[N], filtered[N];
    for (int i = 0; i < N; ++i) carpet[i] = 300;

    LiftedDetector state{};

    // Levantar: todos en aire durante > debounce.
    for (int i = 0; i < N; ++i) filtered[i] = 100;
    lf_lifted_update(state, 0, filtered, carpet, N);
    lf_lifted_update(state, 200, filtered, carpet, N);
    TEST_ASSERT_TRUE(state.is_lifted);

    // Volver al piso — debe desactivarse inmediatamente.
    for (int i = 0; i < N; ++i) filtered[i] = 300;
    lf_lifted_update(state, 210, filtered, carpet, N);
    TEST_ASSERT_FALSE(state.is_lifted);
}

// ============================================================================
// MAIN
// ============================================================================

// ============================================================================
// MuxWatchdog (TEMA 1 P0 — 2026-05-29)
// ============================================================================

// Helper: arma un array raw[32] con el mux m fijo en `value` y los demás
// "normales" (cada sensor con un valor distinto, simulando lectura sana).
static void fill_raw_with_mux_stuck(uint16_t* raw, int mux_to_stuck, uint16_t stuck_value) {
    for (int i = 0; i < 32; ++i) {
        const int m = i / MW_SENSORS_PER_MUX;
        if (m == mux_to_stuck) {
            raw[i] = stuck_value;
        } else {
            // Valores con variación sensor-a-sensor (simula lectura sana).
            raw[i] = (uint16_t)(300 + (i * 7) % 200);
        }
    }
}

void test_mw_init_clean(void) {
    MuxWatchdog w;
    mw_init(w);
    TEST_ASSERT_FALSE(mw_any_dead(w));
    TEST_ASSERT_EQUAL_UINT8(0, mw_get_dead_bitmap(w));
    for (int m = 0; m < MW_MAX_MUXES; ++m) {
        TEST_ASSERT_FALSE(mw_is_dead(w, m));
    }
}

void test_mw_stuck_low_extreme_marks_dead(void) {
    MuxWatchdog w;
    mw_init(w);
    uint16_t raw[32];
    fill_raw_with_mux_stuck(raw, /*mux*/2, /*value*/0);
    // Empezamos en t=0 y avanzamos 1 ms por tick durante 150 ms.
    for (uint32_t t = 0; t <= 150; ++t) {
        mw_update(w, raw, 4, MW_SENSORS_PER_MUX, t);
    }
    TEST_ASSERT_TRUE(mw_is_dead(w, 2));
    TEST_ASSERT_FALSE(mw_is_dead(w, 0));
    TEST_ASSERT_FALSE(mw_is_dead(w, 1));
    TEST_ASSERT_FALSE(mw_is_dead(w, 3));
}

void test_mw_stuck_high_extreme_marks_dead(void) {
    MuxWatchdog w;
    mw_init(w);
    uint16_t raw[32];
    fill_raw_with_mux_stuck(raw, /*mux*/0, /*value*/1023);
    for (uint32_t t = 0; t <= 150; ++t) {
        mw_update(w, raw, 4, MW_SENSORS_PER_MUX, t);
    }
    TEST_ASSERT_TRUE(mw_is_dead(w, 0));
}

void test_mw_stuck_mid_value_no_dead(void) {
    // Caso "8 sensores casualmente coinciden en 500": NO debe marcar dead.
    // Mid (no extremo) → false positive filtrado.
    MuxWatchdog w;
    mw_init(w);
    uint16_t raw[32];
    fill_raw_with_mux_stuck(raw, /*mux*/1, /*value*/500);
    for (uint32_t t = 0; t <= 500; ++t) {  // 500ms para asegurar
        mw_update(w, raw, 4, MW_SENSORS_PER_MUX, t);
    }
    TEST_ASSERT_FALSE(mw_is_dead(w, 1));
    TEST_ASSERT_FALSE(mw_any_dead(w));
}

void test_mw_varying_values_no_dead(void) {
    MuxWatchdog w;
    mw_init(w);
    uint16_t raw[32];
    for (int i = 0; i < 32; ++i) raw[i] = (uint16_t)(100 + i * 30);
    for (uint32_t t = 0; t <= 500; ++t) {
        mw_update(w, raw, 4, MW_SENSORS_PER_MUX, t);
    }
    TEST_ASSERT_FALSE(mw_any_dead(w));
}

void test_mw_recovery_after_dead(void) {
    MuxWatchdog w;
    mw_init(w);
    uint16_t raw[32];
    // Fase A: stuck → dead.
    fill_raw_with_mux_stuck(raw, /*mux*/0, /*value*/1023);
    for (uint32_t t = 0; t <= 150; ++t) {
        mw_update(w, raw, 4, MW_SENSORS_PER_MUX, t);
    }
    TEST_ASSERT_TRUE(mw_is_dead(w, 0));

    // Fase B: valores vuelven a variar → auto-recovery.
    for (int i = 0; i < 32; ++i) raw[i] = (uint16_t)(100 + (i * 11) % 400);
    mw_update(w, raw, 4, MW_SENSORS_PER_MUX, 200);
    TEST_ASSERT_FALSE(mw_is_dead(w, 0));
}

void test_mw_bitmap_one_mux_dead(void) {
    MuxWatchdog w;
    mw_init(w);
    uint16_t raw[32];
    fill_raw_with_mux_stuck(raw, /*mux*/3, /*value*/0);
    for (uint32_t t = 0; t <= 150; ++t) {
        mw_update(w, raw, 4, MW_SENSORS_PER_MUX, t);
    }
    TEST_ASSERT_EQUAL_UINT8(0x08, mw_get_dead_bitmap(w));  // bit 3 = 1<<3 = 8
}

void test_mw_bitmap_multiple_dead(void) {
    MuxWatchdog w;
    mw_init(w);
    uint16_t raw[32];
    // Forzar mux 0 y mux 2 a stuck en valores extremos diferentes.
    for (int i = 0; i < 32; ++i) {
        const int m = i / MW_SENSORS_PER_MUX;
        if (m == 0) raw[i] = 0;
        else if (m == 2) raw[i] = 1023;
        else raw[i] = (uint16_t)(300 + (i * 7) % 200);
    }
    for (uint32_t t = 0; t <= 150; ++t) {
        mw_update(w, raw, 4, MW_SENSORS_PER_MUX, t);
    }
    TEST_ASSERT_EQUAL_UINT8(0x05, mw_get_dead_bitmap(w));  // bits 0 y 2 → 0b0101
}

void test_mw_no_dead_before_debounce(void) {
    // Antes de que pase debounce_ms, NO debería marcar dead.
    MuxWatchdog w;
    mw_init(w);
    uint16_t raw[32];
    fill_raw_with_mux_stuck(raw, /*mux*/1, /*value*/0);
    // Tick t=0 inicia racha; t=50ms aún no llegó al debounce de 100ms.
    for (uint32_t t = 0; t <= 50; ++t) {
        mw_update(w, raw, 4, MW_SENSORS_PER_MUX, t);
    }
    TEST_ASSERT_FALSE(mw_is_dead(w, 1));
}

void test_mw_oob_index_safe(void) {
    MuxWatchdog w;
    mw_init(w);
    TEST_ASSERT_FALSE(mw_is_dead(w, -1));
    TEST_ASSERT_FALSE(mw_is_dead(w, MW_MAX_MUXES));
    TEST_ASSERT_FALSE(mw_is_dead(w, 99));
}

void test_mw_now_ms_wrap_no_false_positive(void) {
    // Fix audit 2026-05-29: si now_ms retrocede (millis() wrap a ~49.7 días),
    // mw_update no debe disparar dead inmediato por elapsed gigante.
    MuxWatchdog w;
    mw_init(w);
    uint16_t raw[32];
    fill_raw_with_mux_stuck(raw, /*mux*/0, /*value*/0);
    // Fase A: tick normal a t=1000, arranca racha.
    mw_update(w, raw, 4, MW_SENSORS_PER_MUX, 1000);
    // Fase B: now_ms retrocede a t=50 (simula wrap). Racha debería reiniciarse,
    // NO disparar dead inmediato a pesar de que aparente elapsed = -950 → 4G.
    mw_update(w, raw, 4, MW_SENSORS_PER_MUX, 50);
    TEST_ASSERT_FALSE(mw_is_dead(w, 0));
    // Fase C: avanzar normalmente desde el nuevo cero. Racha completa →
    // confirmar que sigue funcionando post-wrap.
    for (uint32_t t = 51; t <= 200; ++t) {
        mw_update(w, raw, 4, MW_SENSORS_PER_MUX, t);
    }
    TEST_ASSERT_TRUE(mw_is_dead(w, 0));
}

int main(int, char**) {
    UNITY_BEGIN();

    // Temporal
    RUN_TEST(test_temporal_first_sample_returns_itself);
    RUN_TEST(test_temporal_steady_state_returns_same_value);
    RUN_TEST(test_temporal_smooths_single_spike);
    RUN_TEST(test_temporal_converges_to_new_value);

    // Hysteresis
    RUN_TEST(test_hysteresis_enters_white_only_above_high_band);
    RUN_TEST(test_hysteresis_exits_white_only_below_low_band);
    RUN_TEST(test_hysteresis_no_flicker_in_band);

    // Spatial
    RUN_TEST(test_spatial_rejects_isolated_sensor);
    RUN_TEST(test_spatial_keeps_consecutive_sensors);
    RUN_TEST(test_spatial_anillo_wraparound);
    RUN_TEST(test_spatial_all_white_all_validated);
    RUN_TEST(test_spatial_inplace);

    // Saturación todo-blanco (TEMA P1.5)
    RUN_TEST(test_all_white_full_ring_is_saturated);
    RUN_TEST(test_all_white_below_threshold_not_saturated);
    RUN_TEST(test_all_white_exact_threshold_is_saturated);
    RUN_TEST(test_all_white_none_white_not_saturated);
    RUN_TEST(test_all_white_guards_invalid_args);
    RUN_TEST(test_all_white_32_ring_thresholds);

    // Centroide angular
    RUN_TEST(test_angle_single_sensor_at_0_returns_0_deg);
    RUN_TEST(test_angle_single_sensor_at_90_deg);
    RUN_TEST(test_angle_single_sensor_at_180_deg);
    RUN_TEST(test_angle_3_consecutive_sensors_centroid);
    RUN_TEST(test_angle_no_sensors_invalid);
    RUN_TEST(test_angle_intensity_weights_centroid);

    // Lifted
    RUN_TEST(test_lifted_below_threshold_count_triggers);
    RUN_TEST(test_lifted_few_sensors_low_does_not_trigger);
    RUN_TEST(test_lifted_debounce_anti_glitch);
    RUN_TEST(test_lifted_recovers_when_back_on_floor);

    // MuxWatchdog (TEMA 1 P0)
    RUN_TEST(test_mw_init_clean);
    RUN_TEST(test_mw_stuck_low_extreme_marks_dead);
    RUN_TEST(test_mw_stuck_high_extreme_marks_dead);
    RUN_TEST(test_mw_stuck_mid_value_no_dead);
    RUN_TEST(test_mw_varying_values_no_dead);
    RUN_TEST(test_mw_recovery_after_dead);
    RUN_TEST(test_mw_bitmap_one_mux_dead);
    RUN_TEST(test_mw_bitmap_multiple_dead);
    RUN_TEST(test_mw_no_dead_before_debounce);
    RUN_TEST(test_mw_oob_index_safe);
    RUN_TEST(test_mw_now_ms_wrap_no_false_positive);

    return UNITY_END();
}
