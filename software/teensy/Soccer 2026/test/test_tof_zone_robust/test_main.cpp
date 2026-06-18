// test_tof_zone_robust — reductor ROBUSTO de zonas tof_zone_masked_robust (descarta rayos
// fuera de cancha y outliers bajos = rebote en robot). Puro, host-testeable.
// Corre con: bash scripts/run-host-tests.sh test_tof_zone_robust

#include <unity.h>
#include "tof_zone_mask.h"

using namespace iitasoccer;

static const uint16_t NR = 0xFFFF;          // no_reading
static const uint16_t FMAX = 2430;          // dimension mas larga de la cancha
static const uint8_t  PCT = 70;             // descarta < 70% de la mediana
static const uint64_t ALL = ~(uint64_t)0;   // todas las zonas activas

void setUp(void) {}
void tearDown(void) {}

void test_all_similar_is_plain_mean(void) {
    const uint16_t z[4] = {1000, 1010, 990, 1000};
    // mediana 1000, low_thresh 700, todas pasan -> (1000+1010+990+1000)/4 = 1000
    TEST_ASSERT_EQUAL_UINT16(1000, tof_zone_masked_robust(z, 4, ALL, NR, FMAX, PCT));
}

void test_low_outlier_robot_bounce_excluded(void) {
    // 300 = rebote en otro robot (corto). mediana de {300,990,1000,1010}=1000, thresh=700.
    // 300 descartado -> mean(990,1000,1010) = 1000.
    const uint16_t z[4] = {1000, 1010, 300, 990};
    TEST_ASSERT_EQUAL_UINT16(1000, tof_zone_masked_robust(z, 4, ALL, NR, FMAX, PCT));
}

void test_over_field_max_excluded(void) {
    // 2600 > 2430 (sobre la pared / sin retorno) -> descartado en la recoleccion.
    const uint16_t z[4] = {1000, 1010, 2600, 990};
    TEST_ASSERT_EQUAL_UINT16(1000, tof_zone_masked_robust(z, 4, ALL, NR, FMAX, PCT));
}

void test_no_reading_zones_ignored(void) {
    const uint16_t z[4] = {1000, NR, 990, NR};   // 2 validas (<3) -> promedio simple, sin outliers
    TEST_ASSERT_EQUAL_UINT16(995, tof_zone_masked_robust(z, 4, ALL, NR, FMAX, PCT));
}

void test_all_out_of_range_returns_no_reading(void) {
    const uint16_t z[3] = {3000, NR, 2500};      // todas > FMAX o NR -> sin lectura
    TEST_ASSERT_EQUAL_UINT16(NR, tof_zone_masked_robust(z, 3, ALL, NR, FMAX, PCT));
}

void test_mask_vetoes_zones(void) {
    // máscara veta la zona 2 (la buena 990); quedan {1000,1010,1000} -> 1003 (1003.33 trunc)
    const uint16_t z[4] = {1000, 1010, 990, 1000};
    const uint64_t mask = ALL & ~(1ull << 2);
    TEST_ASSERT_EQUAL_UINT16(1003, tof_zone_masked_robust(z, 4, mask, NR, FMAX, PCT));
}

void test_disabling_filters_equals_mean(void) {
    // field_max=0 y low_keep_pct=0 -> sin filtros -> igual que promedio de todas las validas.
    const uint16_t z[4] = {1000, 1010, 300, 990};
    // mean(1000,1010,300,990) = 825
    TEST_ASSERT_EQUAL_UINT16(825, tof_zone_masked_robust(z, 4, ALL, NR, 0, 0));
}

void test_majority_robot_limitation_documented(void) {
    // LIMITE CONOCIDO: si el robot domina el FOV (mayoria de zonas cortas), la mediana ES el
    // robot y el filtro NO recupera la pared. Aca 3 cortas (300) + 1 pared (1000): mediana=300,
    // thresh=210, todas pasan -> mean contaminado ~475. (Documenta el limite; no es un bug.)
    const uint16_t z[4] = {300, 300, 1000, 300};
    TEST_ASSERT_EQUAL_UINT16(475, tof_zone_masked_robust(z, 4, ALL, NR, FMAX, PCT));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_all_similar_is_plain_mean);
    RUN_TEST(test_low_outlier_robot_bounce_excluded);
    RUN_TEST(test_over_field_max_excluded);
    RUN_TEST(test_no_reading_zones_ignored);
    RUN_TEST(test_all_out_of_range_returns_no_reading);
    RUN_TEST(test_mask_vetoes_zones);
    RUN_TEST(test_disabling_filters_equals_mean);
    RUN_TEST(test_majority_robot_limitation_documented);
    return UNITY_END();
}
