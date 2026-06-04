// test_pose_targeting — tests unitarios para src/shared/pose_targeting.cpp
// Corre en host con: pio test -e test_native -f test_pose_targeting
//   o vía: bash scripts/run-host-tests.sh test_pose_targeting
//
// Convención (ver pose_targeting.h / docs/CONVENCION-EJES-ROBOT.md §2):
//   marco CANCHA: +X lateral (corto, 1820), +Y arco-a-arco (largo, 2430),
//   origen esquina propia; arco propio centrado x≈910, línea de gol y≈120,
//   boca de 600 mm → postes en x=610 y x=1210. Heading 0=+Y (arco rival),
//   crece CCW (izquierda).

#include <unity.h>
#include <cmath>
#include "pose_targeting.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// dist_mm
// ============================================================================

void test_dist_zero_same_point(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, dist_mm(100.0f, 200.0f, 100.0f, 200.0f));
}

void test_dist_axis_aligned(void) {
    // (0,0) → (300,400) = 500 (triángulo 3-4-5).
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 500.0f, dist_mm(0.0f, 0.0f, 300.0f, 400.0f));
}

void test_dist_symmetric(void) {
    const float a = dist_mm(50.0f, 60.0f, 900.0f, 1200.0f);
    const float b = dist_mm(900.0f, 1200.0f, 50.0f, 60.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, a, b);
}

// ============================================================================
// aim_heading_to_point_centideg
//   heading 0 = +Y; crece CCW (izquierda). Front apunta (−sinθ, cosθ).
// ============================================================================

void test_aim_to_plus_y_is_zero(void) {
    // Target directamente al frente (+Y) → heading 0.
    int16_t h = aim_heading_to_point_centideg(910.0f, 1000.0f, 910.0f, 2000.0f);
    TEST_ASSERT_EQUAL_INT16(0, h);
}

void test_aim_to_plus_x_is_270(void) {
    // Target a la DERECHA (+X). Para mirar +X el robot gira CW (a la derecha):
    // heading = atan2(-dx, dy) = atan2(-590, 0) = -90° → 270° = 27000 centideg.
    int16_t h = aim_heading_to_point_centideg(910.0f, 1000.0f, 1500.0f, 1000.0f);
    TEST_ASSERT_INT16_WITHIN(2, 27000, h);
}

void test_aim_to_minus_x_is_90(void) {
    // Target a la IZQUIERDA (−X). heading = atan2(610, 0) = +90° = 9000 centideg.
    int16_t h = aim_heading_to_point_centideg(910.0f, 1000.0f, 300.0f, 1000.0f);
    TEST_ASSERT_INT16_WITHIN(2, 9000, h);
}

void test_aim_to_minus_y_is_180(void) {
    // Target ATRÁS (−Y). heading = atan2(0, -500) = 180° = 18000 centideg.
    int16_t h = aim_heading_to_point_centideg(910.0f, 1000.0f, 910.0f, 500.0f);
    TEST_ASSERT_INT16_WITHIN(2, 18000, h);
}

void test_aim_diagonal_front_right_is_315(void) {
    // Target adelante-derecha (dx=+1000, dy=+1000). heading = atan2(-1000,1000)
    // = -45° → 315° = 31500 centideg.
    int16_t h = aim_heading_to_point_centideg(910.0f, 1000.0f, 1910.0f, 2000.0f);
    TEST_ASSERT_INT16_WITHIN(2, 31500, h);
}

void test_aim_diagonal_front_left_is_45(void) {
    // Target adelante-izquierda (dx=-1000, dy=+1000). heading = atan2(1000,1000)
    // = +45° = 4500 centideg.
    int16_t h = aim_heading_to_point_centideg(910.0f, 1000.0f, -90.0f, 2000.0f);
    TEST_ASSERT_INT16_WITHIN(2, 4500, h);
}

void test_aim_degenerate_same_point_returns_zero(void) {
    // Target == robot → sin dirección; fallback seguro = mirar al arco (0).
    int16_t h = aim_heading_to_point_centideg(500.0f, 500.0f, 500.0f, 500.0f);
    TEST_ASSERT_EQUAL_INT16(0, h);
}

// ============================================================================
// gk_defend_point
//   default field: postes x=610 (izq) y x=1210 (der), línea de gol y=120,
//   centro x=910.
// ============================================================================

void test_gk_centered_ball_returns_center(void) {
    FieldGeometry f;  // defaults
    FieldPoint p = gk_defend_point(910.0f, 1200.0f, f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 910.0f, p.x_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 120.0f, p.y_mm);
}

void test_gk_lateral_ball_tracks_within_posts(void) {
    // Pelota desplazada a la izquierda pero dentro de la boca → cubre su x.
    FieldGeometry f;
    FieldPoint p = gk_defend_point(800.0f, 1500.0f, f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 800.0f, p.x_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 120.0f, p.y_mm);
}

void test_gk_wide_left_ball_clamps_to_left_post(void) {
    // Pelota muy a la izquierda (x=300 < poste 610) → clamp al poste izquierdo.
    FieldGeometry f;
    FieldPoint p = gk_defend_point(300.0f, 1500.0f, f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 610.0f, p.x_mm);
}

