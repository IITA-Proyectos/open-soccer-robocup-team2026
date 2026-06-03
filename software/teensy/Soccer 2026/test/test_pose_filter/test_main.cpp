// test_pose_filter — tests host-native del módulo puro de filtro temporal de pose.
//
// Cubre: sembrado, suavizado EMA (alpha 1/4 y 1/2), gate de salto, hold/stale,
// mediana, heading circular en el cruce ±180, y los helpers puros.
#include <unity.h>
#include "pose_filter.h"

using namespace iitasoccer;

void setUp() {}
void tearDown() {}

// ---- helper de armado ----
static FilterPose mk(int16_t x, int16_t y, int16_t h, uint8_t conf) {
    FilterPose p; p.x_mm = x; p.y_mm = y; p.heading_centideg = h; p.confidence = conf;
    return p;
}

// ============================ Helpers puros ============================

void test_jump_sq_helper_sin_overflow() {
    // esquinas opuestas de la cancha: 2430^2 + 1820^2 = 5904900 + 3312400 =
    // 9217300, cabe holgado en int32 (<2.1e9). (El spec citaba 9221700 por un
    // error aritmético; el valor correcto es 9217300.)
    FilterPose a = mk(0, 0, 0, 0);
    FilterPose b = mk(2430, 1820, 0, 0);
    TEST_ASSERT_EQUAL_INT32(9217300, pose_filter_jump_sq_mm2(a, b));
    // misma pose => 0.
    FilterPose c = mk(1215, 910, 0, 0);
    TEST_ASSERT_EQUAL_INT32(0, pose_filter_jump_sq_mm2(c, c));
}

void test_ema_step_helper() {
    // 1000 desde 0 con 1/4 => 250. paso siguiente 250->1000 => 250+187=437.
    TEST_ASSERT_EQUAL_INT16(250, pose_filter_ema_step(0, 1000, 1, 4));
    TEST_ASSERT_EQUAL_INT16(437, pose_filter_ema_step(250, 1000, 1, 4));
    // |new-prev|<den => truncamiento a 0 (no se mueve): 997->1000 con 1/4 => +0.
    TEST_ASSERT_EQUAL_INT16(997, pose_filter_ema_step(997, 1000, 1, 4));
}

void test_circular_mean_centideg_wrap() {
    // promedio circular de 17900 y -17900: el camino corto cruza ±18000, no 0.
    int16_t vals[2] = { 17900, -17900 };
    int16_t m = pose_filter_circular_mean_centideg(vals, 2);
    // resultado debe estar cerca de ±18000, NO cerca de 0.
    int32_t am = m < 0 ? -m : m;
    TEST_ASSERT_TRUE(am > 17000);
}

// ============================ Sembrado ============================

