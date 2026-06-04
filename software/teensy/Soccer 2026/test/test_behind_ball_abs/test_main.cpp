// test_behind_ball_abs — tests unitarios para src/shared/behind_ball_abs.cpp
//
// Behind-the-ball en coordenadas ABSOLUTAS de cancha (Nivel 3). Corre en host:
//   bash scripts/run-host-tests.sh test_behind_ball_abs
//   (o `pio test -e test_native -f test_behind_ball_abs`)
//
// Convención de cancha (docs/CONVENCION-EJES-ROBOT.md §2):
//   origen (0,0)=esquina propia-izq; +Y→arco rival (largo, 2430);
//   +X→derecha (corto, 1820); heading 0=mira +Y; heading crece CCW.

#include <unity.h>
#include <cmath>
#include "behind_ball_abs.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Dimensiones reales de cancha (pinout_common.h).
static constexpr uint16_t FW = 1820;   // FIELD_WIDTH_MM  (X, corto)
static constexpr uint16_t FH = 2430;   // FIELD_HEIGHT_MM (Y, largo)
static constexpr int16_t  GOAL_X = 910;    // centro arco rival
static constexpr int16_t  GOAL_Y = 2310;   // boca arco rival
static constexpr uint16_t GAP = 130;
static constexpr int16_t  TOL = 1000;      // 10° en centideg

// Helper: arma una entrada con defaults sensatos.
static BehindBallAbsIn make_in(int16_t rx, int16_t ry, int16_t bx, int16_t by) {
    BehindBallAbsIn in{};
    in.robot_x_mm = rx; in.robot_y_mm = ry;
    in.robot_heading_centideg = 0;
    in.ball_x_mm = bx; in.ball_y_mm = by;
    in.opp_goal_x_mm = GOAL_X; in.opp_goal_y_mm = GOAL_Y;
    in.gap_mm = GAP;
    in.field_width_mm = FW; in.field_height_mm = FH;
    return in;
}

// ============================================================================
// target detrás de la pelota sobre la recta pelota→arco
// ============================================================================

void test_target_ball_centered_goal_straight_ahead(void) {
    // Pelota en (910, 1200), arco en (910, 2310): recta pelota→arco es +Y puro.
    // û = (0, +1). target = pelota - gap*û = (910, 1200 - 130) = (910, 1070).
    BehindBallAbsIn in = make_in(910, 600, 910, 1200);
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    TEST_ASSERT_INT16_WITHIN(1, 910, o.target_x_mm);
    TEST_ASSERT_INT16_WITHIN(1, 1070, o.target_y_mm);
}

void test_target_is_on_far_side_from_goal(void) {
    // Caso clásico "pelota ENTRE robot y arco": robot atrás (Y bajo), pelota en
    // el medio, arco adelante. El target debe quedar del lado OPUESTO al arco
    // respecto a la pelota → con menor Y que la pelota (más cerca del robot).
    BehindBallAbsIn in = make_in(910, 300, 910, 1500);
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    TEST_ASSERT_TRUE(o.target_y_mm < in.ball_y_mm);   // detrás (lado robot)
    TEST_ASSERT_INT16_WITHIN(1, 910, o.target_x_mm);
    TEST_ASSERT_INT16_WITHIN(1, 1370, o.target_y_mm); // 1500 - 130
}

void test_target_diagonal_goal(void) {
    // Pelota en (510, 1110), arco en (910, 2310): dx=400, dy=1200, d=1264.9.
    // û≈(0.3162, 0.9487). target = (510 - 130*0.3162, 1110 - 130*0.9487)
    //   = (510 - 41.1, 1110 - 123.3) = (468.9, 986.7).
    BehindBallAbsIn in = make_in(400, 600, 510, 1110);
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    TEST_ASSERT_INT16_WITHIN(2, 469, o.target_x_mm);
    TEST_ASSERT_INT16_WITHIN(2, 987, o.target_y_mm);
}

// ============================================================================
// clamp a cancha
// ============================================================================

void test_target_clamped_inside_field(void) {
    // Pelota pegada a la pared del arco rival (910, 2300) → el target detrás
    // queda con menor Y (dentro), pero forzamos un caso límite: pelota en una
    // esquina con la recta empujando el target afuera, y verificamos clamp.
    // Pelota en (40, 2300), arco (910, 2310): û casi +X. gap=130 grande →
    // target_x = 40 - 130*ux. dx=870,dy=10,d≈870.06 → ux≈0.99989 →
    // target_x ≈ 40 - 130 = -90 → clamp a 0.
    BehindBallAbsIn in = make_in(40, 2000, 40, 2300);
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    TEST_ASSERT_TRUE(o.target_x_mm >= 0);
    TEST_ASSERT_TRUE(o.target_x_mm <= (int16_t)FW);
    TEST_ASSERT_TRUE(o.target_y_mm >= 0);
    TEST_ASSERT_TRUE(o.target_y_mm <= (int16_t)FH);
    TEST_ASSERT_INT16_WITHIN(1, 0, o.target_x_mm);   // clampeado al borde
}

