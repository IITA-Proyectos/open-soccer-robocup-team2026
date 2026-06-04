// test_down_model — pio test -e test_native -f test_down_model
#include <unity.h>
#include "down_model.h"
#include "sensor_geometry.h"   // SENSOR_POS / SENSOR_COUNT — geometría real (WP-3-DOWN)
#include <cmath>
using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

static void mkcfg(DownModelCfg& c){
    c.imminent_depth=6; c.adapt_alpha=0.02f; c.calib_min_margin=120;
    c.lifted_debounce_ms=100; c.lifted_min_sensors=7; c.lifted_delta_below=80;
    c.line_end_min_track_ms=200;
}

void test_no_line_carpet_valid(void){
    DownModel m{}; DownModelCfg cfg; mkcfg(cfg);
    for(int i=0;i<8;++i) lc_set_static(m.calib[i],200,800);
    uint16_t raw[8]={205,198,202,201,199,203,200,204};
    LineStatusV2 s = dm_update(m,cfg,raw,8,1000);
    TEST_ASSERT_EQUAL_UINT8(2, s.schema_version);
    TEST_ASSERT_EQUAL_UINT8(1, s.data_valid);
    TEST_ASSERT_EQUAL_UINT8(0, s.line_present);
    TEST_ASSERT_EQUAL_INT16(LSV2_NA_I16, s.line_angle_centideg);
    TEST_ASSERT_EQUAL_UINT8(0, s.sensors_on_line);
    // Sin línea: penetration y cross_track son N/A (WP-3-DOWN).
    TEST_ASSERT_EQUAL_UINT16(LSV2_NA_U16, s.penetration_mm);
    TEST_ASSERT_EQUAL_INT16(LSV2_NA_I16, s.cross_track_mm);
}

void test_line_front_sets_fields(void){
    DownModel m{}; DownModelCfg cfg; mkcfg(cfg);
    for(int i=0;i<8;++i) lc_set_static(m.calib[i],200,800);
    uint16_t raw[8]={850,830,210,200,205,202,198,206};
    LineStatusV2 s = dm_update(m,cfg,raw,8,1000);
    TEST_ASSERT_EQUAL_UINT8(1, s.line_present);
    TEST_ASSERT_TRUE(s.sensors_on_line >= 1);
    TEST_ASSERT_NOT_EQUAL(LSV2_NA_I16, s.line_angle_centideg);
}

void test_lifted_sets_invalid_and_flag(void){
    DownModel m{}; DownModelCfg cfg; mkcfg(cfg);
    for(int i=0;i<8;++i) lc_set_static(m.calib[i],200,800);
    uint16_t raw[8]={5,5,5,5,5,5,5,5};
    dm_update(m,cfg,raw,8,0);
    LineStatusV2 s = dm_update(m,cfg,raw,8,200);
    TEST_ASSERT_EQUAL_UINT8(0, s.data_valid);
    TEST_ASSERT_TRUE(s.event_flags & EV_LIFTED);
}

void test_calib_suspect_sets_invalid_and_flag(void){
    DownModel m{}; DownModelCfg cfg; mkcfg(cfg);
    // margen blanco-carpet = 50 < calib_min_margin=120 => suspect
    for(int i=0;i<8;++i) lc_set_static(m.calib[i],500,550);
    uint16_t raw[8]={505,508,502,501,499,503,506,504};
    LineStatusV2 s = dm_update(m,cfg,raw,8,1000);
    TEST_ASSERT_EQUAL_UINT8(0, s.data_valid);
    TEST_ASSERT_TRUE(s.event_flags & EV_CALIB_SUSPECT);
}

void test_n_over_max_is_clamped(void){
    DownModel m{}; DownModelCfg cfg; mkcfg(cfg);
    for(int i=0;i<DM_MAX_SENSORS;++i) lc_set_static(m.calib[i],200,800);
    uint16_t raw[DM_MAX_SENSORS]; for(int i=0;i<DM_MAX_SENSORS;++i) raw[i]=200;
    LineStatusV2 s = dm_update(m,cfg,raw,64,1000);   // n=64 > 32 => clamp
    TEST_ASSERT_TRUE(s.sensors_on_line <= DM_MAX_SENSORS);
}

