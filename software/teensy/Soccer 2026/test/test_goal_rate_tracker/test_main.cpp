// test_goal_rate_tracker — tasa de rotación inferida del bearing al arco (TASK-212 Fase 1).
// Corre host: bash scripts/run-host-tests.sh test_goal_rate_tracker
//
// Cubre lo que el review adversarial marcó como CRÍTICO (NO es espejo de ball_velocity):
//   • wraparound ±180 → resta envuelta (no salto falso de 360°/s),
//   • salto físicamente imposible → descartado (glitch / cambio de arco),
//   • visibilidad CONTINUA ≥2 frames para validar (ref única de R2, sin picos),
//   • gap de dt → re-siembra (no deriva contra una muestra vieja).

#include <unity.h>
#include "goal_rate_tracker.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Helper de la resta angular envuelta.
void test_wrap_centideg(void) {
    TEST_ASSERT_EQUAL_INT32(0,    grt_wrap_centideg(0));
    TEST_ASSERT_EQUAL_INT32(200,  grt_wrap_centideg(-35800));  // +179 → -179 = +2°, no -358°
    TEST_ASSERT_EQUAL_INT32(-200, grt_wrap_centideg(35800));
    TEST_ASSERT_EQUAL_INT32(18000, grt_wrap_centideg(18000));  // límite no envuelve
    TEST_ASSERT_EQUAL_INT32(-17999, grt_wrap_centideg(18001)); // recién pasado envuelve
}

// Zero-init: primer frame visible solo SIEMBRA (sin tasa todavía).
void test_first_frame_seeds(void) {
    GoalRateTracker t{};
    GoalRateParams p = goal_rate_default_params();
    GoalRateResult r = goal_rate_update(t, 0, true, 0, p);
    TEST_ASSERT_FALSE(r.valid);
    TEST_ASSERT_TRUE(t.seen);
    TEST_ASSERT_EQUAL_UINT8(1, t.visible_streak);
}

// Rotación estable: 2º frame visible da tasa = −Δbearing/Δt.
void test_steady_rotation_rate(void) {
    GoalRateTracker t{};
    GoalRateParams p = goal_rate_default_params();
    uint32_t now = 0;
    goal_rate_update(t, 0, true, now, p); now += 100;        // siembra en 0
    GoalRateResult r = goal_rate_update(t, 1000, true, now, p);  // +1000cdeg=+10° en 100ms
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, -100.0f, r.rate_dps);    // −10°/0.1s = −100 dps
}

// WRAPAROUND ±180: de +179° a −179° NO es un salto de 360°/s.
void test_wraparound_no_false_jump(void) {
    GoalRateTracker t{};
    GoalRateParams p = goal_rate_default_params();
    uint32_t now = 0;
    goal_rate_update(t, 17900, true, now, p); now += 100;   // +179°
    GoalRateResult r = goal_rate_update(t, -17900, true, now, p);  // −179°: Δ envuelto = +2°
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, -20.0f, r.rate_dps);     // −2°/0.1s = −20 dps, NO ±3580
}

// Salto físicamente imposible (>max_jump) → glitch → descartado (no valid).
void test_max_jump_gate(void) {
    GoalRateTracker t{};
    GoalRateParams p = goal_rate_default_params();
    uint32_t now = 0;
    goal_rate_update(t, 0, true, now, p); now += 10;
    GoalRateResult r = goal_rate_update(t, 10000, true, now, p);  // 100° en 10ms = 10000 dps >> 720
    TEST_ASSERT_FALSE(r.valid);
    TEST_ASSERT_EQUAL_UINT8(1, t.visible_streak);   // racha reiniciada (no confiar)
}

// Pérdida de visibilidad reinicia; re-adquirir exige 2 frames de nuevo.
void test_visibility_loss_resets(void) {
    GoalRateTracker t{};
    GoalRateParams p = goal_rate_default_params();
    uint32_t now = 0;
    goal_rate_update(t, 0, true, now, p);   now += 100;
    GoalRateResult ok = goal_rate_update(t, 500, true, now, p); now += 100;
    TEST_ASSERT_TRUE(ok.valid);
    GoalRateResult lost = goal_rate_update(t, 0, false, now, p);  // no visible
    TEST_ASSERT_FALSE(lost.valid);
    TEST_ASSERT_FALSE(t.seen);
    TEST_ASSERT_EQUAL_UINT8(0, t.visible_streak);
    now += 100;
    GoalRateResult re = goal_rate_update(t, 700, true, now, p);   // re-adquiere: re-siembra
    TEST_ASSERT_FALSE(re.valid);
}

// Gap de dt (> max_dt) → re-siembra, no deriva contra una muestra vieja.
void test_dt_gap_reseeds(void) {
    GoalRateTracker t{};
    GoalRateParams p = goal_rate_default_params();
    uint32_t now = 0;
    goal_rate_update(t, 0, true, now, p);   now += 100;
    goal_rate_update(t, 500, true, now, p); now += 500;   // gap 500ms > max_dt 200
    GoalRateResult r = goal_rate_update(t, 600, true, now, p);
    TEST_ASSERT_FALSE(r.valid);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_wrap_centideg);
    RUN_TEST(test_first_frame_seeds);
    RUN_TEST(test_steady_rotation_rate);
    RUN_TEST(test_wraparound_no_false_jump);
    RUN_TEST(test_max_jump_gate);
    RUN_TEST(test_visibility_loss_resets);
    RUN_TEST(test_dt_gap_reseeds);
    return UNITY_END();
}
