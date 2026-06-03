// test_otos_position — tests unitarios para el módulo otos_position (BUILD-OTOS).
// Corre en host con: bash scripts/run-host-tests.sh test_otos_position
//   o (dirigido, durante TDD) con g++ apuntando a src/shared/otos_position.cpp,
//   test/test_otos_position/test_main.cpp y lib/Unity/src/unity.c.
//
// Contrato (definido por estos tests, TDD-first):
//   Marco robot: +Y adelante, +X derecha. Heading en grados, CCW+.
//   set_target: target_world = cur_pos + R(cur_heading) * (dx,dy)_robot.
//   update:     err_world  = target_world - cur_pos
//               remaining  = hypot(err_world)
//               err_robot  = R(-cur_heading) * err_world
//               si remaining < tol -> arrived, vx=vy=0
//               si no: speed = clamp(min, max, max*remaining/decel_zone)
//                      vx = speed * err_robot_x / remaining
//                      vy = speed * err_robot_y / remaining

#include <unity.h>
#include <cmath>
#include "otos_position.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// (a) target dy=500 desde (0,0,heading 0): al inicio vy>0, vx~0, no arrived;
//     al llegar a (0,500,0) -> arrived, v=0.
// ============================================================================

void test_forward_target_initial_then_arrive(void) {
    OtosMoveParams p = otos_move_default_params();
    OtosMoveTarget t;
    otos_move_set_target(t, 0.0f, 0.0f, 0.0f, /*dx*/0.0f, /*dy*/500.0f);
    TEST_ASSERT_TRUE(t.active);

    // Al inicio, lejos del target: vy > 0 (avanza adelante), vx ~ 0, no llegó.
    OtosMoveCmd c0 = otos_move_update(t, p, 0.0f, 0.0f, 0.0f);
    TEST_ASSERT_FALSE(c0.arrived);
    TEST_ASSERT_TRUE(c0.vy_mm_s > 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, c0.vx_mm_s);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 500.0f, c0.remaining_mm);

    // Llegando exactamente a (0,500): arrived, v=0.
    OtosMoveCmd c1 = otos_move_update(t, p, 0.0f, 500.0f, 0.0f);
    TEST_ASSERT_TRUE(c1.arrived);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, c1.vx_mm_s);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, c1.vy_mm_s);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, c1.remaining_mm);
}

// ============================================================================
// (b) target dx=300 (strafe a la derecha) desde (0,0,heading 0): vx>0, vy~0.
// ============================================================================

void test_strafe_right_target_vx_positive(void) {
    OtosMoveParams p = otos_move_default_params();
    OtosMoveTarget t;
    otos_move_set_target(t, 0.0f, 0.0f, 0.0f, /*dx*/300.0f, /*dy*/0.0f);

    OtosMoveCmd c = otos_move_update(t, p, 0.0f, 0.0f, 0.0f);
    TEST_ASSERT_FALSE(c.arrived);
    TEST_ASSERT_TRUE(c.vx_mm_s > 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, c.vy_mm_s);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 300.0f, c.remaining_mm);
}

// ============================================================================
// (c) rampa de frenado: cerca del target (dentro de decel_zone) la speed baja
//     proporcional a remaining.
// ============================================================================

void test_decel_ramp_speed_scales_with_remaining(void) {
    OtosMoveParams p = otos_move_default_params();  // max=600, decel_zone=300, tol=15, min=60
    OtosMoveTarget t;
    otos_move_set_target(t, 0.0f, 0.0f, 0.0f, 0.0f, 500.0f);

    // A remaining=200 (dentro de la zona de 300): speed = 600*200/300 = 400.
    // pos=(0,300) -> remaining = 500-300 = 200.
    OtosMoveCmd c = otos_move_update(t, p, 0.0f, 300.0f, 0.0f);
    TEST_ASSERT_FALSE(c.arrived);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 200.0f, c.remaining_mm);
    // Toda la velocidad es vy (avance puro), magnitud = speed esperado.
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 400.0f, c.vy_mm_s);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, c.vx_mm_s);

    // A remaining=100: speed = 600*100/300 = 200.
    OtosMoveCmd c2 = otos_move_update(t, p, 0.0f, 400.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 100.0f, c2.remaining_mm);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 200.0f, c2.vy_mm_s);
    // Más cerca => más lento.
    TEST_ASSERT_TRUE(c2.vy_mm_s < c.vy_mm_s);
}

void test_decel_ramp_clamps_to_min_speed(void) {
    OtosMoveParams p = otos_move_default_params();  // min=60
    OtosMoveTarget t;
    otos_move_set_target(t, 0.0f, 0.0f, 0.0f, 0.0f, 500.0f);

    // A remaining=20 (> tol=15, pero muy cerca): speed cruda = 600*20/300 = 40 < min(60)
    // -> debe clampear a min=60.
    OtosMoveCmd c = otos_move_update(t, p, 0.0f, 480.0f, 0.0f);
    TEST_ASSERT_FALSE(c.arrived);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 20.0f, c.remaining_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 60.0f, c.vy_mm_s);
}

// ============================================================================
// (d) clamp a max_speed lejos del target.
// ============================================================================