// TEMA P1.5 — saturación "todo blanco" (audit 2026-05-29).
// Si CASI TODO el anillo lee blanco no es línea real: se invalida el packet.
void test_all_white_saturation_rejected(void){
    DownModel m{}; DownModelCfg cfg; mkcfg(cfg);
    for(int i=0;i<8;++i) lc_set_static(m.calib[i],200,800);  // threshold=500
    // Los 8 sensores muy por encima de threshold+band (520) → saturado.
    uint16_t raw[8]={820,830,810,840,825,815,835,820};
    LineStatusV2 s = dm_update(m,cfg,raw,8,1000);
    TEST_ASSERT_EQUAL_UINT8(0, s.line_present);            // no es línea
    TEST_ASSERT_EQUAL_UINT8(0, s.data_valid);              // packet inválido
    TEST_ASSERT_EQUAL_UINT8(0, s.sensors_on_line);
    TEST_ASSERT_TRUE(s.event_flags & EV_CALIB_SUSPECT);    // saturación reusa este flag
    TEST_ASSERT_EQUAL_INT16(LSV2_NA_I16, s.line_angle_centideg);
}

// Caso límite opuesto: una línea fuerte (6/8) NO debe confundirse con saturación.
void test_strong_line_not_saturation(void){
    DownModel m{}; DownModelCfg cfg; mkcfg(cfg);
    for(int i=0;i<8;++i) lc_set_static(m.calib[i],200,800);
    // 6 blancos + 2 carpet: umbral saturación 7/8=7 NO se alcanza.
    uint16_t raw[8]={820,830,810,840,825,815,205,200};
    LineStatusV2 s = dm_update(m,cfg,raw,8,1000);
    TEST_ASSERT_EQUAL_UINT8(1, s.line_present);
    TEST_ASSERT_EQUAL_UINT8(1, s.data_valid);              // sigue válido
    TEST_ASSERT_TRUE(s.sensors_on_line >= 1);
    TEST_ASSERT_TRUE(s.event_flags & EV_IMMINENT_EXIT);    // 6 >= imminent_depth=6
}

// ============================================================================
// WP-3-DOWN (Capa 3): cross_track_mm y penetration_mm REALES con geometría.
//
// El cómputo geométrico real sólo corre cuando n == SENSOR_COUNT (32), porque
// SENSOR_POS[] mapea 1:1 con los 32 sensores físicos. Con n<32 (anillo parcial)
// las posiciones no corresponden y ambos campos quedan en N/A.
//
// Convención de signo fijada por WP-3-DOWN (coincide con el seguidor de arquero
// diag_central_line_sweep.cpp):
//   cross_track_mm = centroide Y (mm) de los sensores en blanco.
//     +  => la línea está ADELANTE del centro del robot (+Y).
//     -  => la línea está ATRÁS (-Y).
//   Es la distancia perpendicular firmada del centro (0,0) a la recta de la
//   línea cuando la línea corre lateral (caso del arquero pisando la línea de
//   fondo). |cy| es la proyección del centroide sobre el eje delantero/trasero.
//
//   penetration_mm = R_OUTER - min(radio de los sensores en blanco), clamp >=0.
//     0  => recién tocando (sólo el anillo externo ve blanco).
//     crece a medida que sensores más internos ven blanco (robot más adentro).
//
// Valores esperados calculados con la LUT real de sensor_geometry.cpp.
// R_OUTER (radio máximo del anillo) ≈ 89.79 mm (S8 idx 7).
// ============================================================================

