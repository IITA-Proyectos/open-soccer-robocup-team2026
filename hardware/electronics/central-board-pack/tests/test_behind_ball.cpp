// test_behind_ball — tests unitarios para src/shared/behind_ball.cpp
// Corre en host con: pio test -e test_native -f test_behind_ball

#include <unity.h>
#include <cmath>
#include "behind_ball.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

constexpr float GAP_MM = 120.0f;

// ============================================================================
// compute_behind_ball_target
// ============================================================================

void test_target_ball_in_front_goal_in_front(void) {
    // Pelota al frente (0, 500). Arco al frente (θ=0). Target detrás de la
    // pelota mirando al arco = (0, 500 - GAP).
    BehindBallTarget t = compute_behind_ball_target(0.0f, 500.0f, 0.0f, GAP_MM);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, t.target_x_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 500.0f - GAP_MM, t.target_y_mm);
}

void test_target_ball_in_front_goal_to_right(void) {
    // Pelota al frente (0, 500). Arco a 90° derecha (θ=90°). Vector u al arco
    // = (sin 90°, cos 90°) = (1, 0). Target = pelota - GAP*u = (-GAP, 500).
    BehindBallTarget t = compute_behind_ball_target(0.0f, 500.0f, 90.0f, GAP_MM);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, -GAP_MM, t.target_x_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 500.0f, t.target_y_mm);
}

void test_target_ball_to_right_goal_in_front(void) {
    // Pelota a la derecha (300, 0). Arco al frente (θ=0). u = (0, 1).
    // Target = (300, 0) - GAP*(0,1) = (300, -GAP).
    BehindBallTarget t = compute_behind_ball_target(300.0f, 0.0f, 0.0f, GAP_MM);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 300.0f, t.target_x_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, -GAP_MM, t.target_y_mm);
}

void test_target_zero_gap_returns_ball_position(void) {
    // gap=0 → target = pelota.
    BehindBallTarget t = compute_behind_ball_target(150.0f, 200.0f, 45.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 150.0f, t.target_x_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 200.0f, t.target_y_mm);
}

void test_target_goal_behind_robot_pulls_target_forward(void) {
    // Pelota al frente (0, 400). Arco DETRÁS del robot (θ=180°). u =
    // (sin 180°, cos 180°) = (0, -1). Target = (0, 400) - GAP*(0,-1) = (0, 400+GAP).
    // O sea: el robot tendría que pasar de largo la pelota para quedar
    // alineado (lo cual es absurdo, pero matemáticamente correcto).
    BehindBallTarget t = compute_behind_ball_target(0.0f, 400.0f, 180.0f, GAP_MM);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, t.target_x_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 400.0f + GAP_MM, t.target_y_mm);
}

// ============================================================================
// is_aligned_to_shoot
// ============================================================================

void test_aligned_close_and_front_returns_true(void) {
    // Pelota a 50 mm al frente, arco al frente → alineado.
    TEST_ASSERT_TRUE(is_aligned_to_shoot(0.0f, 50.0f, 0.0f, /*kick_dist=*/80.0f, /*kick_ang=*/12.0f));
}

void test_aligned_too_far_returns_false(void) {
    // Pelota a 200 mm — más lejos que kick_dist=80.
    TEST_ASSERT_FALSE(is_aligned_to_shoot(0.0f, 200.0f, 0.0f, 80.0f, 12.0f));
}

void test_aligned_goal_off_axis_returns_false(void) {
    // Pelota cerca (50 mm) pero arco a 30° → no alineado para patear.
    TEST_ASSERT_FALSE(is_aligned_to_shoot(0.0f, 50.0f, 30.0f, 80.0f, 12.0f));
}

void test_aligned_at_threshold_passes(void) {
    // Exactamente en el threshold (angle=12, dist=80).
    TEST_ASSERT_TRUE(is_aligned_to_shoot(0.0f, 80.0f, 12.0f, 80.0f, 12.0f));
}

void test_aligned_negative_angle_uses_abs(void) {
    // Arco a -10° → dentro de tolerancia ±12°.
    TEST_ASSERT_TRUE(is_aligned_to_shoot(20.0f, 50.0f, -10.0f, 80.0f, 12.0f));
}

// ============================================================================
// ball_is_in_attack_line
// ============================================================================

void test_attack_line_ball_aligned_with_goal_in_front(void) {
    // Pelota al frente (0, 400), arco al frente (θ=0). atan2(0,400)=0,
    // diff=0 → alineado.
    TEST_ASSERT_TRUE(ball_is_in_attack_line(0.0f, 400.0f, 0.0f, 30.0f));
}

void test_attack_line_ball_at_30_goal_at_0(void) {
    // Pelota en (200, 200): atan2(200, 200) ≈ 45°. Arco a 0°. Diff = 45° >
    // tolerancia 30° → NO alineado.
    TEST_ASSERT_FALSE(ball_is_in_attack_line(200.0f, 200.0f, 0.0f, 30.0f));
}

void test_attack_line_ball_and_goal_both_to_side(void) {
    // Pelota a 90° derecha (300, 0). Arco también a ~90° (θ=90°). Diff = 0 →
    // alineado.
    TEST_ASSERT_TRUE(ball_is_in_attack_line(300.0f, 0.0f, 90.0f, 30.0f));
}

void test_attack_line_wraparound(void) {
    // Pelota a 170° (-100, -500 → atan2(-100,-500) ≈ -169°). Arco a -170°.
    // Diff = 1° → alineado (no -339°, gracias al wrap).
    TEST_ASSERT_TRUE(ball_is_in_attack_line(-100.0f, -500.0f, -170.0f, 30.0f));
}

// ============================================================================
// compute_kickoff_velocity
// ============================================================================

void test_kickoff_velocity_only_forward(void) {
    // Kickoff: el robot avanza solo al frente, sin componente lateral.
    KickoffVelocity v = compute_kickoff_velocity(500.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, v.vx_mm_s);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 500.0f, v.vy_mm_s);
}

void test_kickoff_velocity_scales_with_speed(void) {
    KickoffVelocity v1 = compute_kickoff_velocity(100.0f);
    KickoffVelocity v2 = compute_kickoff_velocity(800.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, v1.vy_mm_s);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 800.0f, v2.vy_mm_s);
}

// ============================================================================
// Runner
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();

    // compute_behind_ball_target
    RUN_TEST(test_target_ball_in_front_goal_in_front);
    RUN_TEST(test_target_ball_in_front_goal_to_right);
    RUN_TEST(test_target_ball_to_right_goal_in_front);
    RUN_TEST(test_target_zero_gap_returns_ball_position);
    RUN_TEST(test_target_goal_behind_robot_pulls_target_forward);

    // is_aligned_to_shoot
    RUN_TEST(test_aligned_close_and_front_returns_true);
    RUN_TEST(test_aligned_too_far_returns_false);
    RUN_TEST(test_aligned_goal_off_axis_returns_false);
    RUN_TEST(test_aligned_at_threshold_passes);
    RUN_TEST(test_aligned_negative_angle_uses_abs);

    // ball_is_in_attack_line
    RUN_TEST(test_attack_line_ball_aligned_with_goal_in_front);
    RUN_TEST(test_attack_line_ball_at_30_goal_at_0);
    RUN_TEST(test_attack_line_ball_and_goal_both_to_side);
    RUN_TEST(test_attack_line_wraparound);

    // compute_kickoff_velocity
    RUN_TEST(test_kickoff_velocity_only_forward);
    RUN_TEST(test_kickoff_velocity_scales_with_speed);

    return UNITY_END();
}