void test_target_never_negative_or_over_bounds(void) {
    // Pelota fuera de banda imposible no se da, pero gap gigante sí puede tirar
    // el target afuera. Pelota en (100, 100), arco lejos en +Y → target detrás
    // baja de 0 en Y → clamp a 0.
    BehindBallAbsIn in = make_in(910, 50, 100, 100);
    in.gap_mm = 500;
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    TEST_ASSERT_TRUE(o.target_y_mm >= 0);
    TEST_ASSERT_TRUE(o.target_x_mm >= 0);
}

// ============================================================================
// desired_heading (robot → arco rival)
// ============================================================================

void test_heading_goal_straight_ahead_is_zero(void) {
    // Robot en (910, 500), arco en (910, 2310): mirar +Y → heading 0.
    BehindBallAbsIn in = make_in(910, 500, 910, 1000);
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    TEST_ASSERT_INT16_WITHIN(2, 0, o.desired_heading_centideg);
}

void test_heading_goal_to_right_is_negative(void) {
    // Arco a la DERECHA del robot (+X puro): robot en (0, 2310), arco (910,2310).
    // dx=+910, dy=0 → atan2(+,0)=+90° → heading = -90° = -9000 centideg.
    BehindBallAbsIn in = make_in(0, 2310, 400, 2310);
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    TEST_ASSERT_INT16_WITHIN(10, -9000, o.desired_heading_centideg);
}

void test_heading_goal_to_left_is_positive(void) {
    // Arco a la IZQUIERDA del robot (-X puro): robot en (1820, 2310), arco
    // (910, 2310). dx=-910, dy=0 → atan2(-,0)=-90° → heading = +90° = +9000.
    BehindBallAbsIn in = make_in(1820, 2310, 1400, 2310);
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    TEST_ASSERT_INT16_WITHIN(10, 9000, o.desired_heading_centideg);
}

void test_heading_goal_behind_is_180(void) {
    // Arco DETRÁS del robot (-Y): robot en (910, 2300), arco lo dejamos en +Y
    // pero ponemos al robot más cerca del arco rival y la pelota propia atrás —
    // para -Y puro usamos un arco ficticio detrás: robot (910,1500), arco abajo.
    BehindBallAbsIn in = make_in(910, 1500, 910, 1000);
    in.opp_goal_x_mm = 910; in.opp_goal_y_mm = 0;   // "arco" en -Y respecto robot
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    // dx=0, dy=-1500 → atan2(0,-)=180° → heading=-180°=±18000 (wrap a +18000).
    TEST_ASSERT_INT16_WITHIN(10, 18000, std::abs(o.desired_heading_centideg));
}

// ============================================================================
// is_aligned_to_shoot
// ============================================================================

void test_aligned_robot_behind_ball_colinear_true(void) {
    // Robot, pelota y arco colineales en +Y: robot (910,800), pelota (910,1200),
    // arco (910,2310). robot→pelota = +Y, pelota→arco = +Y → diff 0 → alineado.
    BehindBallAbsIn in = make_in(910, 800, 910, 1200);
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    TEST_ASSERT_TRUE(o.is_aligned_to_shoot);
}

void test_not_aligned_robot_off_axis_false(void) {
    // Robot lateralizado: robot (300,800), pelota (910,1200), arco (910,2310).
    // robot→pelota apunta arriba-derecha; pelota→arco es +Y puro → diff grande
    // (>10°) → NO alineado.
    BehindBallAbsIn in = make_in(300, 800, 910, 1200);
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    TEST_ASSERT_FALSE(o.is_aligned_to_shoot);
}

void test_not_aligned_robot_in_front_of_ball_false(void) {
    // Robot del lado EQUIVOCADO (entre pelota y arco): robot (910,1800),
    // pelota (910,1200), arco (910,2310). robot→pelota = -Y, pelota→arco = +Y →
    // diff 180° → NO alineado (empujar mandaría la pelota a tu propio arco).
    BehindBallAbsIn in = make_in(910, 1800, 910, 1200);
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    TEST_ASSERT_FALSE(o.is_aligned_to_shoot);
}