// Helper: arma un raw[32] con `white_idx` en blanco (800) y el resto en carpet
// (~200, con pequeña variación por sensor para no disparar mux-dead).
static void mk_raw32(uint16_t* raw, const int* white_idx, int n_white){
    for(int i=0;i<SENSOR_COUNT;++i) raw[i] = (uint16_t)(195 + (i % 7));  // carpet variado
    for(int k=0;k<n_white;++k) raw[white_idx[k]] = 800;                  // blanco fuerte
}

static DownModel mk_model32(DownModelCfg& cfg){
    DownModel m{}; mkcfg(cfg);
    for(int i=0;i<SENSOR_COUNT;++i) lc_set_static(m.calib[i],200,800); // thr=500
    return m;
}

// Línea al FRENTE (fila externa frontal completa, idx 0..7). centroide ≈ (0,+76).
// cross_track ≈ +76 mm (línea adelante). penetration ≈ R_OUTER - 74.86 ≈ 15 mm.
void test_cross_track_front_positive(void){
    DownModelCfg cfg; DownModel m = mk_model32(cfg);
    const int white[] = {0,1,2,3,4,5,6,7};
    uint16_t raw[SENSOR_COUNT]; mk_raw32(raw, white, 8);
    LineStatusV2 s = dm_update(m,cfg,raw,SENSOR_COUNT,1000);
    TEST_ASSERT_EQUAL_UINT8(1, s.line_present);
    TEST_ASSERT_EQUAL_UINT8(1, s.data_valid);
    TEST_ASSERT_NOT_EQUAL(LSV2_NA_I16, s.cross_track_mm);
    TEST_ASSERT_TRUE(s.cross_track_mm > 0);                 // línea ADELANTE
    TEST_ASSERT_INT16_WITHIN(6, 76, s.cross_track_mm);      // ≈ +76 mm
}

// Línea ATRÁS (fila trasera, idx 13..18). centroide ≈ (0,-69).
// cross_track ≈ -69 mm (línea atrás del centro).
void test_cross_track_rear_negative(void){
    DownModelCfg cfg; DownModel m = mk_model32(cfg);
    const int white[] = {13,14,15,16,17,18};
    uint16_t raw[SENSOR_COUNT]; mk_raw32(raw, white, 6);
    LineStatusV2 s = dm_update(m,cfg,raw,SENSOR_COUNT,1000);
    TEST_ASSERT_EQUAL_UINT8(1, s.line_present);
    TEST_ASSERT_NOT_EQUAL(LSV2_NA_I16, s.cross_track_mm);
    TEST_ASSERT_TRUE(s.cross_track_mm < 0);                 // línea ATRÁS
    TEST_ASSERT_INT16_WITHIN(8, -69, s.cross_track_mm);     // ≈ -69 mm
}

// Línea LATERAL derecha (idx 20..23, x≈+85, y≈-7). El centroide está casi sobre
// el eje X: cross_track (proyección Y) ≈ -7 mm (cerca de 0, ligeramente atrás).
// Verifica que una línea lateral da cross_track chico (el arquero se centra Y≈0).
void test_cross_track_side_near_zero(void){
    DownModelCfg cfg; DownModel m = mk_model32(cfg);
    const int white[] = {20,21,22,23};
    uint16_t raw[SENSOR_COUNT]; mk_raw32(raw, white, 4);
    LineStatusV2 s = dm_update(m,cfg,raw,SENSOR_COUNT,1000);
    TEST_ASSERT_EQUAL_UINT8(1, s.line_present);
    TEST_ASSERT_NOT_EQUAL(LSV2_NA_I16, s.cross_track_mm);
    TEST_ASSERT_INT16_WITHIN(10, -7, s.cross_track_mm);     // ≈ -7 mm (chico)
}

