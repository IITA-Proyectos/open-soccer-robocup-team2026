// test_pose_fusion — tests unitarios del filtro complementario ToF+OTOS (puro).
// Corre en host con: bash scripts/run-host-tests.sh test_pose_fusion
//
// Cubre: seed con ToF, no-init sin absoluto, deriva OTOS, corrección suave,
// gating de saltos, pérdida temporal de ToF, anti-teletransporte, pass-through
// del heading y clamp de salida a cancha.

#include <unity.h>
#include "pose_fusion.h"

using namespace iitasoccer;

// ============================================================
// Helpers de test
// ============================================================
namespace {

PoseFusionConfig cfg;   // config estándar (defaults) — se re-arma en setUp.

// Construye un PoseFusionInputs con todos los campos.
PoseFusionInputs make_inputs(int16_t tof_x, int16_t tof_y, bool tof_valid,
                             int16_t otos_x, int16_t otos_y, bool otos_fresh,
                             int16_t heading, uint16_t dt_ms) {
    PoseFusionInputs in{};
    in.tof_x_mm             = tof_x;
    in.tof_y_mm             = tof_y;
    in.tof_valid            = tof_valid;
    in.otos_x_mm            = otos_x;
    in.otos_y_mm            = otos_y;
    in.otos_fresh           = otos_fresh;
    in.bno_heading_centideg = heading;
    in.dt_ms                = dt_ms;
    return in;
}

// Seed: primer tick con ToF válido en (x,y). Deja el estado inicializado.
PoseFusionOutput seed_at(PoseFusionState& st, int16_t x, int16_t y) {
    PoseFusionInputs in = make_inputs(x, y, /*tof_valid=*/true,
                                      /*otos=*/x, y, /*otos_fresh=*/true,
                                      /*heading=*/0, /*dt=*/10);
    return pose_fusion_update(st, in, cfg);
}

}  // namespace

void setUp(void)    { cfg = pose_fusion_default_config(); }
void tearDown(void) {}

// ============================================================
// test_seed_con_tof_inicializa
// ============================================================
void test_seed_con_tof_inicializa(void) {
    PoseFusionState st;
    pose_fusion_reset(st);
    // Primer update con tof_valid en (1000,800), otos cualquier valor.
    PoseFusionInputs in = make_inputs(1000, 800, /*tof_valid=*/true,
                                      /*otos=*/12345, -999, /*otos_fresh=*/true,
                                      /*heading=*/0, /*dt=*/10);
    PoseFusionOutput out = pose_fusion_update(st, in, cfg);

    TEST_ASSERT_TRUE(out.valid);
    TEST_ASSERT_INT16_WITHIN(2, 1000, out.x_mm);
    TEST_ASSERT_INT16_WITHIN(2, 800, out.y_mm);
    TEST_ASSERT_TRUE(out.confidence >= cfg.conf_tof_anchor);  // >= 90
    // El seed cuenta como anclaje al absoluto.
    TEST_ASSERT_TRUE((out.source_flags & POSE_FUSION_FLAG_TOF_CORR) != 0);
}

// ============================================================
// test_sin_tof_y_sin_otos_no_inicializa
// ============================================================
void test_sin_tof_y_sin_otos_no_inicializa(void) {
    PoseFusionState st;
    pose_fusion_reset(st);
    PoseFusionInputs in = make_inputs(0, 0, /*tof_valid=*/false,
                                      0, 0, /*otos_fresh=*/false,
                                      /*heading=*/7777, /*dt=*/10);
    PoseFusionOutput out = pose_fusion_update(st, in, cfg);

    TEST_ASSERT_FALSE(out.valid);
    TEST_ASSERT_EQUAL_UINT8(0, out.confidence);
    // Pass-through del heading aun sin inicializar.
    TEST_ASSERT_EQUAL_INT16(7777, out.heading_centideg);
}

// ============================================================
// test_otos_sola_deriva_poco_en_corto
// ============================================================
void test_otos_sola_deriva_poco_en_corto(void) {
    PoseFusionState st;
    pose_fusion_reset(st);
    seed_at(st, 1000, 800);  // ancla en (1000,800), otos_prev = (1000,800)

    // 50 ticks de SOLO OTOS (sin ToF), avanzando la pose OTOS +2mm/tick en X.
    PoseFusionOutput out{};
    int16_t otos_x = 1000;
    for (int i = 0; i < 50; ++i) {
        otos_x += 2;  // +2mm/tick => +100mm total (101mm 'reales' aprox)
        PoseFusionInputs in = make_inputs(0, 0, /*tof_valid=*/false,
                                          otos_x, 800, /*otos_fresh=*/true,
                                          /*heading=*/0, /*dt=*/10);
        out = pose_fusion_update(st, in, cfg);
    }
    // La fusión integra los deltas OTOS: x_final ~ 1100.
    TEST_ASSERT_INT16_WITHIN(2, 1100, out.x_mm);
    TEST_ASSERT_INT16_WITHIN(2, 800, out.y_mm);
    // Confidence decayó desde 90 hacia conf_otos_only (sin ToF reciente).
    TEST_ASSERT_TRUE(out.confidence < 90);
    TEST_ASSERT_TRUE(out.confidence >= cfg.conf_min);
}

