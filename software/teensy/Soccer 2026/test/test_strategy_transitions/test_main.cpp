// test_strategy_transitions — caracterización del árbol de decisión de la FSM.
// Corre en host con: pio test -e test_native -f test_strategy_transitions
//
// Estos tests fijan el comportamiento de transición que strategy.cpp tiene HOY
// (commit b10c66d, Nivel 2). Ver strategy_transitions.h para el límite honesto:
// strategy.cpp todavía NO llama a estas funciones — esto es una red de
// caracterización, no prueba línea-por-línea del binario.

#include <unity.h>
#include "strategy_transitions.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

static AtkTuning T_ATK;
static GkTuning  T_GK;

// Helper: AtkWorldView con defaults "neutros" (match corriendo, sin línea, sin
// pelota, sin arco). Cada test sobreescribe lo que necesita.
static AtkWorldView atk_w() {
    AtkWorldView w{};
    w.match_running = true;
    w.imminent_exit = false;
    w.line_fresh = false;
    w.ball_visible = false;
    w.goal_visible = false;
    w.ball_x_mm = 0.0f;
    w.ball_y_mm = 0.0f;
    w.goal_angle_deg = 0.0f;
    w.now_ms = 10000;
    w.kickoff_started_ms = 0;
    return w;
}

static GkWorldView gk_w() {
    GkWorldView w{};
    w.match_running = true;
    w.imminent_exit = false;
    w.line_fresh = false;
    w.ball_visible = false;
    w.ball_x_mm = 0.0f;
    w.ball_y_mm = 0.0f;
    return w;
}

// ============================================================================
// ATK — transiciones globales prioritarias
// ============================================================================

void test_atk_match_stopped_forces_wait_start_from_any_state(void) {
    AtkWorldView w = atk_w();
    w.match_running = false;
    AtkDecision d = atk_decide_transition(AtkPhase::APPROACH, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::WAIT_START, d.next_phase);
}

void test_atk_match_stop_beats_imminent_exit(void) {
    // !match_running gana sobre línea inminente (orden de strategy.cpp).
    AtkWorldView w = atk_w();
    w.match_running = false;
    w.imminent_exit = true;
    w.line_fresh = true;
    AtkDecision d = atk_decide_transition(AtkPhase::SEARCH, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::WAIT_START, d.next_phase);
}

void test_atk_kickoff_edge_enters_kickoff(void) {
    // Flanco STOP→RUN: prev=false, ahora match corre.
    AtkWorldView w = atk_w();
    w.match_running = true;
    AtkDecision d = atk_decide_transition(AtkPhase::WAIT_START, false, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::KICKOFF, d.next_phase);
    TEST_ASSERT_TRUE(d.start_kickoff_timer);
}

void test_atk_no_kickoff_when_match_already_running(void) {
    // prev=true → no hay flanco → no entra KICKOFF.
    AtkWorldView w = atk_w();
    AtkDecision d = atk_decide_transition(AtkPhase::SEARCH, true, w, T_ATK);
    TEST_ASSERT_FALSE(d.start_kickoff_timer);
    TEST_ASSERT_EQUAL(AtkPhase::SEARCH, d.next_phase);  // se queda buscando
}

void test_atk_line_avoid_beats_kickoff(void) {
    // Línea inminente+fresca gana sobre el flanco kickoff.
    AtkWorldView w = atk_w();
    w.imminent_exit = true;
    w.line_fresh = true;
    AtkDecision d = atk_decide_transition(AtkPhase::WAIT_START, false, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::LINE_AVOID, d.next_phase);
    TEST_ASSERT_FALSE(d.start_kickoff_timer);
}

void test_atk_imminent_exit_ignored_if_line_stale(void) {
    // imminent_exit pero datos NO frescos → no entra LINE_AVOID.
    AtkWorldView w = atk_w();
    w.imminent_exit = true;
    w.line_fresh = false;
    AtkDecision d = atk_decide_transition(AtkPhase::SEARCH, true, w, T_ATK);
    TEST_ASSERT_NOT_EQUAL(AtkPhase::LINE_AVOID, d.next_phase);
}

// ============================================================================
// ATK — KICKOFF timer
// ============================================================================