// Penetración REAL: línea sólo en el anillo externo frontal (idx 0..7) →
// recién tocando → penetration pequeña (≈ 15 mm). Y NO es un conteo: con 8
// sensores en blanco el viejo proxy habría dado 8.
void test_penetration_shallow_outer_only(void){
    DownModelCfg cfg; DownModel m = mk_model32(cfg);
    const int white[] = {0,1,2,3,4,5,6,7};
    uint16_t raw[SENSOR_COUNT]; mk_raw32(raw, white, 8);
    LineStatusV2 s = dm_update(m,cfg,raw,SENSOR_COUNT,1000);
    TEST_ASSERT_EQUAL_UINT8(1, s.line_present);
    TEST_ASSERT_NOT_EQUAL(LSV2_NA_U16, s.penetration_mm);
    TEST_ASSERT_UINT16_WITHIN(6, 15, s.penetration_mm);     // ≈ 15 mm, no "8"
    TEST_ASSERT_NOT_EQUAL((uint16_t)s.sensors_on_line, s.penetration_mm); // NO es conteo
}

// Penetración REAL profunda: blanco llega hasta el anillo interno central
// (idx 28..31, R≈35 mm) → robot muy adentro → penetration grande (≈ 55 mm) y
// claramente mayor que el caso "recién tocando".
void test_penetration_deep_through_center(void){
    DownModelCfg cfg; DownModel m = mk_model32(cfg);
    const int white[] = {0,1,2,3,4,5,6,7, 24,25,26,27, 28,29,30,31};
    uint16_t raw[SENSOR_COUNT]; mk_raw32(raw, white, 16);
    LineStatusV2 s = dm_update(m,cfg,raw,SENSOR_COUNT,1000);
    TEST_ASSERT_EQUAL_UINT8(1, s.line_present);
    TEST_ASSERT_NOT_EQUAL(LSV2_NA_U16, s.penetration_mm);
    TEST_ASSERT_UINT16_WITHIN(8, 55, s.penetration_mm);     // ≈ 55 mm
    TEST_ASSERT_TRUE(s.penetration_mm > 30);                // mucho más que "tocando"
}

// Geometría parcial (n<32): SENSOR_POS no mapea → cross_track/penetration N/A,
// pero la línea y el ángulo siguen reportándose.
void test_partial_ring_fields_na(void){
    DownModel m{}; DownModelCfg cfg; mkcfg(cfg);
    for(int i=0;i<8;++i) lc_set_static(m.calib[i],200,800);
    uint16_t raw[8]={850,830,210,200,205,202,198,206};
    LineStatusV2 s = dm_update(m,cfg,raw,8,1000);
    TEST_ASSERT_EQUAL_UINT8(1, s.line_present);
    TEST_ASSERT_EQUAL_UINT16(LSV2_NA_U16, s.penetration_mm);
    TEST_ASSERT_EQUAL_INT16(LSV2_NA_I16, s.cross_track_mm);
}

// ============================================================================
// TEMA #4 (audit 2026-06-03): el gate anti-deriva de lc_adapt_carpet usa white[]
// (pre-espacial), NO validated[]. Un sensor que ve blanco REAL pero sin vecino-
// de-índice blanco (array-singleton) NO debe contaminar su baseline de carpet.
// ============================================================================

// cfg con alpha agresivo para que el drift (si existiera) sea nítido en pocos ticks.
static void mkcfg_fast_adapt(DownModelCfg& c){ mkcfg(c); c.adapt_alpha=0.2f; }

// idx24 (anillo interno, S25) ve blanco 800 como ARRAY-SINGLETON: sus vecinos
// de índice 23 y 25 leen carpet, así que validated[24]=false. Con el bug viejo
// (gate=validated[]) lc_adapt_carpet lo trataría como off-line y arrastraría su
// carpet hacia 800 → threshold trepa → sensor ciego. Con el fix (gate=white[])
// queda gated OUT de la adaptación y su carpet se mantiene ~200.
void test_array_singleton_white_does_not_drift_carpet(void){
    DownModelCfg cfg; DownModel m = mk_model32(cfg);
    cfg.adapt_alpha = 0.2f;   // acelerar el drift potencial
    const int white[] = {24};
    uint16_t raw[SENSOR_COUNT]; mk_raw32(raw, white, 1);
    for(uint32_t t=0;t<300;++t) dm_update(m,cfg,raw,SENSOR_COUNT, t*5);  // ~200Hz
    // El carpet de idx24 NO debe haber trepado: gated out por white[].
    TEST_ASSERT_UINT16_WITHIN(15, 200, m.calib[24].carpet);
    TEST_ASSERT_UINT16_WITHIN(15, 500, m.calib[24].threshold);
}