void test_gk_wide_right_ball_clamps_to_right_post(void) {
    // Pelota muy a la derecha (x=1600 > poste 1210) → clamp al poste derecho.
    FieldGeometry f;
    FieldPoint p = gk_defend_point(1600.0f, 1500.0f, f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 1210.0f, p.x_mm);
}

void test_gk_ball_behind_goal_line_returns_center(void) {
    // Pelota DETRÁS de la línea de gol (y=50 < 120) → centro del arco.
    FieldGeometry f;
    FieldPoint p = gk_defend_point(700.0f, 50.0f, f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 910.0f, p.x_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 120.0f, p.y_mm);
}

void test_gk_ball_on_goal_line_returns_center(void) {
    // Pelota EXACTAMENTE sobre la línea (ball_y == línea) → centro (degenerado).
    FieldGeometry f;
    FieldPoint p = gk_defend_point(700.0f, 120.0f, f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 910.0f, p.x_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 120.0f, p.y_mm);
}

// ============================================================================
// in_shooting_lane
//   lane de prueba: pelota (910,1500) → arco (910,120) = recta vertical x=910.
// ============================================================================

void test_lane_robot_on_line_true(void) {
    // Robot justo sobre la recta x=910 → en el carril.
    TEST_ASSERT_TRUE(in_shooting_lane(910.0f, 800.0f, 910.0f, 1500.0f, 910.0f, 120.0f, 100.0f));
}

void test_lane_robot_off_line_false(void) {
    // Robot a 190 mm lateral (x=1100) > tol 100 → fuera del carril.
    TEST_ASSERT_FALSE(in_shooting_lane(1100.0f, 800.0f, 910.0f, 1500.0f, 910.0f, 120.0f, 100.0f));
}

void test_lane_robot_near_line_within_tol_true(void) {
    // Robot a 50 mm lateral (x=960) ≤ tol 100 → dentro del carril.
    TEST_ASSERT_TRUE(in_shooting_lane(960.0f, 800.0f, 910.0f, 1500.0f, 910.0f, 120.0f, 100.0f));
}

void test_lane_robot_beyond_goal_endpoint_false(void) {
    // Robot por detrás del arco (y=-200, más allá del extremo goal y=120). La
    // proyección se clampa al extremo → distancia al arco = 320 mm > tol → false.
    TEST_ASSERT_FALSE(in_shooting_lane(910.0f, -200.0f, 910.0f, 1500.0f, 910.0f, 120.0f, 100.0f));
}

void test_lane_diagonal_segment(void) {
    // Carril diagonal: pelota (600,1500) → arco (910,120). Robot sobre el punto
    // medio del segmento → distancia ~0 → en el carril.
    const float mid_x = (600.0f + 910.0f) * 0.5f;
    const float mid_y = (1500.0f + 120.0f) * 0.5f;
    TEST_ASSERT_TRUE(in_shooting_lane(mid_x, mid_y, 600.0f, 1500.0f, 910.0f, 120.0f, 30.0f));
}

void test_lane_degenerate_ball_equals_goal(void) {
    // Pelota y arco coinciden (segmento de longitud 0) → distancia punto-a-punto.
    // Robot a 20 mm → dentro de tol 100.
    TEST_ASSERT_TRUE(in_shooting_lane(930.0f, 120.0f, 910.0f, 120.0f, 910.0f, 120.0f, 100.0f));
    // Robot a 280 mm → fuera.
    TEST_ASSERT_FALSE(in_shooting_lane(910.0f, 400.0f, 910.0f, 120.0f, 910.0f, 120.0f, 100.0f));
}

// ============================================================================
// Runner
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();

    // dist_mm
    RUN_TEST(test_dist_zero_same_point);
    RUN_TEST(test_dist_axis_aligned);
    RUN_TEST(test_dist_symmetric);

    // aim_heading_to_point_centideg
    RUN_TEST(test_aim_to_plus_y_is_zero);
    RUN_TEST(test_aim_to_plus_x_is_270);
    RUN_TEST(test_aim_to_minus_x_is_90);
    RUN_TEST(test_aim_to_minus_y_is_180);
    RUN_TEST(test_aim_diagonal_front_right_is_315);
    RUN_TEST(test_aim_diagonal_front_left_is_45);
    RUN_TEST(test_aim_degenerate_same_point_returns_zero);

    // gk_defend_point
    RUN_TEST(test_gk_centered_ball_returns_center);
    RUN_TEST(test_gk_lateral_ball_tracks_within_posts);
    RUN_TEST(test_gk_wide_left_ball_clamps_to_left_post);
    RUN_TEST(test_gk_wide_right_ball_clamps_to_right_post);
    RUN_TEST(test_gk_ball_behind_goal_line_returns_center);
    RUN_TEST(test_gk_ball_on_goal_line_returns_center);

    // in_shooting_lane
    RUN_TEST(test_lane_robot_on_line_true);
    RUN_TEST(test_lane_robot_off_line_false);
    RUN_TEST(test_lane_robot_near_line_within_tol_true);
    RUN_TEST(test_lane_robot_beyond_goal_endpoint_false);
    RUN_TEST(test_lane_diagonal_segment);
    RUN_TEST(test_lane_degenerate_ball_equals_goal);

    return UNITY_END();
}