void test_atk_kickoff_holds_until_duration(void) {
    AtkWorldView w = atk_w();
    w.now_ms = 800;
    w.kickoff_started_ms = 700;   // 100 ms < 250 ms
    AtkDecision d = atk_decide_transition(AtkPhase::KICKOFF, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::KICKOFF, d.next_phase);
}

void test_atk_kickoff_expires_to_search(void) {
    AtkWorldView w = atk_w();
    w.now_ms = 1000;
    w.kickoff_started_ms = 700;   // 300 ms >= 250 ms
    AtkDecision d = atk_decide_transition(AtkPhase::KICKOFF, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::SEARCH, d.next_phase);
}

void test_atk_kickoff_just_started_uses_now_not_stale_timestamp(void) {
    // Detalle sutil: cuando el flanco recién dispara KICKOFF, el timer se
    // evalúa con `now` (no con el kickoff_started_ms viejo=0). Si usara el
    // viejo, now-0 sería enorme y saltaría a SEARCH en el mismo tick.
    AtkWorldView w = atk_w();
    w.now_ms = 99999;             // grande a propósito
    w.kickoff_started_ms = 0;     // valor viejo
    AtkDecision d = atk_decide_transition(AtkPhase::WAIT_START, false, w, T_ATK);
    TEST_ASSERT_TRUE(d.start_kickoff_timer);
    TEST_ASSERT_EQUAL(AtkPhase::KICKOFF, d.next_phase);  // NO salta a SEARCH
}

// ============================================================================
// ATK — LINE_AVOID
// ============================================================================

void test_atk_line_avoid_returns_to_search_when_clear(void) {
    AtkWorldView w = atk_w();
    w.imminent_exit = false;      // ya no hay línea
    AtkDecision d = atk_decide_transition(AtkPhase::LINE_AVOID, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::SEARCH, d.next_phase);
}

void test_atk_line_avoid_holds_while_imminent(void) {
    AtkWorldView w = atk_w();
    w.imminent_exit = true;
    w.line_fresh = true;
    AtkDecision d = atk_decide_transition(AtkPhase::LINE_AVOID, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::LINE_AVOID, d.next_phase);
}

// ============================================================================
// ATK — SEARCH → POSITION / APPROACH (decisión behind-the-ball)
// ============================================================================

void test_atk_search_to_approach_when_ball_aligned_with_goal(void) {
    AtkWorldView w = atk_w();
    w.ball_visible = true;
    w.goal_visible = true;
    w.ball_x_mm = 0.0f;
    w.ball_y_mm = 400.0f;         // ball al frente
    w.goal_angle_deg = 0.0f;      // arco al frente → alineado
    AtkDecision d = atk_decide_transition(AtkPhase::SEARCH, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::APPROACH, d.next_phase);
}

void test_atk_search_to_position_when_ball_not_aligned(void) {
    AtkWorldView w = atk_w();
    w.ball_visible = true;
    w.goal_visible = true;
    w.ball_x_mm = 400.0f;         // ball a la derecha
    w.ball_y_mm = 0.0f;
    w.goal_angle_deg = 0.0f;      // arco al frente → NO alineado (90° off)
    AtkDecision d = atk_decide_transition(AtkPhase::SEARCH, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::POSITION, d.next_phase);
}

void test_atk_search_to_approach_fallback_when_no_goal(void) {
    AtkWorldView w = atk_w();
    w.ball_visible = true;
    w.goal_visible = false;       // sin arco → APPROACH directo (Nivel 1)
    w.ball_x_mm = 400.0f;
    w.ball_y_mm = 0.0f;
    AtkDecision d = atk_decide_transition(AtkPhase::SEARCH, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::APPROACH, d.next_phase);
}

void test_atk_search_holds_when_no_ball(void) {
    AtkWorldView w = atk_w();
    w.ball_visible = false;
    AtkDecision d = atk_decide_transition(AtkPhase::SEARCH, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::SEARCH, d.next_phase);
}

// ============================================================================
// ATK — POSITION
// ============================================================================

void test_atk_position_to_search_when_ball_lost(void) {
    AtkWorldView w = atk_w();
    w.ball_visible = false;
    AtkDecision d = atk_decide_transition(AtkPhase::POSITION, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::SEARCH, d.next_phase);
}