void test_siembra_primer_ciclo_valid() {
    PoseFilterState st; pose_filter_init(st);
    PoseFilterConfig cfg = pose_filter_default_config();
    FilterPose out = pose_filter_update(st, cfg, mk(1215, 910, 0, 70), true);
    // el sembrado NO suaviza: out == measured exacto.
    TEST_ASSERT_EQUAL_INT16(1215, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(910, out.y_mm);
    TEST_ASSERT_EQUAL_INT16(0, out.heading_centideg);
    TEST_ASSERT_EQUAL_UINT8(70, out.confidence);
    TEST_ASSERT_TRUE(st.primed);
    TEST_ASSERT_FALSE(pose_filter_is_stale(st, cfg));
}

void test_primer_ciclo_invalid_no_siembra() {
    PoseFilterState st; pose_filter_init(st);
    PoseFilterConfig cfg = pose_filter_default_config();
    FilterPose out = pose_filter_update(st, cfg, mk(1215, 910, 0, 70), false);
    // no se siembra basura: out queda {0,0,0,0}, primed sigue false.
    TEST_ASSERT_EQUAL_INT16(0, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(0, out.y_mm);
    TEST_ASSERT_EQUAL_INT16(0, out.heading_centideg);
    TEST_ASSERT_FALSE(st.primed);
}

// ============================ Suavizado EMA ============================

void test_ruido_se_suaviza_EMA() {
    PoseFilterState st; pose_filter_init(st);
    PoseFilterConfig cfg = pose_filter_default_config();  // EMA 1/4
    cfg.max_jump_mm = 5000;   // no gatear: queremos aislar el suavizado del ruido
    // sembrar en 1000.
    pose_filter_update(st, cfg, mk(1000, 0, 0, 100), true);
    // crudo oscilando ±30mm alrededor de 1000.
    int16_t crudo[8] = { 970, 1030, 985, 1015, 975, 1025, 990, 1010 };
    for (int i = 0; i < 8; ++i) {
        FilterPose out = pose_filter_update(st, cfg, mk(crudo[i], 0, 0, 100), true);
        // atenuación ~3x: la salida queda dentro de ±10mm de 1000 mientras el
        // crudo oscila ±30mm.
        int32_t err = out.x_mm - 1000;
        if (err < 0) err = -err;
        TEST_ASSERT_TRUE(err <= 10);
    }
    // excursión de un solo paso 1000->1030 = 1007, NO 1030.
    PoseFilterState st2; pose_filter_init(st2);
    pose_filter_update(st2, cfg, mk(1000, 0, 0, 100), true);
    FilterPose o = pose_filter_update(st2, cfg, mk(1030, 0, 0, 100), true);
    TEST_ASSERT_EQUAL_INT16(1007, o.x_mm);
}

void test_converge_tras_N_ciclos_EMA_alpha_1_4() {
    PoseFilterState st; pose_filter_init(st);
    PoseFilterConfig cfg = pose_filter_default_config();  // 1/4
    cfg.max_jump_mm = 5000;   // que el sembrado en (0,0) -> (1000,500) no gatee
    // sembrar implícito con primer valid=(0,0).
    pose_filter_update(st, cfg, mk(0, 0, 0, 100), true);
    // alimentar constante (1000,500).
    FilterPose out = mk(0, 0, 0, 0);
    for (int c = 1; c <= 16; ++c) {
        out = pose_filter_update(st, cfg, mk(1000, 500, 0, 100), true);
        if (c == 12) {
            int32_t err = out.x_mm - 1000; if (err < 0) err = -err;
            TEST_ASSERT_TRUE(err <= 50);   // within 50mm al ciclo 12
        }
    }
    // al ciclo 16, within 15mm (stalla en ~997 por truncamiento, no exige 1000).
    int32_t err = out.x_mm - 1000; if (err < 0) err = -err;
    TEST_ASSERT_TRUE(err <= 15);
}

void test_converge_mas_rapido_alpha_1_2() {
    PoseFilterState st; pose_filter_init(st);
    PoseFilterConfig cfg = pose_filter_default_config();
    cfg.ema_alpha_num = 1; cfg.ema_alpha_den = 2;   // alpha 1/2
    cfg.max_jump_mm = 5000;
    pose_filter_update(st, cfg, mk(0, 0, 0, 100), true);  // siembra en 0
    // trayectoria esperada: 500,750,875,937,968,...
    FilterPose out = mk(0, 0, 0, 0);
    for (int c = 1; c <= 5; ++c) {
        out = pose_filter_update(st, cfg, mk(1000, 0, 0, 100), true);
    }
    int32_t err = out.x_mm - 1000; if (err < 0) err = -err;
    TEST_ASSERT_TRUE(err <= 50);   // within 50mm al ciclo 5 (converge antes que 1/4)
}

// ============================ Gate de salto ============================

void test_outlier_se_rechaza_por_gate() {
    PoseFilterState st; pose_filter_init(st);
    PoseFilterConfig cfg = pose_filter_default_config();
    cfg.max_jump_mm = 400; cfg.warmup_cycles = 2;
    // sembrar y superar warmup en (1215,910).
    pose_filter_update(st, cfg, mk(1215, 910, 0, 90), true);
    pose_filter_update(st, cfg, mk(1215, 910, 0, 90), true);  // valid_seen llega a warmup
    pose_filter_update(st, cfg, mk(1215, 910, 0, 90), true);  // gate ya activo
    // teleport: y salta 690mm. jump_sq = 690^2 = 476100 > 400^2 = 160000.
    FilterPose out = pose_filter_update(st, cfg, mk(1215, 1600, 0, 90), true);
    // RECHAZADO: out NO salta a 1600, se sostiene la última buena.
    TEST_ASSERT_EQUAL_INT16(1215, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(910, out.y_mm);
    TEST_ASSERT_EQUAL_UINT8(2, st.last_reject);   // jump-gate
    TEST_ASSERT_EQUAL_UINT8(1, st.hold_count);
}

void test_movimiento_legitimo_pasa_gate() {
    PoseFilterState st; pose_filter_init(st);
    PoseFilterConfig cfg = pose_filter_default_config();
    cfg.max_jump_mm = 400; cfg.warmup_cycles = 2;
    pose_filter_update(st, cfg, mk(1215, 910, 0, 90), true);
    pose_filter_update(st, cfg, mk(1215, 910, 0, 90), true);
    pose_filter_update(st, cfg, mk(1215, 910, 0, 90), true);  // estable, gate activo
    // desplazamiento 58mm: jump_sq = 50^2+30^2 = 3400 < 160000 => ACEPTADO.
    FilterPose out = pose_filter_update(st, cfg, mk(1265, 940, 0, 90), true);
    // EMA 1/4: x = 1215 + (1265-1215)/4 = 1227.
    TEST_ASSERT_EQUAL_INT16(1227, out.x_mm);
    TEST_ASSERT_EQUAL_UINT8(0, st.last_reject);
    TEST_ASSERT_EQUAL_UINT8(0, st.hold_count);
}

void test_outlier_en_warmup_no_gatea() {
    PoseFilterState st; pose_filter_init(st);
    PoseFilterConfig cfg = pose_filter_default_config();
    cfg.warmup_cycles = 2; cfg.max_jump_mm = 400;
    // ciclo1: siembra en (0,0).
    pose_filter_update(st, cfg, mk(0, 0, 0, 100), true);
    // ciclo2: salto enorme (2000,1500) ~2500mm. valid_seen=1 < warmup => NO gatea.
    FilterPose out = pose_filter_update(st, cfg, mk(2000, 1500, 0, 100), true);
    TEST_ASSERT_EQUAL_UINT8(0, st.last_reject);   // gate deshabilitado en warmup
    // se integró: out se movió hacia (2000,1500) via EMA (no se quedó en 0).
    TEST_ASSERT_TRUE(out.x_mm > 0);
}

// ============================ Hold / stale ============================

void test_hold_y_stale_tras_invalids() {
    PoseFilterState st; pose_filter_init(st);
    PoseFilterConfig cfg = pose_filter_default_config();
    cfg.max_hold_cycles = 5;
    // estabilizar/sembrar en (1215,910,0,80).
    pose_filter_update(st, cfg, mk(1215, 910, 0, 80), true);
    uint8_t prev_conf = 80;
    for (int i = 1; i <= 5; ++i) {
        FilterPose out = pose_filter_update(st, cfg, mk(0, 0, 0, 0), false);
        // out.xy se sostiene en (1215,910).
        TEST_ASSERT_EQUAL_INT16(1215, out.x_mm);
        TEST_ASSERT_EQUAL_INT16(910, out.y_mm);
        // hold_count incrementa 1..5.
        TEST_ASSERT_EQUAL_UINT8((uint8_t)i, st.hold_count);
        // confidence decae monótonamente.
        TEST_ASSERT_TRUE(out.confidence <= prev_conf);
        prev_conf = out.confidence;
        // is_stale: false hasta ciclo 4, true al ciclo 5.
        if (i < 5) TEST_ASSERT_FALSE(pose_filter_is_stale(st, cfg));
        else       TEST_ASSERT_TRUE(pose_filter_is_stale(st, cfg));
    }
    TEST_ASSERT_EQUAL_UINT8(0, prev_conf);   // llegó a 0 al ciclo 5
    // ciclo 6: un valid resetea hold y stale.
    pose_filter_update(st, cfg, mk(1220, 915, 0, 75), true);
    TEST_ASSERT_EQUAL_UINT8(0, st.hold_count);
    TEST_ASSERT_FALSE(pose_filter_is_stale(st, cfg));
}

// ============================ Mediana ============================

void test_mediana_rechaza_spike_aislado() {
    PoseFilterState st; pose_filter_init(st);
    PoseFilterConfig cfg = pose_filter_default_config();
    cfg.smooth_mode = PoseSmooth::MEDIAN;
    cfg.median_window = 3;
    cfg.max_jump_mm = 5000;   // gate grande para AISLAR el efecto de la mediana
    // sembrar en 1000.
    pose_filter_update(st, cfg, mk(1000, 0, 0, 100), true);
    // 1005 -> ventana [1000,1005].
    pose_filter_update(st, cfg, mk(1005, 0, 0, 100), true);
    // spike 1600 -> ventana [1000,1005,1600] => median == 1005 (spike ignorado).
    FilterPose o1 = pose_filter_update(st, cfg, mk(1600, 0, 0, 100), true);
    TEST_ASSERT_EQUAL_INT16(1005, o1.x_mm);
    // siguiente lectura 1002 -> ventana [1005,1600,1002] => median == 1005.
    FilterPose o2 = pose_filter_update(st, cfg, mk(1002, 0, 0, 100), true);
    TEST_ASSERT_EQUAL_INT16(1005, o2.x_mm);
}

// ============================ Heading circular ============================

void test_heading_wrap_circular() {
    PoseFilterState st; pose_filter_init(st);
    PoseFilterConfig cfg = pose_filter_default_config();  // EMA
    cfg.max_jump_mm = 5000;
    // sembrar heading=17900 (~179°).
    pose_filter_update(st, cfg, mk(0, 0, 17900, 100), true);
    // measured heading=-17900 (~-179°): giro real de solo ~2° cruzando ±180.
    FilterPose out = pose_filter_update(st, cfg, mk(0, 0, -17900, 100), true);
    // la salida NO debe saltar ~358°: queda cerca de ±18000 del lado correcto,
    // NO cerca de 0. delta corto = +200 centideg, EMA 1/4 => out ~ 17950.
    TEST_ASSERT_TRUE(out.heading_centideg > 17000);
    // y NO cae cerca de 0 (lo que daría un EMA lineal ingenuo).
    int32_t ah = out.heading_centideg < 0 ? -out.heading_centideg : out.heading_centideg;
    TEST_ASSERT_TRUE(ah > 17000);
}

// ============================ main ============================

int main() {
    UNITY_BEGIN();
    // helpers
    RUN_TEST(test_jump_sq_helper_sin_overflow);
    RUN_TEST(test_ema_step_helper);
    RUN_TEST(test_circular_mean_centideg_wrap);
    // sembrado
    RUN_TEST(test_siembra_primer_ciclo_valid);
    RUN_TEST(test_primer_ciclo_invalid_no_siembra);
    // suavizado EMA
    RUN_TEST(test_ruido_se_suaviza_EMA);
    RUN_TEST(test_converge_tras_N_ciclos_EMA_alpha_1_4);
    RUN_TEST(test_converge_mas_rapido_alpha_1_2);
    // gate de salto
    RUN_TEST(test_outlier_se_rechaza_por_gate);
    RUN_TEST(test_movimiento_legitimo_pasa_gate);
    RUN_TEST(test_outlier_en_warmup_no_gatea);
    // hold / stale
    RUN_TEST(test_hold_y_stale_tras_invalids);
    // mediana
    RUN_TEST(test_mediana_rechaza_spike_aislado);
    // heading circular
    RUN_TEST(test_heading_wrap_circular);
    return UNITY_END();
}