// ============================================================
// test_tof_corrige_drift_otos
// ============================================================
void test_tof_corrige_drift_otos(void) {
    PoseFusionState st;
    pose_fusion_reset(st);
    seed_at(st, 1000, 800);

    // Inyectar un offset artificial de +150mm en X via deltas OTOS falsos
    // (la pose OTOS reporta avance pero la 'verdad' sigue en 1000,800).
    int16_t otos_x = 1000;
    for (int i = 0; i < 10; ++i) {
        otos_x += 15;  // 10 ticks de +15mm = +150mm de drift
        PoseFusionInputs in = make_inputs(0, 0, /*tof_valid=*/false,
                                          otos_x, 800, /*otos_fresh=*/true,
                                          /*heading=*/0, /*dt=*/10);
        pose_fusion_update(st, in, cfg);
    }
    // Pose fusionada arrastra ~+150mm de offset (x ~ 1150).
    TEST_ASSERT_INT16_WITHIN(5, 1150, static_cast<int16_t>(st.x_mm_q0));

    // Ahora activar tof_valid en (1000,800) por 30 ticks. OTOS congelada (delta 0).
    int32_t prev_err = 1150 - 1000;  // error inicial en X
    bool saw_corr = false;
    PoseFusionOutput out{};
    for (int i = 0; i < 30; ++i) {
        PoseFusionInputs in = make_inputs(1000, 800, /*tof_valid=*/true,
                                          otos_x, 800, /*otos_fresh=*/true,
                                          /*heading=*/0, /*dt=*/10);
        out = pose_fusion_update(st, in, cfg);
        if (out.source_flags & POSE_FUSION_FLAG_TOF_CORR) saw_corr = true;
        int32_t err = static_cast<int32_t>(out.x_mm) - 1000;
        if (err < 0) err = -err;
        // El error decae monotónicamente (cada corrección lo reduce).
        TEST_ASSERT_TRUE(err <= prev_err);
        prev_err = err;
    }
    // El ToF mató el drift: x converge a ~1000.
    TEST_ASSERT_INT16_WITHIN(15, 1000, out.x_mm);
    TEST_ASSERT_TRUE(saw_corr);  // bit1 visto en los ticks con ToF
}

// ============================================================
// test_correccion_es_suave_no_salta
// ============================================================
void test_correccion_es_suave_no_salta(void) {
    PoseFusionState st;
    pose_fusion_reset(st);
    seed_at(st, 1000, 800);

    // Un solo tick con ToF en (1300,800): salto de 300mm, bajo el gate (400).
    // OTOS sin avance (delta 0).
    PoseFusionInputs in = make_inputs(1300, 800, /*tof_valid=*/true,
                                      1000, 800, /*otos_fresh=*/true,
                                      /*heading=*/0, /*dt=*/10);
    PoseFusionOutput out = pose_fusion_update(st, in, cfg);

    // K=26/256: corrige ~10% de 300 => +30mm aprox. x ~ 1030 (NO salta a 1300).
    int16_t expected = static_cast<int16_t>(1000 + (300 * 26) / 256);  // = 1030
    TEST_ASSERT_INT16_WITHIN(5, expected, out.x_mm);
    TEST_ASSERT_TRUE((out.source_flags & POSE_FUSION_FLAG_TOF_CORR) != 0);
    // No heredó el salto del ToF.
    TEST_ASSERT_TRUE(out.x_mm < 1100);
}

