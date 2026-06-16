// test_adc_scan_plan — el PLAN de barrido dual-ADC del anillo de línea (§3.2 DOWN-RT).
// Corre host: bash scripts/run-host-tests.sh test_adc_scan_plan
//
// Qué verifica: que adc_plan_build() empareje los muxes de a dos para lecturas
// sincronizadas ADC1+ADC2 (uno por ADC), conserve el orden del barrido, deje el impar
// SOLO sin inventar compañero, y degrade coherente a 1/0 muxes. Es lógica PURA — no toca
// hardware; el glue de line_ring.cpp consume este plan en otra fase gateada.

#include <unity.h>
#include "adc_scan_plan.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Ningún mux de un par se repite en otro par, ni un par se repite consigo mismo.
// Helper de aserción para los tests con pares.
static void assert_pairs_sin_repetir(const AdcScanPlan& p) {
    bool visto[ADC_SCAN_MAX_MUXES] = {false};
    for (int k = 0; k < p.n_pairs; ++k) {
        const uint8_t a = p.pairs[k][0];
        const uint8_t b = p.pairs[k][1];
        TEST_ASSERT_NOT_EQUAL(a, b);                 // un par no se aparea consigo mismo
        TEST_ASSERT_TRUE(a < ADC_SCAN_MAX_MUXES);    // índices dentro de rango físico
        TEST_ASSERT_TRUE(b < ADC_SCAN_MAX_MUXES);
        TEST_ASSERT_FALSE(visto[a]);                 // sin repetir mux entre pares
        TEST_ASSERT_FALSE(visto[b]);
        visto[a] = true;
        visto[b] = true;
    }
}

// =====================================================================
// PRODUCCIÓN: 4 muxes → 2 pares válidos sin repetir, sin mux solo.
// =====================================================================
void test_cuatro_muxes_dos_pares(void) {
    AdcScanPlan p = adc_plan_build(4);
    TEST_ASSERT_EQUAL_INT(2, p.n_pairs);
    TEST_ASSERT_FALSE(p.has_solo);
    TEST_ASSERT_EQUAL_INT(4, p.n_muxes);
    assert_pairs_sin_repetir(p);
}

// El apareo consecutivo es {0,1} y {2,3} — conserva el orden lógico del barrido histórico.
// idxA va por ADC1, idxB por ADC2 (convención fija que el glue respeta al cosechar).
void test_cuatro_muxes_apareo_consecutivo(void) {
    AdcScanPlan p = adc_plan_build(4);
    TEST_ASSERT_EQUAL_UINT8(0, p.pairs[0][0]);
    TEST_ASSERT_EQUAL_UINT8(1, p.pairs[0][1]);
    TEST_ASSERT_EQUAL_UINT8(2, p.pairs[1][0]);
    TEST_ASSERT_EQUAL_UINT8(3, p.pairs[1][1]);
}

// Los 4 muxes quedan cubiertos exactamente una vez entre los 2 pares.
void test_cuatro_muxes_cobertura_completa(void) {
    AdcScanPlan p = adc_plan_build(4);
    int cubiertos = 2 * p.n_pairs + (p.has_solo ? 1 : 0);
    TEST_ASSERT_EQUAL_INT(4, cubiertos);
    TEST_ASSERT_EQUAL_INT(p.n_muxes, cubiertos);
}

// =====================================================================
// 2 muxes → 1 par, sin solo (degrada coherente).
// =====================================================================
void test_dos_muxes_un_par(void) {
    AdcScanPlan p = adc_plan_build(2);
    TEST_ASSERT_EQUAL_INT(1, p.n_pairs);
    TEST_ASSERT_FALSE(p.has_solo);
    TEST_ASSERT_EQUAL_INT(2, p.n_muxes);
    TEST_ASSERT_EQUAL_UINT8(0, p.pairs[0][0]);
    TEST_ASSERT_EQUAL_UINT8(1, p.pairs[0][1]);
    assert_pairs_sin_repetir(p);
}

