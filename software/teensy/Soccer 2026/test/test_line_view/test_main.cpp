// test_line_view — corre con: pio test -e test_native -f test_line_view
//
// Tests PUROS de los helpers de line_view.h. Foco de este archivo (Track 3):
// lsv2_sample_is_stale, que implementa la regla §3.5.5 del contrato
// (CONTRATO-DATOS-DOWN.md): sample_age_ms viaja en el frame pero hoy nadie lo
// consume; este helper decide si la muestra está RANCIA, respetando la
// compuerta maestra data_valid y el sentinel 255 (= edad N/A, NO stale).
//
// Helper PURO, sin estado ni Arduino: el mismo que (eventualmente) consumiría
// world_model.cpp, pero acá lo probamos aislado.
#include <unity.h>
#include "types.h"
#include "line_view.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Arma un LineStatusV2 mínimo y válido (schema + data_valid) con la edad pedida.
static LineStatusV2 make_sample(uint8_t data_valid, uint8_t sample_age_ms) {
    LineStatusV2 s{};
    s.schema_version = LSV2_SCHEMA;
    s.data_valid     = data_valid;
    s.sample_age_ms  = sample_age_ms;
    return s;
}

// ============================================================================
// lsv2_sample_is_stale (Track 3 — regla §3.5.5)
// ============================================================================

// data_valid == 0 ⇒ false: la compuerta maestra ya descartó el frame; no
// reportamos "stale" sobre datos que de por sí no son confiables.
void test_stale_data_invalid_is_false(void) {
    // Edad altísima pero data_valid=0 → NO stale (gateado por la compuerta).
    LineStatusV2 s = make_sample(/*data_valid=*/0, /*age=*/200);
    TEST_ASSERT_FALSE(lsv2_sample_is_stale(s, 50));
}

// sample_age_ms == 255 ⇒ false: 255 es el sentinel N/A ("edad desconocida"),
// NO necesariamente rancio; no lo tratamos como stale.
void test_stale_age_sentinel_255_is_false(void) {
    LineStatusV2 s = make_sample(/*data_valid=*/1, /*age=*/255);
    TEST_ASSERT_FALSE(lsv2_sample_is_stale(s, 50));
    // Incluso con max_age muy chico, el sentinel sigue sin ser stale.
    TEST_ASSERT_FALSE(lsv2_sample_is_stale(s, 0));
}

// sample_age_ms <= max_age_ms ⇒ false (fresca).
void test_stale_age_within_limit_is_false(void) {
    // age == max → NO stale (la regla es estrictamente >).
    LineStatusV2 s_eq = make_sample(/*data_valid=*/1, /*age=*/50);
    TEST_ASSERT_FALSE(lsv2_sample_is_stale(s_eq, 50));
    // age < max → NO stale.
    LineStatusV2 s_lt = make_sample(/*data_valid=*/1, /*age=*/10);
    TEST_ASSERT_FALSE(lsv2_sample_is_stale(s_lt, 50));
    // age 0 (recién muestreado) → NO stale.
    LineStatusV2 s_zero = make_sample(/*data_valid=*/1, /*age=*/0);
    TEST_ASSERT_FALSE(lsv2_sample_is_stale(s_zero, 50));
}

// sample_age_ms > max_age_ms ⇒ true (rancia).
void test_stale_age_above_limit_is_true(void) {
    // age == max+1 → stale (primer valor fuera de tolerancia).
    LineStatusV2 s_edge = make_sample(/*data_valid=*/1, /*age=*/51);
    TEST_ASSERT_TRUE(lsv2_sample_is_stale(s_edge, 50));
    // age muy por encima → stale.
    LineStatusV2 s_old = make_sample(/*data_valid=*/1, /*age=*/200);
    TEST_ASSERT_TRUE(lsv2_sample_is_stale(s_old, 50));
    // 254 (justo por debajo del sentinel) con max chico → stale.
    LineStatusV2 s_254 = make_sample(/*data_valid=*/1, /*age=*/254);
    TEST_ASSERT_TRUE(lsv2_sample_is_stale(s_254, 50));
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_stale_data_invalid_is_false);
    RUN_TEST(test_stale_age_sentinel_255_is_false);
    RUN_TEST(test_stale_age_within_limit_is_false);
    RUN_TEST(test_stale_age_above_limit_is_true);
    return UNITY_END();
}
