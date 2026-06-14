// test_cameras_fusion — tests unitarios para src/shared/cameras_fusion.cpp
// Corre en host con: pio test -e test_native -f test_cameras_fusion

#include <unity.h>
#include <cmath>
#include "cameras_fusion.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

constexpr float UNIT_TO_MM = 10.0f;  // mismo factor que usa cameras_runtime
constexpr float PI_F = 3.14159265358979323846f;

// ============================================================================
// cam_obs_to_robot_frame
// ============================================================================

void test_to_robot_frame_front_camera_passes_through(void) {
    // Cámara frontal: las coords se multiplican por UNIT_TO_MM sin invertir.
    CamObs o = cam_obs_to_robot_frame(/*x=*/30, /*y=*/50, /*visible=*/true,
                                       /*cam_id=*/0, UNIT_TO_MM);
    TEST_ASSERT_EQUAL_UINT8(0, o.camera_id);
    TEST_ASSERT_TRUE(o.visible);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 300.0f, o.x_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 500.0f, o.y_mm);
}

void test_to_robot_frame_back_camera_rotates_180(void) {
    // Cámara trasera: el +y de la cámara apunta al -y del robot. Se invierte
    // signo en X e Y.
    CamObs o = cam_obs_to_robot_frame(/*x=*/30, /*y=*/50, /*visible=*/true,
                                       /*cam_id=*/1, UNIT_TO_MM);
    TEST_ASSERT_EQUAL_UINT8(1, o.camera_id);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -300.0f, o.x_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -500.0f, o.y_mm);
}

void test_to_robot_frame_preserves_visible_flag(void) {
    CamObs visible = cam_obs_to_robot_frame(10, 10, true, 0, UNIT_TO_MM);
    CamObs hidden  = cam_obs_to_robot_frame(10, 10, false, 0, UNIT_TO_MM);
    TEST_ASSERT_TRUE(visible.visible);
    TEST_ASSERT_FALSE(hidden.visible);
}

void test_to_robot_frame_zero_coords(void) {
    CamObs o = cam_obs_to_robot_frame(0, 0, true, 1, UNIT_TO_MM);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, o.x_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, o.y_mm);
}

// ============================================================================
// fuse_ball_dual — escenarios de visibilidad
// ============================================================================

