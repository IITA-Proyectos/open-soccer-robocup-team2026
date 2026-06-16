// test_down_early_evidence — F3 (§4.2/4.3 del diseño RT de DOWN): el glue GATEADO
// de la detección temprana por VECINO FÍSICO dentro de dm_update.
//
// Este archivo corre en DOS envs y verifica DOS cosas con el MISMO input:
//   - pio test -e test_native            (sin -DDOWN_EARLY_EVIDENCE) → rama #else:
//        verifica la BYTE-IDENTIDAD de competencia (el caso temprano NO dispara,
//        line_present=0 igual que hoy).
//   - pio test -e test_native_earlyev    (con -DDOWN_EARLY_EVIDENCE) → rama #ifdef:
//        verifica que la unión por vecino FÍSICO detecta ANTES (line_present=1).
//
// El caso clave: dos sensores que están FÍSICAMENTE adyacentes pero AISLADOS por
// ÍNDICE. idx 5 y idx 7 (fila frontal externa) están a ~18 mm — line_neighbors.h
// los considera vecinos físicos (idx 7 tiene a 5 en su LUT) — pero NO son contiguos
// de índice (idx 6 está en el medio y lee carpet). El filtro espacial de ÍNDICE
// (lf_spatial_filter) descarta ambos como "blanco aislado"; la unión de F3 los
// valida como par. Es el modo de falla del arquero sobre la línea de fondo.

#include <unity.h>
#include "down_model.h"
#include "sensor_geometry.h"   // SENSOR_POS / SENSOR_COUNT — geometría real del PCB
#include <cmath>
using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

static void mkcfg(DownModelCfg& c){
    c.imminent_depth=6; c.adapt_alpha=0.02f; c.calib_min_margin=120;
    c.lifted_debounce_ms=100; c.lifted_min_sensors=7; c.lifted_delta_below=80;
    c.line_end_min_track_ms=200;
}

// raw[32] con `white_idx` en blanco fuerte (800) y el resto en carpet (~195-201,
// con variación por sensor para no disparar el mux-watchdog). Idéntico patrón a
// test_down_model::mk_raw32 (consistencia entre tests).
static void mk_raw32(uint16_t* raw, const int* white_idx, int n_white){
    for(int i=0;i<SENSOR_COUNT;++i) raw[i] = (uint16_t)(195 + (i % 7));
    for(int k=0;k<n_white;++k) raw[white_idx[k]] = 800;
}

static DownModel mk_model32(DownModelCfg& cfg){
    DownModel m{}; mkcfg(cfg);
    for(int i=0;i<SENSOR_COUNT;++i) lc_set_static(m.calib[i],200,800); // thr=500
    return m;
}

// ============================================================================
// CASO CLAVE — par físicamente adyacente pero aislado por índice (idx 5 y 7).
// ============================================================================
void test_physical_pair_index_isolated(void){
    DownModelCfg cfg; DownModel m = mk_model32(cfg);
    const int white[] = {5, 7};   // ~18mm en el PCB; idx 6 (entre medio) = carpet
    uint16_t raw[SENSOR_COUNT]; mk_raw32(raw, white, 2);
    LineStatusV2 s = dm_update(m,cfg,raw,SENSOR_COUNT,1000);

#ifdef DOWN_EARLY_EVIDENCE
    // Gate ON: la unión por vecino FÍSICO valida el par → detección TEMPRANA.
    TEST_ASSERT_EQUAL_UINT8(1, s.line_present);
    TEST_ASSERT_EQUAL_UINT8(2, s.sensors_on_line);
    TEST_ASSERT_EQUAL_UINT8(1, s.data_valid);
    // El vector de escape EXISTE en el rango temprano (hoy estaría en N/A).
    TEST_ASSERT_NOT_EQUAL(LSV2_NA_I16, s.escape_angle_centideg);
    // 2 < imminent_depth(6): es EVIDENCIA temprana, NO el freno duro todavía.
    TEST_ASSERT_FALSE(s.event_flags & EV_IMMINENT_EXIT);
#else
    // Gate OFF (competencia): el filtro de índice descarta ambos → byte-idéntico
    // al binario de hoy (sin detección temprana).
    TEST_ASSERT_EQUAL_UINT8(0, s.line_present);
    TEST_ASSERT_EQUAL_UINT8(0, s.sensors_on_line);
    TEST_ASSERT_EQUAL_INT16(LSV2_NA_I16, s.escape_angle_centideg);
#endif
}

// ============================================================================
// CONTROL 1 — un único blanco SIN par físico NO dispara (ni con el gate ON).
// Prueba que F3 no inventa evidencia con un sensor loco aislado.
// ============================================================================
void test_single_isolated_white_never_fires(void){
    DownModelCfg cfg; DownModel m = mk_model32(cfg);
    const int white[] = {9};   // idx 9: sus vecinos físicos (8,10) leen carpet
    uint16_t raw[SENSOR_COUNT]; mk_raw32(raw, white, 1);
    LineStatusV2 s = dm_update(m,cfg,raw,SENSOR_COUNT,1000);
    // En AMBOS envs: sin par (de índice o físico) no hay línea.
    TEST_ASSERT_EQUAL_UINT8(0, s.line_present);
    TEST_ASSERT_EQUAL_UINT8(0, s.sensors_on_line);
}

// ============================================================================
// CONTROL 2 — un cluster contiguo normal se detecta igual (la unión es ADITIVA,
// no rompe la detección histórica). Flag-independiente.
// ============================================================================
void test_contiguous_cluster_unaffected(void){
    DownModelCfg cfg; DownModel m = mk_model32(cfg);
    const int white[] = {0,1,2,3};   // contiguos de índice Y físicos
    uint16_t raw[SENSOR_COUNT]; mk_raw32(raw, white, 4);
    LineStatusV2 s = dm_update(m,cfg,raw,SENSOR_COUNT,1000);
    // El filtro de índice ya los valida; la unión no agrega nada → mismo resultado
    // con o sin el flag.
    TEST_ASSERT_EQUAL_UINT8(1, s.line_present);
    TEST_ASSERT_EQUAL_UINT8(4, s.sensors_on_line);
    TEST_ASSERT_EQUAL_UINT8(1, s.data_valid);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_physical_pair_index_isolated);
    RUN_TEST(test_single_isolated_white_never_fires);
    RUN_TEST(test_contiguous_cluster_unaffected);
    return UNITY_END();
}