void test_atk_position_to_approach_when_goal_lost(void) {
    AtkWorldView w = atk_w();
    w.ball_visible = true;
    w.goal_visible = false;       // perdí el arco → degradar a APPROACH
    AtkDecision d = atk_decide_transition(AtkPhase::POSITION, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::APPROACH, d.next_phase);
}

void test_atk_position_to_approach_when_reached_and_aligned(void) {
    // ball al frente cerca: target detrás queda dentro de POSITION_REACHED.
    AtkWorldView w = atk_w();
    w.ball_visible = true;
    w.goal_visible = true;
    w.ball_x_mm = 0.0f;
    w.ball_y_mm = 100.0f;         // target=(0, 100-120)=(0,-20) → |t|=20 < 80
    w.goal_angle_deg = 0.0f;      // alineado
    AtkDecision d = atk_decide_transition(AtkPhase::POSITION, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::APPROACH, d.next_phase);
}

void test_atk_position_holds_when_not_reached(void) {
    AtkWorldView w = atk_w();
    w.ball_visible = true;
    w.goal_visible = true;
    w.ball_x_mm = 0.0f;
    w.ball_y_mm = 600.0f;         // target=(0, 480) → |t|=480 > 80 → NO reached
    w.goal_angle_deg = 0.0f;
    AtkDecision d = atk_decide_transition(AtkPhase::POSITION, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::POSITION, d.next_phase);
}

// ============================================================================
// ATK — APPROACH (incluye el flag de empuje/push alineado, campo kicker_fire —
// nombre vestigial: el robot NO tiene kicker físico, empuja por inercia)
// ============================================================================

void test_atk_approach_to_search_when_ball_lost(void) {
    AtkWorldView w = atk_w();
    w.ball_visible = false;
    AtkDecision d = atk_decide_transition(AtkPhase::APPROACH, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::SEARCH, d.next_phase);
}

void test_atk_approach_to_search_when_ball_too_close_singular(void) {
    // dist < 1 mm → tratado como "perdida" (evita división por ~0).
    AtkWorldView w = atk_w();
    w.ball_visible = true;
    w.ball_x_mm = 0.0f;
    w.ball_y_mm = 0.5f;
    AtkDecision d = atk_decide_transition(AtkPhase::APPROACH, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::SEARCH, d.next_phase);
}

void test_atk_approach_back_to_position_on_hysteresis_loss(void) {
    // Pelota se fue de la línea de ataque más allá de tol+10 → re-orbitar.
    AtkWorldView w = atk_w();
    w.ball_visible = true;
    w.goal_visible = true;
    w.ball_x_mm = 400.0f;         // 90° respecto al frente
    w.ball_y_mm = 0.0f;
    w.goal_angle_deg = 0.0f;      // diff 90° > (30+10) → fuera de línea
    AtkDecision d = atk_decide_transition(AtkPhase::APPROACH, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::POSITION, d.next_phase);
    TEST_ASSERT_FALSE(d.kicker_fire);
}

void test_atk_approach_commits_push_when_aligned_and_close(void) {
    // "commits push" = alineado para empujar (el robot NO tiene kicker físico).
    AtkWorldView w = atk_w();
    w.ball_visible = true;
    w.goal_visible = true;
    w.ball_x_mm = 0.0f;
    w.ball_y_mm = 50.0f;          // dist 50 < kick_dist 80
    w.goal_angle_deg = 0.0f;      // |goal| 0 < kick_angle 12
    AtkDecision d = atk_decide_transition(AtkPhase::APPROACH, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::APPROACH, d.next_phase);
    TEST_ASSERT_TRUE(d.kicker_fire);   // campo vestigial = aligned_to_push
}

void test_atk_approach_no_push_when_far(void) {
    AtkWorldView w = atk_w();
    w.ball_visible = true;
    w.goal_visible = true;
    w.ball_x_mm = 0.0f;
    w.ball_y_mm = 200.0f;         // dist 200 > kick_dist 80
    w.goal_angle_deg = 0.0f;
    AtkDecision d = atk_decide_transition(AtkPhase::APPROACH, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::APPROACH, d.next_phase);
    TEST_ASSERT_FALSE(d.kicker_fire);  // campo vestigial = aligned_to_push
}