// ============================================================
// test_tof_saltada_se_rechaza_por_gating
// ============================================================
void test_tof_saltada_se_rechaza_por_gating(void) {
    PoseFusionState st;
    pose_fusion_reset(st);
    seed_at(st, 1000, 800);

    // ToF en (1500,800): residual 500 > jump_gate 400 => rechazo. OTOS delta 0.
    PoseFusionInputs in = make_inputs(1500, 800, /*tof_valid=*/true,
                                      1000, 800, /*otos_fresh=*/true,
                                      /*heading=*/0, /*dt=*/10);
    PoseFusionOutput out = pose_fusion_update(st, in, cfg);

    // Pose se mantiene (no corrige).
    TEST_ASSERT_INT16_WITHIN(2, 1000, out.x_mm);
    TEST_ASSERT_TRUE((out.source_flags & POSE_FUSION_FLAG_TOF_REJECT) != 0);  // bit2
    TEST_ASSERT_TRUE((out.source_flags & POSE_FUSION_FLAG_TOF_CORR) == 0);    // bit1 NO
}

// ============================================================
// test_perdida_temporal_de_tof_no_rompe
// ============================================================
void test_perdida_temporal_de_tof_no_rompe(void) {
    PoseFusionState st;
    pose_fusion_reset(st);
    seed_at(st, 500, 500);

    // 20 ticks con ToF+OTOS coherentes (pose estable ~500,500).
    PoseFusionOutput out{};
    for (int i = 0; i < 20; ++i) {
        PoseFusionInputs in = make_inputs(500, 500, /*tof_valid=*/true,
                                          500, 500, /*otos_fresh=*/true,
                                          /*heading=*/0, /*dt=*/10);
        out = pose_fusion_update(st, in, cfg);
    }
    TEST_ASSERT_TRUE(out.valid);
    TEST_ASSERT_INT16_WITHIN(2, 500, out.x_mm);

    // 100 ticks SIN ToF (solo OTOS fresca, delta 0).
    for (int i = 0; i < 100; ++i) {
        PoseFusionInputs in = make_inputs(0, 0, /*tof_valid=*/false,
                                          500, 500, /*otos_fresh=*/true,
                                          /*heading=*/0, /*dt=*/10);
        out = pose_fusion_update(st, in, cfg);
        // Durante el gap: valid sigue true y la pose se mantiene.
        TEST_ASSERT_TRUE(out.valid);
        TEST_ASSERT_INT16_WITHIN(2, 500, out.x_mm);
    }
    // Confidence decae pero >= conf_min, nunca invalida.
    TEST_ASSERT_TRUE(out.confidence >= cfg.conf_min);

    // ToF vuelve por 10 ticks: confidence sube de nuevo y bit1 set.
    bool saw_corr = false;
    for (int i = 0; i < 10; ++i) {
        PoseFusionInputs in = make_inputs(500, 500, /*tof_valid=*/true,
                                          500, 500, /*otos_fresh=*/true,
                                          /*heading=*/0, /*dt=*/10);
        out = pose_fusion_update(st, in, cfg);
        if (out.source_flags & POSE_FUSION_FLAG_TOF_CORR) saw_corr = true;
    }
    TEST_ASSERT_TRUE(saw_corr);
    TEST_ASSERT_TRUE(out.confidence >= cfg.conf_tof_anchor - 5);  // vuelve a ~90
}

// ============================================================
// test_otos_no_fresca_mantiene_pose
// ============================================================
void test_otos_no_fresca_mantiene_pose(void) {
    PoseFusionState st;
    pose_fusion_reset(st);
    seed_at(st, 700, 600);

    PoseFusionOutput out{};
    for (int i = 0; i < 5; ++i) {
        // otos_fresh=false y tof_valid=false: sin predicción ni corrección.
        PoseFusionInputs in = make_inputs(0, 0, /*tof_valid=*/false,
                                          9999, 9999, /*otos_fresh=*/false,
                                          /*heading=*/0, /*dt=*/10);
        out = pose_fusion_update(st, in, cfg);
        TEST_ASSERT_INT16_WITHIN(2, 700, out.x_mm);
        TEST_ASSERT_INT16_WITHIN(2, 600, out.y_mm);
        // bit0 (predicción) NO set en estos ticks.
        TEST_ASSERT_TRUE((out.source_flags & POSE_FUSION_FLAG_OTOS_PRED) == 0);
    }
    // La confidence decae por ms_since_tof_corr.
    TEST_ASSERT_TRUE(out.confidence < 90);
}

