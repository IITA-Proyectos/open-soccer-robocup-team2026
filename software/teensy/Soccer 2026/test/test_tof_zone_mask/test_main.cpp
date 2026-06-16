// test_tof_zone_mask — TDD de la máscara de zonas del ToF (A2.2).
//
// Cubre src/shared/tof_zone_mask.h: tof_zone_masked_mean (reduce las 16 zonas a una
// distancia aplicando la máscara), tof_zone_enabled y tof_zone_mask_set. Verifica el
// FAIL-SAFE (mask=~0 = no-op idéntico a promediar las válidas; todo anulado = no_reading).
//
//   bash scripts/run-host-tests.sh test_tof_zone_mask

#include <unity.h>
#include "tof_zone_mask.h"

using namespace iitasoccer;

static const uint16_t NR = 0xFFFF;   // TOF_NO_READING

void setUp(void) {}
void tearDown(void) {}

// 16 zonas de ejemplo: fila 0 (idx 0..3) "altas" = 1500mm, resto 300mm.
static void make_zones(uint16_t* z) {
    for (int i = 0; i < 16; ++i) z[i] = (i < 4) ? 1500 : 300;
}

// (1) mask=~0 (default) → promedio de TODAS las válidas (no-op vs comportamiento actual).
void test_full_mask_is_mean_of_all_valid(void) {
    uint16_t z[16]; make_zones(z);
    // promedio = (4*1500 + 12*300)/16 = (6000+3600)/16 = 9600/16 = 600
    TEST_ASSERT_EQUAL_UINT16(600, tof_zone_masked_mean(z, 16, ~0ull, NR));
}

// (2) anular la fila superior (idx 0..3) → promedio solo de las 12 de abajo = 300.
void test_mask_out_top_row(void) {
    uint16_t z[16]; make_zones(z);
    uint64_t mask = ~0ull;
    for (uint8_t i = 0; i < 4; ++i) mask = tof_zone_mask_set(mask, i, false);
    TEST_ASSERT_EQUAL_UINT16(300, tof_zone_masked_mean(z, 16, mask, NR));
}

// (3) zonas NO_READING se saltean (no cuentan en el promedio).
void test_no_reading_zones_skipped(void) {
    uint16_t z[16];
    for (int i = 0; i < 16; ++i) z[i] = NR;
    z[5] = 200; z[6] = 400;   // solo dos válidas
    TEST_ASSERT_EQUAL_UINT16(300, tof_zone_masked_mean(z, 16, ~0ull, NR));  // (200+400)/2
}

// (4) todas anuladas por máscara → no_reading (no inventa distancia).
void test_all_masked_is_no_reading(void) {
    uint16_t z[16]; make_zones(z);
    TEST_ASSERT_EQUAL_UINT16(NR, tof_zone_masked_mean(z, 16, 0ull, NR));
}

// (5) máscara deja zonas habilitadas pero todas sin lectura → no_reading.
void test_enabled_but_all_no_reading(void) {
    uint16_t z[16];
    for (int i = 0; i < 16; ++i) z[i] = NR;
    TEST_ASSERT_EQUAL_UINT16(NR, tof_zone_masked_mean(z, 16, ~0ull, NR));
}

// (6) mezcla: anular una fila Y una zona de abajo está sin lectura.
void test_mask_and_no_reading_combo(void) {
    uint16_t z[16]; make_zones(z);   // idx0..3=1500, resto=300
    z[8] = NR;                        // una de abajo sin lectura
    uint64_t mask = ~0ull;
    for (uint8_t i = 0; i < 4; ++i) mask = tof_zone_mask_set(mask, i, false);  // anular fila alta
    // quedan 11 zonas de 300 (12 de abajo menos la idx8 NR) → promedio 300.
    TEST_ASSERT_EQUAL_UINT16(300, tof_zone_masked_mean(z, 16, mask, NR));
}

// (7) tof_zone_enabled / tof_zone_mask_set: bits correctos + fuera de rango.
void test_enabled_and_set(void) {
    uint64_t m = 0ull;
    m = tof_zone_mask_set(m, 3, true);
    TEST_ASSERT_TRUE(tof_zone_enabled(m, 3));
    TEST_ASSERT_FALSE(tof_zone_enabled(m, 4));
    m = tof_zone_mask_set(m, 3, false);
    TEST_ASSERT_FALSE(tof_zone_enabled(m, 3));
    // fuera de rango: no-op + false
    TEST_ASSERT_EQUAL_UINT64(m, tof_zone_mask_set(m, 64, true));
    TEST_ASSERT_FALSE(tof_zone_enabled(m, 64));
}

// (8) puntero nulo → no_reading (defensivo).
void test_null_ptr_is_no_reading(void) {
    TEST_ASSERT_EQUAL_UINT16(NR, tof_zone_masked_mean(nullptr, 16, ~0ull, NR));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_full_mask_is_mean_of_all_valid);
    RUN_TEST(test_mask_out_top_row);
    RUN_TEST(test_no_reading_zones_skipped);
    RUN_TEST(test_all_masked_is_no_reading);
    RUN_TEST(test_enabled_but_all_no_reading);
    RUN_TEST(test_mask_and_no_reading_combo);
    RUN_TEST(test_enabled_and_set);
    RUN_TEST(test_null_ptr_is_no_reading);
    return UNITY_END();
}