void test_aligned_within_tolerance_true(void) {
    // Pequeño desfasaje dentro de tolerancia. Pelota (910,1200), arco (910,2310)
    // → pelota→arco = +Y (0°). Robot ligeramente al costado: (945,800).
    // robot→pelota: dx=-35, dy=400 → atan2(-35,400) ≈ -5.0° → |diff|≈5° < 10°.
    BehindBallAbsIn in = make_in(945, 800, 910, 1200);
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    TEST_ASSERT_TRUE(o.is_aligned_to_shoot);
}

void test_aligned_just_outside_tolerance_false(void) {
    // Justo afuera: robot→pelota a ~12° del eje. Pelota (910,1200), arco +Y.
    // Robot (995,800): dx=-85,dy=400 → atan2(-85,400)≈ -12.0° → |diff|≈12°>10°.
    BehindBallAbsIn in = make_in(995, 800, 910, 1200);
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    TEST_ASSERT_FALSE(o.is_aligned_to_shoot);
}

// ============================================================================
// degenerados / robustez
// ============================================================================

void test_degenerate_ball_equals_goal(void) {
    // pelota == arco → recta indefinida. No debe crashear ni dividir por cero:
    // target = pelota (clampeada), alineado = false.
    BehindBallAbsIn in = make_in(910, 800, GOAL_X, GOAL_Y);
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    TEST_ASSERT_INT16_WITHIN(1, GOAL_X, o.target_x_mm);
    TEST_ASSERT_INT16_WITHIN(1, GOAL_Y, o.target_y_mm);
    TEST_ASSERT_FALSE(o.is_aligned_to_shoot);
}

void test_degenerate_robot_equals_goal_heading_fallback(void) {
    // robot == arco → heading robot→arco indefinido; cae a robot→pelota.
    // robot=arco=(910,2310), pelota (910,1000): dx=0,dy=-1310 → heading ±180°.
    BehindBallAbsIn in = make_in(GOAL_X, GOAL_Y, 910, 1000);
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    TEST_ASSERT_INT16_WITHIN(10, 18000, std::abs(o.desired_heading_centideg));
}

void test_degenerate_robot_equals_ball_not_aligned(void) {
    // robot == pelota → dirección robot→pelota indefinida → no alineado (no
    // crashea). target sigue bien definido (recta pelota→arco existe).
    BehindBallAbsIn in = make_in(910, 1200, 910, 1200);
    BehindBallAbsOut o = behind_ball_abs(in, TOL);
    TEST_ASSERT_FALSE(o.is_aligned_to_shoot);
    TEST_ASSERT_INT16_WITHIN(1, 1070, o.target_y_mm);  // 1200 - 130 sobre +Y
}

void test_output_target_in_bounds_always(void) {
    // Barrido rápido: para varias pelotas, el target SIEMPRE cae en cancha.
    for (int16_t bx = 100; bx <= 1700; bx += 400) {
        for (int16_t by = 100; by <= 2300; by += 500) {
            BehindBallAbsIn in = make_in(910, 100, bx, by);
            BehindBallAbsOut o = behind_ball_abs(in, TOL);
            TEST_ASSERT_TRUE(o.target_x_mm >= 0 && o.target_x_mm <= (int16_t)FW);
            TEST_ASSERT_TRUE(o.target_y_mm >= 0 && o.target_y_mm <= (int16_t)FH);
        }
    }
}

// ============================================================================
// Runner
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();

    // target
    RUN_TEST(test_target_ball_centered_goal_straight_ahead);
    RUN_TEST(test_target_is_on_far_side_from_goal);
    RUN_TEST(test_target_diagonal_goal);

    // clamp
    RUN_TEST(test_target_clamped_inside_field);
    RUN_TEST(test_target_never_negative_or_over_bounds);

    // heading
    RUN_TEST(test_heading_goal_straight_ahead_is_zero);
    RUN_TEST(test_heading_goal_to_right_is_negative);
    RUN_TEST(test_heading_goal_to_left_is_positive);
    RUN_TEST(test_heading_goal_behind_is_180);

    // is_aligned_to_shoot
    RUN_TEST(test_aligned_robot_behind_ball_colinear_true);
    RUN_TEST(test_not_aligned_robot_off_axis_false);
    RUN_TEST(test_not_aligned_robot_in_front_of_ball_false);
    RUN_TEST(test_aligned_within_tolerance_true);
    RUN_TEST(test_aligned_just_outside_tolerance_false);

    // degenerados / robustez
    RUN_TEST(test_degenerate_ball_equals_goal);
    RUN_TEST(test_degenerate_robot_equals_goal_heading_fallback);
    RUN_TEST(test_degenerate_robot_equals_ball_not_aligned);
    RUN_TEST(test_output_target_in_bounds_always);

    return UNITY_END();
}
