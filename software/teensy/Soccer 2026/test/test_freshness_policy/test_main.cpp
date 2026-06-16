// test_freshness_policy — TDD de la POLÍTICA PURA de frescura→sentinela.
//
// Cubre src/shared/freshness_policy.h: slot_eval() decide {FRESH,STALE,NEVER}
// según (ever_ok, last_ok, now, timeout), wrap-safe y con el borde == timeout =
// FRESH (idéntico a tof_fresh_or_no_reading). Encima: fresh_ok(), conf_gate()
// (el techo de frescura le gana a la confianza), age_to_u8_ms() (satura a 255).
//
// Foco en el FAIL-SAFE: nunca-ok → NEVER + conf 0 aunque el timestamp luzca
// reciente; stale → conf 0; reloj raro (now<last_ok) → FRESH sin underflow.
//
// Corre host (sin Arduino) con:
//   bash scripts/run-host-tests.sh test_freshness_policy

#include <unity.h>
#include "freshness_policy.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// (1) FRESCO: dato reciente dentro del timeout → FRESH, edad real, conf pasa.
// ============================================================================

void test_fresh_within_timeout(void) {
    // last_ok=1000, now=1100, timeout=250 → elapsed 100 <= 250 → FRESH.
    SlotFreshness f = slot_eval(/*ever_ok=*/true, /*last_ok=*/1000u,
                                /*now=*/1100u, /*timeout=*/250u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SlotState::FRESH, (uint8_t)f.state);
    TEST_ASSERT_EQUAL_UINT32(100u, f.age_ms);
    TEST_ASSERT_TRUE(fresh_ok(f));
    // conf pasa tal cual cuando está fresco.
    TEST_ASSERT_EQUAL_UINT8(90, conf_gate(90, f));
}

// ============================================================================
// (2) BORDE: elapsed == timeout EXACTO → todavía FRESCO (corte es > timeout).
// ============================================================================

void test_border_elapsed_equals_timeout_is_fresh(void) {
    // last_ok=1000, now=1250, timeout=250 → elapsed 250 == timeout → FRESH.
    SlotFreshness f = slot_eval(true, 1000u, 1250u, 250u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SlotState::FRESH, (uint8_t)f.state);
    TEST_ASSERT_EQUAL_UINT32(250u, f.age_ms);
    TEST_ASSERT_TRUE(fresh_ok(f));
    TEST_ASSERT_EQUAL_UINT8(80, conf_gate(80, f));
}

// ============================================================================
// (3) BORDE +1: un ms más allá del timeout → STALE, y conf_gate colapsa a 0.
// ============================================================================

void test_one_ms_over_timeout_is_stale_and_gates_conf_to_zero(void) {
    // last_ok=1000, now=1251, timeout=250 → elapsed 251 > 250 → STALE.
    SlotFreshness f = slot_eval(true, 1000u, 1251u, 250u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SlotState::STALE, (uint8_t)f.state);
    TEST_ASSERT_EQUAL_UINT32(251u, f.age_ms);
    TEST_ASSERT_FALSE(fresh_ok(f));
    // El techo de frescura le gana a la confianza: 99 → 0.
    TEST_ASSERT_EQUAL_UINT8(0, conf_gate(99, f));
}

// ============================================================================
// (4) NEVER: ever_ok=false → NEVER + edad UINT32_MAX + conf 0, AUNQUE el
// last_ok luzca reciente (un .bss en cero diría "publicado en t=0" — basura).
// ============================================================================

void test_never_ok_is_never_even_if_last_ok_looks_recent(void) {
    // last_ok=now (luce "0 ms de viejo"), pero ever_ok=false → NEVER igual.
    SlotFreshness f = slot_eval(/*ever_ok=*/false, /*last_ok=*/5000u,
                                /*now=*/5000u, /*timeout=*/250u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SlotState::NEVER, (uint8_t)f.state);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, f.age_ms);
    TEST_ASSERT_FALSE(fresh_ok(f));
    // Confianza altísima NO sobrevive a NEVER.
    TEST_ASSERT_EQUAL_UINT8(0, conf_gate(255, f));
}

// Variante: ever_ok=false con last_ok=0 (el caso .bss puro). Igual: NEVER.
void test_never_ok_zero_init_slot(void) {
    SlotFreshness f = slot_eval(false, 0u, 0u, 250u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SlotState::NEVER, (uint8_t)f.state);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, f.age_ms);
    TEST_ASSERT_FALSE(fresh_ok(f));
}

// ============================================================================
// (5) RELOJ RARO: now < last_ok (wrap del contador o saltito atrás) → FRESH
// con edad 0, SIN underflow espurio a ~49 días.
// ============================================================================