void test_atk_approach_no_push_when_goal_not_visible(void) {
    AtkWorldView w = atk_w();
    w.ball_visible = true;
    w.goal_visible = false;       // sin arco → no se evalúa el empuje
    w.ball_x_mm = 0.0f;
    w.ball_y_mm = 50.0f;
    AtkDecision d = atk_decide_transition(AtkPhase::APPROACH, true, w, T_ATK);
    TEST_ASSERT_EQUAL(AtkPhase::APPROACH, d.next_phase);
    TEST_ASSERT_FALSE(d.kicker_fire);  // campo vestigial = aligned_to_push
}

// ============================================================================
// GK — transiciones
// ============================================================================

void test_gk_match_stopped_forces_wait_start(void) {
    GkWorldView w = gk_w();
    w.match_running = false;
    GkDecision d = gk_decide_transition(GkPhase::INTERCEPT, w, T_GK);
    TEST_ASSERT_EQUAL(GkPhase::WAIT_START, d.next_phase);
}

void test_gk_wait_start_to_patrol_when_match_runs(void) {
    GkWorldView w = gk_w();
    w.match_running = true;
    GkDecision d = gk_decide_transition(GkPhase::WAIT_START, w, T_GK);
    TEST_ASSERT_EQUAL(GkPhase::PATROL, d.next_phase);
}

void test_gk_patrol_to_intercept_when_ball_seen(void) {
    GkWorldView w = gk_w();
    w.ball_visible = true;
    GkDecision d = gk_decide_transition(GkPhase::PATROL, w, T_GK);
    TEST_ASSERT_EQUAL(GkPhase::INTERCEPT, d.next_phase);
}

void test_gk_intercept_to_clear_when_ball_close(void) {
    GkWorldView w = gk_w();
    w.ball_visible = true;
    w.ball_x_mm = 100.0f;
    w.ball_y_mm = 100.0f;         // dist ~141 < 250 trigger
    GkDecision d = gk_decide_transition(GkPhase::INTERCEPT, w, T_GK);
    TEST_ASSERT_EQUAL(GkPhase::CLEAR, d.next_phase);
}

void test_gk_intercept_holds_when_ball_far_and_visible(void) {
    GkWorldView w = gk_w();
    w.ball_visible = true;
    w.ball_x_mm = 0.0f;
    w.ball_y_mm = 300.0f;         // dist 300 >= 250 trigger
    GkDecision d = gk_decide_transition(GkPhase::INTERCEPT, w, T_GK);
    TEST_ASSERT_EQUAL(GkPhase::INTERCEPT, d.next_phase);
}

void test_gk_intercept_to_patrol_when_ball_lost_and_far(void) {
    GkWorldView w = gk_w();
    w.ball_visible = false;
    w.ball_x_mm = 0.0f;
    w.ball_y_mm = 300.0f;         // dist >= trigger → no CLEAR; !visible → PATROL
    GkDecision d = gk_decide_transition(GkPhase::INTERCEPT, w, T_GK);
    TEST_ASSERT_EQUAL(GkPhase::PATROL, d.next_phase);
}

void test_gk_clear_to_patrol_when_ball_lost_beats_release(void) {
    // Orden EXACTO: !ball_visible gana sobre el chequeo de release.
    GkWorldView w = gk_w();
    w.ball_visible = false;
    w.ball_x_mm = 0.0f;
    w.ball_y_mm = 9999.0f;        // dist enorme > release, pero !visible primero
    GkDecision d = gk_decide_transition(GkPhase::CLEAR, w, T_GK);
    TEST_ASSERT_EQUAL(GkPhase::PATROL, d.next_phase);
}

void test_gk_clear_to_intercept_on_release_hysteresis(void) {
    GkWorldView w = gk_w();
    w.ball_visible = true;
    w.ball_x_mm = 0.0f;
    w.ball_y_mm = 500.0f;         // dist 500 > 400 release
    GkDecision d = gk_decide_transition(GkPhase::CLEAR, w, T_GK);
    TEST_ASSERT_EQUAL(GkPhase::INTERCEPT, d.next_phase);
}