void test_fuse_ball_both_dead_returns_invisible(void) {
    CamObs f = cam_obs_to_robot_frame(50, 50, true, 0, UNIT_TO_MM);
    CamObs b = cam_obs_to_robot_frame(50, 50, true, 1, UNIT_TO_MM);
    BallFused out = fuse_ball_dual(f, b, /*front_alive=*/false, /*back_alive=*/false);
    TEST_ASSERT_FALSE(out.visible);
    TEST_ASSERT_EQUAL_UINT8(0, out.confidence);
    TEST_ASSERT_EQUAL_INT16(0, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(0, out.y_mm);
}

void test_fuse_ball_only_front_alive_uses_front(void) {
    CamObs f = cam_obs_to_robot_frame(40, 60, true, 0, UNIT_TO_MM);
    CamObs b = cam_obs_to_robot_frame(99, 99, true, 1, UNIT_TO_MM);  // ignored
    BallFused out = fuse_ball_dual(f, b, true, false);
    TEST_ASSERT_TRUE(out.visible);
    TEST_ASSERT_EQUAL_INT16(400, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(600, out.y_mm);
}

void test_fuse_ball_only_back_alive_uses_back(void) {
    CamObs f = cam_obs_to_robot_frame(99, 99, true, 0, UNIT_TO_MM);  // ignored
    CamObs b = cam_obs_to_robot_frame(40, 60, true, 1, UNIT_TO_MM);
    BallFused out = fuse_ball_dual(f, b, false, true);
    TEST_ASSERT_TRUE(out.visible);
    // back rota 180° → x=40→-400, y=60→-600
    TEST_ASSERT_EQUAL_INT16(-400, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(-600, out.y_mm);
}

void test_fuse_ball_neither_visible_returns_invisible(void) {
    CamObs f = cam_obs_to_robot_frame(0, 0, false, 0, UNIT_TO_MM);
    CamObs b = cam_obs_to_robot_frame(0, 0, false, 1, UNIT_TO_MM);
    BallFused out = fuse_ball_dual(f, b, true, true);
    TEST_ASSERT_FALSE(out.visible);
}

void test_fuse_ball_both_visible_averages(void) {
    // Front ve la pelota en (200, 300). Back también la "ve" en su frame y
    // después de rotar 180° apunta a (-200, -300). Como las cámaras están en
    // orientaciones distintas y NO deberían coincidir físicamente, este test
    // solo verifica que el algoritmo promedie correctamente las dos
    // observaciones (que en la práctica viven en el mismo sistema robot).
    CamObs f = cam_obs_to_robot_frame(20, 30, true, 0, UNIT_TO_MM);   // → (200, 300)
    // back con coords que rotadas 180° apuntan a (200, 300) también: x=-20, y=-30
    CamObs b = cam_obs_to_robot_frame(-20, -30, true, 1, UNIT_TO_MM); // → (200, 300)
    BallFused out = fuse_ball_dual(f, b, true, true);
    TEST_ASSERT_TRUE(out.visible);
    TEST_ASSERT_EQUAL_INT16(200, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(300, out.y_mm);
    TEST_ASSERT_TRUE(out.confidence > 80);  // bonus por consenso
}

void test_fuse_ball_alive_but_not_visible(void) {
    // Cámara está alive (responde) pero el packet dice ball_visible=false.
    // No debe contar para la fusión.
    CamObs f = cam_obs_to_robot_frame(99, 99, /*visible=*/false, 0, UNIT_TO_MM);
    CamObs b = cam_obs_to_robot_frame(50, 50, /*visible=*/true,  1, UNIT_TO_MM);
    BallFused out = fuse_ball_dual(f, b, true, true);
    TEST_ASSERT_TRUE(out.visible);
    // Solo back contribuye, rotado: x=50→-500, y=50→-500
    TEST_ASSERT_EQUAL_INT16(-500, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(-500, out.y_mm);
}

// ============================================================================
// fuse_goal_dual
// ============================================================================

void test_fuse_goal_invisible_when_no_camera(void) {
    CamObs f = cam_obs_to_robot_frame(10, 10, true, 0, UNIT_TO_MM);
    CamObs b = cam_obs_to_robot_frame(10, 10, true, 1, UNIT_TO_MM);
    GoalFused out = fuse_goal_dual(f, b, false, false);
    TEST_ASSERT_FALSE(out.visible);
    TEST_ASSERT_EQUAL_INT16(0, out.angle_centideg);
    TEST_ASSERT_EQUAL_INT16(0, out.distance_mm);
}

void test_fuse_goal_front_only_angle_at_zero_means_in_front(void) {
    // Arco justo al frente (x=0, y=positivo) → ángulo 0.
    CamObs f = cam_obs_to_robot_frame(0, 50, true, 0, UNIT_TO_MM);   // → (0, 500)
    CamObs b = cam_obs_to_robot_frame(0, 0, false, 1, UNIT_TO_MM);
    GoalFused out = fuse_goal_dual(f, b, true, true);
    TEST_ASSERT_TRUE(out.visible);
    TEST_ASSERT_INT16_WITHIN(10, 0, out.angle_centideg);   // ~0 centideg
    TEST_ASSERT_INT16_WITHIN(5, 500, out.distance_mm);
}

void test_fuse_goal_lateral_90_centideg(void) {
    // Arco a la derecha pura (x positivo, y=0) → ángulo +90° (+9000 centideg).
    CamObs f = cam_obs_to_robot_frame(50, 0, true, 0, UNIT_TO_MM);   // → (500, 0)
    CamObs b = cam_obs_to_robot_frame(0, 0, false, 1, UNIT_TO_MM);
    GoalFused out = fuse_goal_dual(f, b, true, false);
    TEST_ASSERT_TRUE(out.visible);
    TEST_ASSERT_INT16_WITHIN(10, 9000, out.angle_centideg);
    TEST_ASSERT_INT16_WITHIN(5, 500, out.distance_mm);
}

void test_fuse_goal_back_camera_inverts_angle(void) {
    // Arco visto por la cámara trasera en su frente (x=0, y=50). En el frame
    // del robot eso es atrás (x=0, y=-500) → angle = atan2(0, -500) = ±180°.
    CamObs f = cam_obs_to_robot_frame(0, 0, false, 0, UNIT_TO_MM);
    CamObs b = cam_obs_to_robot_frame(0, 50, true, 1, UNIT_TO_MM);   // → (0, -500)
    GoalFused out = fuse_goal_dual(f, b, false, true);
    TEST_ASSERT_TRUE(out.visible);
    TEST_ASSERT_TRUE(std::abs(out.angle_centideg) >= 17900);  // cerca de ±180°
    TEST_ASSERT_INT16_WITHIN(5, 500, out.distance_mm);
}

void test_fuse_goal_both_visible_averages(void) {
    // Front en (300, 400), back rotado a (300, 400) también → promedio (300, 400).
    CamObs f = cam_obs_to_robot_frame(30, 40, true, 0, UNIT_TO_MM);   // → (300, 400)
    CamObs b = cam_obs_to_robot_frame(-30, -40, true, 1, UNIT_TO_MM); // → (300, 400)
    GoalFused out = fuse_goal_dual(f, b, true, true);
    TEST_ASSERT_TRUE(out.visible);
    // distancia = sqrt(300² + 400²) = 500
    TEST_ASSERT_INT16_WITHIN(5, 500, out.distance_mm);
    // ángulo atan2(300, 400) ≈ 36.87° = 3687 centideg
    TEST_ASSERT_INT16_WITHIN(10, 3687, out.angle_centideg);
}

// ============================================================================
// Watchdog: alive=false ignora data del packet
// ============================================================================

void test_fuse_ignores_dead_camera_with_visible_packet(void) {
    // Caso: la cámara dejó de responder, pero su último packet decía visible=true.
    // El watchdog (alive=false) debe ignorar esos datos stale.
    CamObs f_stale = cam_obs_to_robot_frame(99, 99, /*visible=*/true, 0, UNIT_TO_MM);
    CamObs b_fresh = cam_obs_to_robot_frame(10, 10, /*visible=*/true, 1, UNIT_TO_MM);
    BallFused out = fuse_ball_dual(f_stale, b_fresh, /*front_alive=*/false, /*back_alive=*/true);
    TEST_ASSERT_TRUE(out.visible);
    // Solo back (rotado 180°): 10*10 → -100
    TEST_ASSERT_EQUAL_INT16(-100, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(-100, out.y_mm);
}

// ============================================================================
// Edge cases adicionales (ítem E del backlog de visión)
// ============================================================================

void test_fuse_goal_left_is_negative_angle(void) {
    // Arco a la IZQUIERDA pura (x negativo, y=0) → ángulo -90° (-9000 centideg).
    // Convención: +x=DERECHA → -x=IZQUIERDA → atan2(-x, 0) < 0. Conecta con el
    // análisis del eje X (research/in-progress/2026-06-03-eje-x-...).
    CamObs f = cam_obs_to_robot_frame(-50, 0, true, 0, UNIT_TO_MM);  // → (-500, 0)
    CamObs b = cam_obs_to_robot_frame(0, 0, false, 1, UNIT_TO_MM);
    GoalFused out = fuse_goal_dual(f, b, true, false);
    TEST_ASSERT_TRUE(out.visible);
    TEST_ASSERT_INT16_WITHIN(10, -9000, out.angle_centideg);
    TEST_ASSERT_INT16_WITHIN(5, 500, out.distance_mm);
}

void test_fuse_ball_both_visible_distinct_points_true_average(void) {
    // Front ve (200, 300); back (rotado) apunta a (100, 100). Promedio → (150, 200).
    CamObs f = cam_obs_to_robot_frame(20, 30, true, 0, UNIT_TO_MM);    // → (200, 300)
    CamObs b = cam_obs_to_robot_frame(-10, -10, true, 1, UNIT_TO_MM);  // → (100, 100)
    BallFused out = fuse_ball_dual(f, b, true, true);
    TEST_ASSERT_TRUE(out.visible);
    TEST_ASSERT_EQUAL_INT16(150, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(200, out.y_mm);
}

void test_fuse_ball_confidence_single_80_consensus_95(void) {
    // Una sola cámara → confidence exacta 80. Dos coincidiendo → 95.
    CamObs f = cam_obs_to_robot_frame(20, 30, true, 0, UNIT_TO_MM);
    CamObs b_off = cam_obs_to_robot_frame(0, 0, false, 1, UNIT_TO_MM);
    BallFused single = fuse_ball_dual(f, b_off, true, true);
    TEST_ASSERT_EQUAL_UINT8(80, single.confidence);

    CamObs b_on = cam_obs_to_robot_frame(-20, -30, true, 1, UNIT_TO_MM);  // → (200,300)
    BallFused consensus = fuse_ball_dual(f, b_on, true, true);
    TEST_ASSERT_EQUAL_UINT8(95, consensus.confidence);
}

// ============================================================================
// SB-4: clamp de los casts float→int16 en la fusión (eval 2026-06-04)
//
// HOY el clamp es no-op (el parser da ~[-1280,1280] mm). Estos tests fuerzan,
// con un unit_to_mm grande (simulando una recalibración de homografía que sube
// el factor), valores que SIN clamp desbordarían int16 y wrappearían de signo.
// Verifican: (a) saturación a los extremos, NO wrap de signo; (b) que el rango
// normal sigue idéntico (el clamp no toca valores in-range).
// ============================================================================

void test_sb4_ball_clamps_positive_overflow_no_wrap(void) {
    // x_raw=20000, unit=10 => 200000 mm, muy por encima de int16 (32767).
    // Sin clamp, el cast crudo wrappearía a un valor negativo (positivo→izq).
    CamObs f = cam_obs_to_robot_frame(20000, 20000, true, 0, /*unit_to_mm=*/10.0f);
    CamObs b = cam_obs_to_robot_frame(0, 0, false, 1, 10.0f);
    BallFused out = fuse_ball_dual(f, b, true, false);
    TEST_ASSERT_TRUE(out.visible);
    TEST_ASSERT_EQUAL_INT16(32767, out.x_mm);   // satura, no wrap negativo
    TEST_ASSERT_EQUAL_INT16(32767, out.y_mm);
}

void test_sb4_ball_clamps_negative_overflow_no_wrap(void) {
    // back rota 180°: x_raw=20000 => -200000 mm => satura a -32768 (no wrap +).
    CamObs f = cam_obs_to_robot_frame(0, 0, false, 0, 10.0f);
    CamObs b = cam_obs_to_robot_frame(20000, 20000, true, 1, 10.0f);
    BallFused out = fuse_ball_dual(f, b, false, true);
    TEST_ASSERT_TRUE(out.visible);
    TEST_ASSERT_EQUAL_INT16(-32768, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(-32768, out.y_mm);
}

void test_sb4_goal_distance_clamps_no_wrap(void) {
    // Distancia enorme (x=300000,y=0) => sqrt = 300000 mm, satura a 32767.
    // El ángulo (atan2) nunca desborda; solo verificamos la distancia.
    CamObs f = cam_obs_to_robot_frame(30000, 0, true, 0, 10.0f);  // x = 300000 mm
    CamObs b = cam_obs_to_robot_frame(0, 0, false, 1, 10.0f);
    GoalFused out = fuse_goal_dual(f, b, true, false);
    TEST_ASSERT_TRUE(out.visible);
    TEST_ASSERT_EQUAL_INT16(32767, out.distance_mm);  // satura, no wrap
    // El ángulo sigue siendo +90° (derecha pura) — el clamp no lo perturba.
    TEST_ASSERT_INT16_WITHIN(10, 9000, out.angle_centideg);
}

void test_sb4_in_range_values_unchanged(void) {
    // Red de regresión explícita: en el rango operativo de hoy el clamp es
    // transparente (mismo truncamiento que el cast previo). (300,400) intactos.
    CamObs f = cam_obs_to_robot_frame(30, 40, true, 0, UNIT_TO_MM);   // → (300,400)
    CamObs b = cam_obs_to_robot_frame(-30, -40, true, 1, UNIT_TO_MM); // → (300,400)
    BallFused out = fuse_ball_dual(f, b, true, true);
    TEST_ASSERT_EQUAL_INT16(300, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(400, out.y_mm);
}

// ============================================================================
// Runner
// ============================================================================

// ============================================================================
// cam_obs_to_polar (A1: ángulo/distancia de UNA cámara, para arcos per-cámara)
// ============================================================================

void test_polar_front_zero_angle(void) {
    CamObs o{}; o.x_mm = 0.0f; o.y_mm = 100.0f; o.visible = true;
    GoalFused p = cam_obs_to_polar(o);
    TEST_ASSERT_TRUE(p.visible);
    TEST_ASSERT_INT16_WITHIN(1, 0, p.angle_centideg);     // 0° = al frente
    TEST_ASSERT_INT16_WITHIN(1, 100, p.distance_mm);
}

void test_polar_right_is_plus_90(void) {
    CamObs o{}; o.x_mm = 100.0f; o.y_mm = 0.0f; o.visible = true;
    GoalFused p = cam_obs_to_polar(o);
    TEST_ASSERT_INT16_WITHIN(1, 9000, p.angle_centideg);  // +90° = DERECHA
    TEST_ASSERT_INT16_WITHIN(1, 100, p.distance_mm);
}

void test_polar_left_is_negative(void) {
    CamObs o{}; o.x_mm = -100.0f; o.y_mm = 0.0f; o.visible = true;
    GoalFused p = cam_obs_to_polar(o);
    TEST_ASSERT_INT16_WITHIN(1, -9000, p.angle_centideg); // -90° = izquierda
}

void test_polar_invisible_is_zero(void) {
    CamObs o{}; o.x_mm = 50.0f; o.y_mm = 50.0f; o.visible = false;
    GoalFused p = cam_obs_to_polar(o);
    TEST_ASSERT_FALSE(p.visible);
    TEST_ASSERT_EQUAL_INT16(0, p.angle_centideg);
    TEST_ASSERT_EQUAL_INT16(0, p.distance_mm);
}

int main(int, char**) {
    UNITY_BEGIN();

    // cam_obs_to_robot_frame
    RUN_TEST(test_to_robot_frame_front_camera_passes_through);
    RUN_TEST(test_to_robot_frame_back_camera_rotates_180);
    RUN_TEST(test_to_robot_frame_preserves_visible_flag);
    RUN_TEST(test_to_robot_frame_zero_coords);

    // fuse_ball_dual
    RUN_TEST(test_fuse_ball_both_dead_returns_invisible);
    RUN_TEST(test_fuse_ball_only_front_alive_uses_front);
    RUN_TEST(test_fuse_ball_only_back_alive_uses_back);
    RUN_TEST(test_fuse_ball_neither_visible_returns_invisible);
    RUN_TEST(test_fuse_ball_both_visible_averages);
    RUN_TEST(test_fuse_ball_alive_but_not_visible);

    // fuse_goal_dual
    RUN_TEST(test_fuse_goal_invisible_when_no_camera);
    RUN_TEST(test_fuse_goal_front_only_angle_at_zero_means_in_front);
    RUN_TEST(test_fuse_goal_lateral_90_centideg);
    RUN_TEST(test_fuse_goal_back_camera_inverts_angle);
    RUN_TEST(test_fuse_goal_both_visible_averages);

    // Watchdog
    RUN_TEST(test_fuse_ignores_dead_camera_with_visible_packet);

    // Edge cases adicionales (ítem E)
    RUN_TEST(test_fuse_goal_left_is_negative_angle);
    RUN_TEST(test_fuse_ball_both_visible_distinct_points_true_average);
    RUN_TEST(test_fuse_ball_confidence_single_80_consensus_95);

    // SB-4: clamp de casts float→int16 en la fusión
    RUN_TEST(test_sb4_ball_clamps_positive_overflow_no_wrap);
    RUN_TEST(test_sb4_ball_clamps_negative_overflow_no_wrap);
    RUN_TEST(test_sb4_goal_distance_clamps_no_wrap);
    RUN_TEST(test_sb4_in_range_values_unchanged);

    RUN_TEST(test_polar_front_zero_angle);
    RUN_TEST(test_polar_right_is_plus_90);
    RUN_TEST(test_polar_left_is_negative);
    RUN_TEST(test_polar_invisible_is_zero);

    return UNITY_END();
}