// Control: un cluster blanco contiguo (idx0/1/2) — idx1 SÍ valida (vecinos 0 y 2
// blancos). El sensor que ve blanco igual queda gated OUT de la adaptación (con
// el fix lo gobierna white[1]=true, no validated[1]), así que su carpet también
// se mantiene. Confirma que el fix no rompe el caso "línea real bien formada".
void test_clustered_white_does_not_drift_carpet(void){
    DownModelCfg cfg; DownModel m = mk_model32(cfg);
    cfg.adapt_alpha = 0.2f;
    const int white[] = {0,1,2};
    uint16_t raw[SENSOR_COUNT]; mk_raw32(raw, white, 3);
    for(uint32_t t=0;t<300;++t) dm_update(m,cfg,raw,SENSOR_COUNT, t*5);
    TEST_ASSERT_UINT16_WITHIN(15, 200, m.calib[1].carpet);   // sigue carpet, no drift
    TEST_ASSERT_UINT16_WITHIN(15, 500, m.calib[1].threshold);
}

// Un sensor que lee CARPET (no blanco) sí debe seguir adaptando su baseline
// hacia la lectura (comportamiento normal anti-deriva, no roto por el fix).
void test_carpet_sensor_still_adapts(void){
    DownModelCfg cfg; DownModel m = mk_model32(cfg);
    cfg.adapt_alpha = 0.3f;
    // calib arranca carpet=200; alimentamos un carpet real más alto (260) en un
    // sensor que NUNCA ve blanco → su carpet debe converger hacia ~260.
    uint16_t raw[SENSOR_COUNT]; const int none[1]={0}; mk_raw32(raw, none, 0);
    raw[5] = 260;   // idx5 lee carpet elevado, nunca cruza threshold (500)
    for(uint32_t t=0;t<300;++t){ raw[5]=260; dm_update(m,cfg,raw,SENSOR_COUNT, t*5); }
    TEST_ASSERT_UINT16_WITHIN(12, 260, m.calib[5].carpet);   // adaptó hacia el carpet real
}

// ============================================================================
// TEMA #16 (audit 2026-06-03): corner detection usa geometría REAL cuando n==32.
// ============================================================================

// Dos clusters perpendiculares en geometría real: frontal-izq (idx0-2, ~ -20°) y
// lateral-der/atrás (idx20-22, ~ +99°). Separación ~119° ∈ [55,125] → corner.
void test_corner_xy_real_geometry(void){
    DownModelCfg cfg; DownModel m = mk_model32(cfg);
    const int white[] = {0,1,2, 20,21,22};
    uint16_t raw[SENSOR_COUNT]; mk_raw32(raw, white, 6);
    LineStatusV2 s = dm_update(m,cfg,raw,SENSOR_COUNT,1000);
    TEST_ASSERT_EQUAL_UINT8(1, s.line_present);
    TEST_ASSERT_TRUE(s.event_flags & EV_CORNER);
    // line_angle sigue viniendo de lg_compute_xy (no N/A).
    TEST_ASSERT_NOT_EQUAL(LSV2_NA_I16, s.line_angle_centideg);
}

// Una franja recta (solo frontales idx0-7) NO es esquina: un solo cluster.
void test_no_corner_straight_strip_real_geometry(void){
    DownModelCfg cfg; DownModel m = mk_model32(cfg);
    const int white[] = {0,1,2,3,4,5,6,7};
    uint16_t raw[SENSOR_COUNT]; mk_raw32(raw, white, 8);
    LineStatusV2 s = dm_update(m,cfg,raw,SENSOR_COUNT,1000);
    TEST_ASSERT_EQUAL_UINT8(1, s.line_present);
    TEST_ASSERT_FALSE(s.event_flags & EV_CORNER);
}