void test_far_target_clamps_to_max_speed(void) {
    OtosMoveParams p = otos_move_default_params();  // max=600, decel_zone=300
    OtosMoveTarget t;
    // dy=2000: muy lejos. speed cruda = 600*2000/300 = 4000 -> clamp a max=600.
    otos_move_set_target(t, 0.0f, 0.0f, 0.0f, 0.0f, 2000.0f);

    OtosMoveCmd c = otos_move_update(t, p, 0.0f, 0.0f, 0.0f);
    TEST_ASSERT_FALSE(c.arrived);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 2000.0f, c.remaining_mm);
    // Avance puro: vy == max, vx ~ 0.
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 600.0f, c.vy_mm_s);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, c.vx_mm_s);
    // La norma del comando == max.
    float speed = std::sqrt(c.vx_mm_s * c.vx_mm_s + c.vy_mm_s * c.vy_mm_s);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 600.0f, speed);
}

// ============================================================================
// (e) tolerancia de llegada.
// ============================================================================

void test_arrival_tolerance(void) {
    OtosMoveParams p = otos_move_default_params();  // tol=15
    OtosMoveTarget t;
    otos_move_set_target(t, 0.0f, 0.0f, 0.0f, 0.0f, 500.0f);

    // Dentro de tol (remaining=10 < 15): arrived, v=0.
    OtosMoveCmd in = otos_move_update(t, p, 0.0f, 490.0f, 0.0f);
    TEST_ASSERT_TRUE(in.arrived);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, in.vx_mm_s);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, in.vy_mm_s);

    // Fuera de tol (remaining=30 > 15): NO arrived, se mueve.
    OtosMoveCmd out = otos_move_update(t, p, 0.0f, 470.0f, 0.0f);
    TEST_ASSERT_FALSE(out.arrived);
    TEST_ASSERT_TRUE(out.vy_mm_s > 0.0f);
}

// ============================================================================
// (f) heading=90deg: conversión marco mundo <-> robot.
//     Un dx robot (+derecha) a heading 90 mueve en +Y mundo.
//     R(90): x_world = x_r*cos90 - y_r*sin90 ; y_world = x_r*sin90 + y_r*cos90.
//     Con (dx=300, dy=0): x_world = -0 ... cos90=0,sin90=1 ->
//       x_world = 300*0 - 0*1 = 0 ; y_world = 300*1 + 0*0 = 300.
//     => target_world = (0, 300).
// ============================================================================

void test_heading_90_dx_maps_to_world_y(void) {
    OtosMoveParams p = otos_move_default_params();
    OtosMoveTarget t;
    otos_move_set_target(t, 0.0f, 0.0f, /*heading*/90.0f, /*dx*/300.0f, /*dy*/0.0f);

    // El target absoluto en mundo es (0, 300).
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, t.tx_world_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 300.0f, t.ty_world_mm);

    // Estando aún en (0,0) con heading 90, el comando en MARCO ROBOT debe ser
    // un avance lateral derecho puro: vx>0 (el +x robot apunta a +y mundo),
    // vy ~ 0. remaining = 300.
    OtosMoveCmd c = otos_move_update(t, p, 0.0f, 0.0f, 90.0f);
    TEST_ASSERT_FALSE(c.arrived);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 300.0f, c.remaining_mm);
    TEST_ASSERT_TRUE(c.vx_mm_s > 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, c.vy_mm_s);
}

void test_heading_90_dy_maps_to_world_negative_x(void) {
    // Con dy robot (+adelante) a heading 90:
    //   x_world = 0*cos90 - 500*sin90 = -500 ; y_world = 0*sin90 + 500*cos90 = 0.
    OtosMoveParams p = otos_move_default_params();
    OtosMoveTarget t;
    otos_move_set_target(t, 0.0f, 0.0f, 90.0f, /*dx*/0.0f, /*dy*/500.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.5f, -500.0f, t.tx_world_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, t.ty_world_mm);

    // De vuelta en marco robot a heading 90, el error mundo (-500,0) se ve como
    // un avance adelante puro: vy>0, vx ~ 0.
    OtosMoveCmd c = otos_move_update(t, p, 0.0f, 0.0f, 90.0f);
    TEST_ASSERT_TRUE(c.vy_mm_s > 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, c.vx_mm_s);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 500.0f, c.remaining_mm);
}

// ============================================================================
// Extra: set_target con offset de pose no nulo (target absoluto = cur + delta).
// ============================================================================

void test_set_target_uses_current_pose_offset(void) {
    OtosMoveTarget t;
    // Desde (100,200,heading 0): dx=0,dy=500 -> target = (100, 700).
    otos_move_set_target(t, 100.0f, 200.0f, 0.0f, 0.0f, 500.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 100.0f, t.tx_world_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 700.0f, t.ty_world_mm);
}

// ============================================================================
// Extra: cancel desactiva el target.
// ============================================================================

void test_cancel_clears_target(void) {
    OtosMoveTarget t;
    otos_move_set_target(t, 0.0f, 0.0f, 0.0f, 0.0f, 500.0f);
    TEST_ASSERT_TRUE(t.active);
    otos_move_cancel(t);
    TEST_ASSERT_FALSE(t.active);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_forward_target_initial_then_arrive);
    RUN_TEST(test_strafe_right_target_vx_positive);
    RUN_TEST(test_decel_ramp_speed_scales_with_remaining);
    RUN_TEST(test_decel_ramp_clamps_to_min_speed);
    RUN_TEST(test_far_target_clamps_to_max_speed);
    RUN_TEST(test_arrival_tolerance);
    RUN_TEST(test_heading_90_dx_maps_to_world_y);
    RUN_TEST(test_heading_90_dy_maps_to_world_negative_x);
    RUN_TEST(test_set_target_uses_current_pose_offset);
    RUN_TEST(test_cancel_clears_target);

    return UNITY_END();
}