void test_gk_clear_holds_when_ball_still_close(void) {
    GkWorldView w = gk_w();
    w.ball_visible = true;
    w.ball_x_mm = 0.0f;
    w.ball_y_mm = 300.0f;         // 250 < 300 < 400 → ni trigger ni release
    GkDecision d = gk_decide_transition(GkPhase::CLEAR, w, T_GK);
    TEST_ASSERT_EQUAL(GkPhase::CLEAR, d.next_phase);
}

void test_gk_line_avoid_and_recovery(void) {
    GkWorldView w = gk_w();
    w.imminent_exit = true;
    w.line_fresh = true;
    GkDecision d1 = gk_decide_transition(GkPhase::PATROL, w, T_GK);
    TEST_ASSERT_EQUAL(GkPhase::LINE_AVOID, d1.next_phase);

    w.imminent_exit = false;      // línea despejada
    GkDecision d2 = gk_decide_transition(GkPhase::LINE_AVOID, w, T_GK);
    TEST_ASSERT_EQUAL(GkPhase::PATROL, d2.next_phase);
}

// ============================================================================
// Runner
// ============================================================================

int main(int, char**) {
    T_ATK = atk_tuning_default();
    T_GK  = gk_tuning_default();

    UNITY_BEGIN();

    // ATK global
    RUN_TEST(test_atk_match_stopped_forces_wait_start_from_any_state);
    RUN_TEST(test_atk_match_stop_beats_imminent_exit);
    RUN_TEST(test_atk_kickoff_edge_enters_kickoff);
    RUN_TEST(test_atk_no_kickoff_when_match_already_running);
    RUN_TEST(test_atk_line_avoid_beats_kickoff);
    RUN_TEST(test_atk_imminent_exit_ignored_if_line_stale);

    // ATK kickoff timer
    RUN_TEST(test_atk_kickoff_holds_until_duration);
    RUN_TEST(test_atk_kickoff_expires_to_search);
    RUN_TEST(test_atk_kickoff_just_started_uses_now_not_stale_timestamp);

    // ATK line avoid
    RUN_TEST(test_atk_line_avoid_returns_to_search_when_clear);
    RUN_TEST(test_atk_line_avoid_holds_while_imminent);

    // ATK search
    RUN_TEST(test_atk_search_to_approach_when_ball_aligned_with_goal);
    RUN_TEST(test_atk_search_to_position_when_ball_not_aligned);
    RUN_TEST(test_atk_search_to_approach_fallback_when_no_goal);
    RUN_TEST(test_atk_search_holds_when_no_ball);

    // ATK position
    RUN_TEST(test_atk_position_to_search_when_ball_lost);
    RUN_TEST(test_atk_position_to_approach_when_goal_lost);
    RUN_TEST(test_atk_position_to_approach_when_reached_and_aligned);
    RUN_TEST(test_atk_position_holds_when_not_reached);

    // ATK approach + empuje/push alineado (campo vestigial kicker_fire)
    RUN_TEST(test_atk_approach_to_search_when_ball_lost);
    RUN_TEST(test_atk_approach_to_search_when_ball_too_close_singular);
    RUN_TEST(test_atk_approach_back_to_position_on_hysteresis_loss);
    RUN_TEST(test_atk_approach_commits_push_when_aligned_and_close);
    RUN_TEST(test_atk_approach_no_push_when_far);
    RUN_TEST(test_atk_approach_no_push_when_goal_not_visible);

    // GK
    RUN_TEST(test_gk_match_stopped_forces_wait_start);
    RUN_TEST(test_gk_wait_start_to_patrol_when_match_runs);
    RUN_TEST(test_gk_patrol_to_intercept_when_ball_seen);
    RUN_TEST(test_gk_intercept_to_clear_when_ball_close);
    RUN_TEST(test_gk_intercept_holds_when_ball_far_and_visible);
    RUN_TEST(test_gk_intercept_to_patrol_when_ball_lost_and_far);
    RUN_TEST(test_gk_clear_to_patrol_when_ball_lost_beats_release);
    RUN_TEST(test_gk_clear_to_intercept_on_release_hysteresis);
    RUN_TEST(test_gk_clear_holds_when_ball_still_close);
    RUN_TEST(test_gk_line_avoid_and_recovery);

    return UNITY_END();
}