// ============================================================================
// TEMA #21 (audit 2026-06-03): penetration robusto a 1-2 sensores internos
// espurios (usa el K-ésimo radio más chico, no el mínimo crudo).
// ============================================================================

// Línea REAL en el anillo externo frontal (idx0-7) + UN PAR de internos
// adyacentes-por-índice espurios (idx30,31, R≈35-39mm). Con el min crudo viejo
// penetration saltaría a ~55mm; con el estimador robusto (K=3) se mantiene cerca
// del grueso externo (~15mm) porque solo 2 sensores están adentro.
void test_penetration_robust_to_inner_outlier(void){
    DownModelCfg cfg; DownModel m = mk_model32(cfg);
    const int white[] = {0,1,2,3,4,5,6,7, 30,31};
    uint16_t raw[SENSOR_COUNT]; mk_raw32(raw, white, 10);
    LineStatusV2 s = dm_update(m,cfg,raw,SENSOR_COUNT,1000);
    TEST_ASSERT_EQUAL_UINT8(1, s.line_present);
    TEST_ASSERT_NOT_EQUAL(LSV2_NA_U16, s.penetration_mm);
    TEST_ASSERT_UINT16_WITHIN(8, 15, s.penetration_mm);     // ~15mm, NO ~55mm
}

// Penetración genuina profunda (idx0-7 + 24-27 + 28-31): el cluster interno es
// REAL (>=3 sensores adentro) → el estimador robusto NO lo penaliza, sigue ~55mm.
void test_penetration_deep_genuine_not_penalized(void){
    DownModelCfg cfg; DownModel m = mk_model32(cfg);
    const int white[] = {0,1,2,3,4,5,6,7, 24,25,26,27, 28,29,30,31};
    uint16_t raw[SENSOR_COUNT]; mk_raw32(raw, white, 16);
    LineStatusV2 s = dm_update(m,cfg,raw,SENSOR_COUNT,1000);
    TEST_ASSERT_EQUAL_UINT8(1, s.line_present);
    TEST_ASSERT_NOT_EQUAL(LSV2_NA_U16, s.penetration_mm);
    TEST_ASSERT_UINT16_WITHIN(8, 55, s.penetration_mm);     // sigue profundo
    TEST_ASSERT_TRUE(s.penetration_mm > 30);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_no_line_carpet_valid);
    RUN_TEST(test_line_front_sets_fields);
    RUN_TEST(test_lifted_sets_invalid_and_flag);
    RUN_TEST(test_calib_suspect_sets_invalid_and_flag);
    RUN_TEST(test_n_over_max_is_clamped);
    RUN_TEST(test_all_white_saturation_rejected);
    RUN_TEST(test_strong_line_not_saturation);
    // WP-3-DOWN — cross_track / penetration reales
    RUN_TEST(test_cross_track_front_positive);
    RUN_TEST(test_cross_track_rear_negative);
    RUN_TEST(test_cross_track_side_near_zero);
    RUN_TEST(test_penetration_shallow_outer_only);
    RUN_TEST(test_penetration_deep_through_center);
    RUN_TEST(test_partial_ring_fields_na);
    // TEMA #4 — gate anti-deriva por white[] (no validated[])
    RUN_TEST(test_array_singleton_white_does_not_drift_carpet);
    RUN_TEST(test_clustered_white_does_not_drift_carpet);
    RUN_TEST(test_carpet_sensor_still_adapts);
    // TEMA #16 — corner con geometría real
    RUN_TEST(test_corner_xy_real_geometry);
    RUN_TEST(test_no_corner_straight_strip_real_geometry);
    // TEMA #21 — penetration robusto a outliers internos
    RUN_TEST(test_penetration_robust_to_inner_outlier);
    RUN_TEST(test_penetration_deep_genuine_not_penalized);
    return UNITY_END();
}