void test_now_before_last_ok_is_fresh_no_underflow(void) {
    // now=10 < last_ok=1000: una resta unsigned ingenua daría ~4.29e9 (≈49 días);
    // la política lo trata como edad 0 → FRESH.
    SlotFreshness f = slot_eval(true, 1000u, 10u, 250u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SlotState::FRESH, (uint8_t)f.state);
    TEST_ASSERT_EQUAL_UINT32(0u, f.age_ms);
    TEST_ASSERT_TRUE(fresh_ok(f));
    TEST_ASSERT_EQUAL_UINT8(77, conf_gate(77, f));
}

// Caso extremo del reloj raro: now=0, last_ok=UINT32_MAX (justo cruzando el wrap).
void test_now_zero_last_ok_max_is_fresh(void) {
    SlotFreshness f = slot_eval(true, UINT32_MAX, 0u, 250u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SlotState::FRESH, (uint8_t)f.state);
    TEST_ASSERT_EQUAL_UINT32(0u, f.age_ms);
    TEST_ASSERT_TRUE(fresh_ok(f));
}

// ============================================================================
// (6) timeout==0: cualquier elapsed>0 vence; elapsed==0 sigue fresco.
// ============================================================================

void test_timeout_zero_only_same_instant_is_fresh(void) {
    // elapsed 0 con timeout 0 → 0 > 0 es false → FRESH.
    SlotFreshness same = slot_eval(true, 1000u, 1000u, 0u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SlotState::FRESH, (uint8_t)same.state);
    TEST_ASSERT_TRUE(fresh_ok(same));

    // elapsed 1 con timeout 0 → 1 > 0 → STALE.
    SlotFreshness later = slot_eval(true, 1000u, 1001u, 0u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SlotState::STALE, (uint8_t)later.state);
    TEST_ASSERT_FALSE(fresh_ok(later));
}

// ============================================================================
// (7) age_to_u8_ms: satura a 255, no envuelve.
// ============================================================================

void test_age_to_u8_ms_saturates(void) {
    TEST_ASSERT_EQUAL_UINT8(255, age_to_u8_ms(300u));    // > 255 → satura
    TEST_ASSERT_EQUAL_UINT8(255, age_to_u8_ms(255u));    // == 255 → 255
    TEST_ASSERT_EQUAL_UINT8(255, age_to_u8_ms(256u));    // 256 NO envuelve a 0
    TEST_ASSERT_EQUAL_UINT8(255, age_to_u8_ms(UINT32_MAX)); // edad de un NEVER → 255
    TEST_ASSERT_EQUAL_UINT8(50,  age_to_u8_ms(50u));     // dentro de rango → tal cual
    TEST_ASSERT_EQUAL_UINT8(0,   age_to_u8_ms(0u));      // 0 → 0
    TEST_ASSERT_EQUAL_UINT8(254, age_to_u8_ms(254u));    // borde justo bajo el techo
}

// ============================================================================
// (8) conf_gate NUNCA inventa confianza: con base_conf 0 sigue 0 aunque FRESH.
// ============================================================================

void test_conf_gate_does_not_invent_confidence(void) {
    SlotFreshness fresh = slot_eval(true, 1000u, 1000u, 250u);
    TEST_ASSERT_TRUE(fresh_ok(fresh));
    TEST_ASSERT_EQUAL_UINT8(0, conf_gate(0, fresh));   // 0 fresco sigue 0
    TEST_ASSERT_EQUAL_UINT8(1, conf_gate(1, fresh));   // mínimo pasa tal cual
}

int main(int, char**) {
    UNITY_BEGIN();
    // (1) fresco dentro de timeout
    RUN_TEST(test_fresh_within_timeout);
    // (2) borde == timeout = fresco
    RUN_TEST(test_border_elapsed_equals_timeout_is_fresh);
    // (3) +1 ms = stale → conf_gate 0
    RUN_TEST(test_one_ms_over_timeout_is_stale_and_gates_conf_to_zero);
    // (4) never-ok → NEVER + conf 0 (aunque timestamp luzca reciente)
    RUN_TEST(test_never_ok_is_never_even_if_last_ok_looks_recent);
    RUN_TEST(test_never_ok_zero_init_slot);
    // (5) reloj raro now<last_ok → fresh sin underflow
    RUN_TEST(test_now_before_last_ok_is_fresh_no_underflow);
    RUN_TEST(test_now_zero_last_ok_max_is_fresh);
    // (6) timeout==0
    RUN_TEST(test_timeout_zero_only_same_instant_is_fresh);
    // (7) age_to_u8_ms satura
    RUN_TEST(test_age_to_u8_ms_saturates);
    // (8) conf_gate no inventa confianza
    RUN_TEST(test_conf_gate_does_not_invent_confidence);
    return UNITY_END();
}