// =====================================================================
// 3 muxes (impar) → 1 par {0,1} + 1 solo {2}. No se inventa compañero.
// =====================================================================
void test_tres_muxes_par_mas_solo(void) {
    AdcScanPlan p = adc_plan_build(3);
    TEST_ASSERT_EQUAL_INT(1, p.n_pairs);
    TEST_ASSERT_TRUE(p.has_solo);
    TEST_ASSERT_EQUAL_UINT8(2, p.solo_mux);          // el impar sobrante es el idx 2
    TEST_ASSERT_EQUAL_INT(3, p.n_muxes);
    TEST_ASSERT_EQUAL_UINT8(0, p.pairs[0][0]);
    TEST_ASSERT_EQUAL_UINT8(1, p.pairs[0][1]);
    // cobertura: 2*1 + 1 = 3
    TEST_ASSERT_EQUAL_INT(3, 2 * p.n_pairs + (p.has_solo ? 1 : 0));
    assert_pairs_sin_repetir(p);
}

// =====================================================================
// FALLBACK 1 mux → 0 pares, mux solo {0} (placa degradada, sin par).
// =====================================================================
void test_un_mux_solo_sin_par(void) {
    AdcScanPlan p = adc_plan_build(1);
    TEST_ASSERT_EQUAL_INT(0, p.n_pairs);
    TEST_ASSERT_TRUE(p.has_solo);
    TEST_ASSERT_EQUAL_UINT8(0, p.solo_mux);
    TEST_ASSERT_EQUAL_INT(1, p.n_muxes);
}

// =====================================================================
// DEFENSA: 0 muxes → plan vacío, no crashea, no inventa lecturas.
// =====================================================================
void test_cero_muxes_plan_vacio(void) {
    AdcScanPlan p = adc_plan_build(0);
    TEST_ASSERT_EQUAL_INT(0, p.n_pairs);
    TEST_ASSERT_FALSE(p.has_solo);
    TEST_ASSERT_EQUAL_INT(0, p.n_muxes);
}

// =====================================================================
// FAIL-SAFE de borde: n fuera de rango se SATURA, nunca índice inexistente.
// =====================================================================
// n negativo → tratado como 0 (plan vacío), sin indexar fuera de los arrays.
void test_negativo_satura_a_cero(void) {
    AdcScanPlan p = adc_plan_build(-3);
    TEST_ASSERT_EQUAL_INT(0, p.n_pairs);
    TEST_ASSERT_FALSE(p.has_solo);
    TEST_ASSERT_EQUAL_INT(0, p.n_muxes);
}

// n > MAX (más muxes que los físicos) → saturado a 4. El plan no genera más de MAX/2 pares
// y todos los índices quedan dentro de [0, MAX). Defensa contra overflow del array.
void test_exceso_satura_a_max(void) {
    AdcScanPlan p = adc_plan_build(99);
    TEST_ASSERT_EQUAL_INT(ADC_SCAN_MAX_MUXES, p.n_muxes);
    TEST_ASSERT_EQUAL_INT(ADC_SCAN_MAX_PAIRS, p.n_pairs);
    TEST_ASSERT_TRUE(p.n_pairs <= ADC_SCAN_MAX_PAIRS);
    TEST_ASSERT_FALSE(p.has_solo);                   // 4 es par → sin solo
    assert_pairs_sin_repetir(p);
}

// =====================================================================
// DETERMINISMO: misma entrada → mismo plan (sin estado oculto).
// =====================================================================
void test_determinista(void) {
    AdcScanPlan a = adc_plan_build(4);
    AdcScanPlan b = adc_plan_build(4);
    TEST_ASSERT_EQUAL_INT(a.n_pairs, b.n_pairs);
    TEST_ASSERT_EQUAL_INT(a.has_solo ? 1 : 0, b.has_solo ? 1 : 0);
    for (int k = 0; k < a.n_pairs; ++k) {
        TEST_ASSERT_EQUAL_UINT8(a.pairs[k][0], b.pairs[k][0]);
        TEST_ASSERT_EQUAL_UINT8(a.pairs[k][1], b.pairs[k][1]);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_cuatro_muxes_dos_pares);
    RUN_TEST(test_cuatro_muxes_apareo_consecutivo);
    RUN_TEST(test_cuatro_muxes_cobertura_completa);
    RUN_TEST(test_dos_muxes_un_par);
    RUN_TEST(test_tres_muxes_par_mas_solo);
    RUN_TEST(test_un_mux_solo_sin_par);
    RUN_TEST(test_cero_muxes_plan_vacio);
    RUN_TEST(test_negativo_satura_a_cero);
    RUN_TEST(test_exceso_satura_a_max);
    RUN_TEST(test_determinista);
    return UNITY_END();
}
