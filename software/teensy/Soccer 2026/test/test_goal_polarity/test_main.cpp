// test_goal_polarity — tests del módulo puro goal_polarity.{h,cpp}.
// Corre con: bash scripts/run-host-tests.sh test_goal_polarity
//
// Cubre la inferencia (arco al frente = rival), el latch anti-rebote (fija y
// queda estable), y el fallback fail-safe.

#include <unity.h>
#include "goal_polarity.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ── Inferencia ───────────────────────────────────────────────────────────────

void test_infer_yellow_front_is_opp(void) {
    // Amarillo al frente (0°) → amarillo es el rival.
    TEST_ASSERT_EQUAL(GoalPolarity::YELLOW_IS_OPP,
                      goal_polarity_infer(true, 0.0f, false, 0.0f));
    // Con el azul atrás (180°) corrobora.
    TEST_ASSERT_EQUAL(GoalPolarity::YELLOW_IS_OPP,
                      goal_polarity_infer(true, 10.0f, true, 175.0f));
}

void test_infer_blue_front_is_opp(void) {
    TEST_ASSERT_EQUAL(GoalPolarity::BLUE_IS_OPP,
                      goal_polarity_infer(false, 0.0f, true, -20.0f));
    TEST_ASSERT_EQUAL(GoalPolarity::BLUE_IS_OPP,
                      goal_polarity_infer(true, 160.0f, true, 5.0f));
}

void test_infer_from_back_goal_only(void) {
    // Sólo se ve el PROPIO (atrás): se deduce el rival por descarte.
    // Amarillo atrás (180°) = propio → azul es rival.
    TEST_ASSERT_EQUAL(GoalPolarity::BLUE_IS_OPP,
                      goal_polarity_infer(true, 180.0f, false, 0.0f));
    // Azul atrás = propio → amarillo rival.
    TEST_ASSERT_EQUAL(GoalPolarity::YELLOW_IS_OPP,
                      goal_polarity_infer(false, 0.0f, true, -150.0f));
}

void test_infer_conflict_both_front_is_unknown(void) {
    // Los dos al frente → no se puede decidir.
    TEST_ASSERT_EQUAL(GoalPolarity::UNKNOWN,
                      goal_polarity_infer(true, 10.0f, true, -10.0f));
}

void test_infer_none_visible_is_unknown(void) {
    TEST_ASSERT_EQUAL(GoalPolarity::UNKNOWN,
                      goal_polarity_infer(false, 0.0f, false, 0.0f));
}

void test_infer_boundary_90deg_is_back(void) {
    // |ángulo| == 90 NO es frente (estrictamente < 90).
    TEST_ASSERT_EQUAL(GoalPolarity::BLUE_IS_OPP,    // amarillo a 90 = atrás = propio
                      goal_polarity_infer(true, 90.0f, false, 0.0f));
}

// ── Latch ────────────────────────────────────────────────────────────────────

void test_latch_confirms_after_n_readings(void) {
    GoalPolarityLatch l;
    goal_polarity_latch_init(l);
    // Antes de CONFIRM, sigue UNKNOWN.
    for (uint16_t i = 0; i < GOAL_POLARITY_CONFIRM - 1; ++i) {
        TEST_ASSERT_EQUAL(GoalPolarity::UNKNOWN,
                          goal_polarity_latch_update(l, GoalPolarity::YELLOW_IS_OPP));
    }
    // La lectura número CONFIRM la fija.
    TEST_ASSERT_EQUAL(GoalPolarity::YELLOW_IS_OPP,
                      goal_polarity_latch_update(l, GoalPolarity::YELLOW_IS_OPP));
}

void test_latch_is_stable_once_set(void) {
    GoalPolarityLatch l;
    goal_polarity_latch_init(l);
    for (uint16_t i = 0; i < GOAL_POLARITY_CONFIRM; ++i) {
        goal_polarity_latch_update(l, GoalPolarity::BLUE_IS_OPP);
    }
    TEST_ASSERT_EQUAL(GoalPolarity::BLUE_IS_OPP, l.value);
    // Lecturas posteriores DISTINTAS no la cambian (estabilidad de partido).
    for (int i = 0; i < 100; ++i) {
        TEST_ASSERT_EQUAL(GoalPolarity::BLUE_IS_OPP,
                          goal_polarity_latch_update(l, GoalPolarity::YELLOW_IS_OPP));
    }
    TEST_ASSERT_EQUAL(GoalPolarity::BLUE_IS_OPP,
                      goal_polarity_latch_update(l, GoalPolarity::UNKNOWN));
}

void test_latch_unknown_resets_candidate(void) {
    GoalPolarityLatch l;
    goal_polarity_latch_init(l);
    // Acumula casi hasta confirmar…
    for (uint16_t i = 0; i < GOAL_POLARITY_CONFIRM - 1; ++i) {
        goal_polarity_latch_update(l, GoalPolarity::YELLOW_IS_OPP);
    }
    // …un UNKNOWN resetea el conteo → NO debe fijar con la siguiente sola lectura.
    goal_polarity_latch_update(l, GoalPolarity::UNKNOWN);
    TEST_ASSERT_EQUAL(GoalPolarity::UNKNOWN,
                      goal_polarity_latch_update(l, GoalPolarity::YELLOW_IS_OPP));
}

void test_latch_inconsistent_does_not_confirm(void) {
    GoalPolarityLatch l;
    goal_polarity_latch_init(l);
    // Alternar candidatos nunca acumula CONFIRM consecutivas → nunca fija.
    for (int i = 0; i < 100; ++i) {
        GoalPolarity g = (i % 2 == 0) ? GoalPolarity::YELLOW_IS_OPP
                                      : GoalPolarity::BLUE_IS_OPP;
        TEST_ASSERT_EQUAL(GoalPolarity::UNKNOWN, goal_polarity_latch_update(l, g));
    }
}

void test_effective_uses_fallback_until_latched(void) {
    GoalPolarityLatch l;
    goal_polarity_latch_init(l);
    // Sin fijar → devuelve el fallback (fail-safe = comportamiento previo).
    TEST_ASSERT_EQUAL(GoalPolarity::YELLOW_IS_OPP,
                      goal_polarity_effective(l, GoalPolarity::YELLOW_IS_OPP));
    // Tras fijar BLUE → devuelve la fijada, ignora el fallback.
    for (uint16_t i = 0; i < GOAL_POLARITY_CONFIRM; ++i) {
        goal_polarity_latch_update(l, GoalPolarity::BLUE_IS_OPP);
    }
    TEST_ASSERT_EQUAL(GoalPolarity::BLUE_IS_OPP,
                      goal_polarity_effective(l, GoalPolarity::YELLOW_IS_OPP));
}

// ============================================================================
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_infer_yellow_front_is_opp);
    RUN_TEST(test_infer_blue_front_is_opp);
    RUN_TEST(test_infer_from_back_goal_only);
    RUN_TEST(test_infer_conflict_both_front_is_unknown);
    RUN_TEST(test_infer_none_visible_is_unknown);
    RUN_TEST(test_infer_boundary_90deg_is_back);
    RUN_TEST(test_latch_confirms_after_n_readings);
    RUN_TEST(test_latch_is_stable_once_set);
    RUN_TEST(test_latch_unknown_resets_candidate);
    RUN_TEST(test_latch_inconsistent_does_not_confirm);
    RUN_TEST(test_effective_uses_fallback_until_latched);
    return UNITY_END();
}