// ============================================================
// test_otos_reset_clamp_no_teletransporta
// ============================================================
void test_otos_reset_clamp_no_teletransporta(void) {
    PoseFusionState st;
    pose_fusion_reset(st);
    seed_at(st, 1000, 800);  // otos_prev = (1000,800)

    // Un tick donde la OTOS salta a (5000,5000) (reset/glitch), fresca, sin ToF.
    PoseFusionInputs in = make_inputs(0, 0, /*tof_valid=*/false,
                                      5000, 5000, /*otos_fresh=*/true,
                                      /*heading=*/0, /*dt=*/10);
    PoseFusionOutput out = pose_fusion_update(st, in, cfg);

    // El delta se acota a max_step_mm (80) por componente => x ~ 1080, no 5000.
    TEST_ASSERT_INT16_WITHIN(5, 1080, out.x_mm);
    TEST_ASSERT_TRUE(out.x_mm < 1200);  // anti-teletransporte
}

// ============================================================
// test_heading_siempre_pass_through
// ============================================================
void test_heading_siempre_pass_through(void) {
    // Caso A: estado NO inicializado, sin ToF ni OTOS.
    {
        PoseFusionState st;
        pose_fusion_reset(st);
        PoseFusionInputs in = make_inputs(0, 0, /*tof_valid=*/false,
                                          0, 0, /*otos_fresh=*/false,
                                          /*heading=*/12345, /*dt=*/10);
        PoseFusionOutput out = pose_fusion_update(st, in, cfg);
        TEST_ASSERT_EQUAL_INT16(12345, out.heading_centideg);
    }
    // Caso B: estado inicializado, con ToF y OTOS.
    {
        PoseFusionState st;
        pose_fusion_reset(st);
        seed_at(st, 1000, 800);
        PoseFusionInputs in = make_inputs(1000, 800, /*tof_valid=*/true,
                                          1000, 800, /*otos_fresh=*/true,
                                          /*heading=*/12345, /*dt=*/10);
        PoseFusionOutput out = pose_fusion_update(st, in, cfg);
        TEST_ASSERT_EQUAL_INT16(12345, out.heading_centideg);
    }
}

// ============================================================
// test_clamp_salida_dentro_de_cancha
// ============================================================
void test_clamp_salida_dentro_de_cancha(void) {
    PoseFusionState st;
    pose_fusion_reset(st);
    seed_at(st, 100, 100);

    // Forzar (via deltas OTOS acotados a max_step) que el estado interno se vaya
    // a x negativo e y > field_height. Aplicamos muchos ticks de paso máximo.
    // En X: empujar hacia abajo (negativo). En Y: empujar hacia arriba (>2430).
    int16_t otos_x = 100;
    int16_t otos_y = 100;
    for (int i = 0; i < 60; ++i) {
        otos_x -= 80;  // hacia negativo (clamp_vector L1 reparte, pero 1 eje => ok)
        otos_y += 80;  // hacia arriba
        // Nota: para que cada eje reciba ~80mm sin que el L1 (160 = 2*80) lo escale,
        // mantenemos cada componente en max_step y la suma L1 = 160 = budget exacto.
        PoseFusionInputs in = make_inputs(0, 0, /*tof_valid=*/false,
                                          otos_x, otos_y, /*otos_fresh=*/true,
                                          /*heading=*/0, /*dt=*/10);
        pose_fusion_update(st, in, cfg);
    }
    // El estado interno está fuera de cancha; la salida se clampa.
    TEST_ASSERT_TRUE(st.x_mm_q0 < 0);
    TEST_ASSERT_TRUE(st.y_mm_q0 > 2430);

    // Un último tick para leer la salida clampada limpia.
    PoseFusionInputs in = make_inputs(0, 0, /*tof_valid=*/false,
                                      otos_x, otos_y, /*otos_fresh=*/true,
                                      /*heading=*/0, /*dt=*/10);
    PoseFusionOutput out = pose_fusion_update(st, in, cfg);
    TEST_ASSERT_EQUAL_INT16(0, out.x_mm);     // clamp a [0, width]
    TEST_ASSERT_EQUAL_INT16(2430, out.y_mm);  // clamp a [0, height]
}

// ============================================================
// Runner
// ============================================================
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_seed_con_tof_inicializa);
    RUN_TEST(test_sin_tof_y_sin_otos_no_inicializa);
    RUN_TEST(test_otos_sola_deriva_poco_en_corto);
    RUN_TEST(test_tof_corrige_drift_otos);
    RUN_TEST(test_correccion_es_suave_no_salta);
    RUN_TEST(test_tof_saltada_se_rechaza_por_gating);
    RUN_TEST(test_perdida_temporal_de_tof_no_rompe);
    RUN_TEST(test_otos_no_fresca_mantiene_pose);
    RUN_TEST(test_otos_reset_clamp_no_teletransporta);
    RUN_TEST(test_heading_siempre_pass_through);
    RUN_TEST(test_clamp_salida_dentro_de_cancha);
    return UNITY_END();
}
