// test_clear_aim — tests unitarios para src/shared/clear_aim.cpp
// Corre en host con: pio test -e test_native -f test_clear_aim
//
// Convención (marco robot, docs/CONVENCION-EJES-ROBOT.md):
//   +x = derecha, +y = frente; ángulo = atan2(x,y), centideg.
//   push_heading_centideg con componente x>0 ⇒ empuje hacia la DERECHA;
//   x<0 ⇒ hacia la IZQUIERDA.

#include <unity.h>
#include <cmath>
#include "clear_aim.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Componente x (lateral) de un rumbo en centideg: sin(ángulo).
static float push_x(int16_t centideg) {
    const float rad = (centideg / 100.0f) * (3.14159265358979323846f / 180.0f);
    return std::sin(rad);
}
// Componente y (frontal) de un rumbo en centideg: cos(ángulo).
static float push_y(int16_t centideg) {
    const float rad = (centideg / 100.0f) * (3.14159265358979323846f / 180.0f);
    return std::cos(rad);
}

// ============================================================================
// FALLBACK EXACTO: arco propio NO visible → valid=false (caller usa su empuje
// derecho actual, byte-idéntico).
// ============================================================================

void test_not_visible_returns_invalid(void) {
    ClearAimIn in{};
    in.ball_x_mm = 0;
    in.ball_y_mm = 100;
    in.goal_own_angle_centideg = -9000;   // se ignora.
    in.goal_own_visible = false;
    ClearAimOut out = clear_aim(in);
    TEST_ASSERT_FALSE(out.valid);
    TEST_ASSERT_EQUAL_INT16(0, out.push_heading_centideg);
}

void test_not_visible_ignores_all_other_inputs(void) {
    // Aunque la pelota y el arco apunten "obvio", sin visibilidad NO opina.
    ClearAimIn in{};
    in.ball_x_mm = 300;
    in.ball_y_mm = -200;
    in.goal_own_angle_centideg = 12000;
    in.goal_own_visible = false;
    ClearAimOut out = clear_aim(in);
    TEST_ASSERT_FALSE(out.valid);
}

// ============================================================================
// Arco propio a la IZQUIERDA → empujar hacia la DERECHA (componente +x).
// Caso realista del arquero: arco DETRÁS-izquierda (mira al campo).
// ============================================================================

void test_goal_behind_left_pushes_right(void) {
    // Arco atrás-izquierda: goal=−135° (atan2(x<0,y<0)). Push esperado +135°
    // (atrás-derecha): la banda OPUESTA al lado del arco. Componente x>0 (derecha).
    ClearAimIn in{};
    in.ball_x_mm = 0;
    in.ball_y_mm = 100;
    in.goal_own_angle_centideg = -13500;
    in.goal_own_visible = true;
    ClearAimOut out = clear_aim(in);
    TEST_ASSERT_TRUE(out.valid);
    TEST_ASSERT_TRUE(push_x(out.push_heading_centideg) > 0.1f);   // hacia la derecha.
    TEST_ASSERT_INT16_WITHIN(2, 13500, out.push_heading_centideg); // ≈ +135°.
}

void test_goal_side_left_pushes_to_right_side(void) {
    // Arco al costado-izquierda pero ligeramente atrás: goal=−100°. cos<0,
    // sin<0 → perp goal−90 = −190 → wrap +170° (atrás-derecha). x>0.
    ClearAimIn in{};
    in.ball_x_mm = 0;
    in.ball_y_mm = 50;
    in.goal_own_angle_centideg = -10000;
    in.goal_own_visible = true;
    ClearAimOut out = clear_aim(in);
    TEST_ASSERT_TRUE(out.valid);
    TEST_ASSERT_TRUE(push_x(out.push_heading_centideg) > 0.0f);   // hacia la derecha.
}

// ============================================================================
// Arco propio a la DERECHA → empujar hacia la IZQUIERDA (componente −x).
// ============================================================================

void test_goal_behind_right_pushes_left(void) {
    // Arco atrás-derecha: goal=+135°. Push esperado −135° (atrás-izquierda). x<0.
    ClearAimIn in{};
    in.ball_x_mm = 0;
    in.ball_y_mm = 100;
    in.goal_own_angle_centideg = 13500;
    in.goal_own_visible = true;
    ClearAimOut out = clear_aim(in);
    TEST_ASSERT_TRUE(out.valid);
    TEST_ASSERT_TRUE(push_x(out.push_heading_centideg) < -0.1f);   // hacia la izquierda.
    TEST_ASSERT_INT16_WITHIN(2, -13500, out.push_heading_centideg); // ≈ −135°.
}

void test_goal_side_right_pushes_to_left_side(void) {
    // Arco costado-derecha ligeramente atrás: goal=+100°. Push x<0.
    ClearAimIn in{};
    in.ball_x_mm = 0;
    in.ball_y_mm = 50;
    in.goal_own_angle_centideg = 10000;
    in.goal_own_visible = true;
    ClearAimOut out = clear_aim(in);
    TEST_ASSERT_TRUE(out.valid);
    TEST_ASSERT_TRUE(push_x(out.push_heading_centideg) < 0.0f);   // hacia la izquierda.
}

// ============================================================================
// Simetría: izquierda y derecha deben ser espejo exacto.
// ============================================================================

void test_left_right_symmetry(void) {
    ClearAimIn l{};  l.goal_own_visible = true; l.ball_y_mm = 100;
    l.goal_own_angle_centideg = -12000;
    ClearAimIn r = l; r.goal_own_angle_centideg = 12000;
    ClearAimOut ol = clear_aim(l);
    ClearAimOut orr = clear_aim(r);
    TEST_ASSERT_TRUE(ol.valid);
    TEST_ASSERT_TRUE(orr.valid);
    // Rumbos espejo: push_x de uno = −push_x del otro.
    TEST_ASSERT_FLOAT_WITHIN(0.02f, push_x(ol.push_heading_centideg),
                                    -push_x(orr.push_heading_centideg));
}

// ============================================================================
// Arco directamente DETRÁS (goal=180°) → push lateral puro; desempate por la
// pelota (lado lateral ambiguo si la pelota está centrada).
// ============================================================================

void test_goal_directly_behind_ball_right_pushes_right(void) {
    // Arco atrás puro (180°): los dos perp son ±90 (derecha/izquierda pura).
    // Pelota a la derecha → empujar a la derecha (su banda más cercana).
    ClearAimIn in{};
    in.ball_x_mm = 200;
    in.ball_y_mm = 0;
    in.goal_own_angle_centideg = 18000;
    in.goal_own_visible = true;
    ClearAimOut out = clear_aim(in);
    TEST_ASSERT_TRUE(out.valid);
    TEST_ASSERT_TRUE(push_x(out.push_heading_centideg) > 0.9f);    // ≈ +90° (derecha pura).
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, push_y(out.push_heading_centideg));
}

void test_goal_directly_behind_ball_left_pushes_left(void) {
    ClearAimIn in{};
    in.ball_x_mm = -200;
    in.ball_y_mm = 0;
    in.goal_own_angle_centideg = 18000;
    in.goal_own_visible = true;
    ClearAimOut out = clear_aim(in);
    TEST_ASSERT_TRUE(out.valid);
    TEST_ASSERT_TRUE(push_x(out.push_heading_centideg) < -0.9f);   // ≈ −90° (izquierda pura).
}

// ============================================================================
// Pelota CENTRADA + arco detrás: convención estable (derecha) y siempre válido.
// ============================================================================

void test_ball_centered_goal_behind_defaults_right(void) {
    ClearAimIn in{};
    in.ball_x_mm = 0;          // centrada.
    in.ball_y_mm = 0;
    in.goal_own_angle_centideg = 18000;  // arco atrás puro.
    in.goal_own_visible = true;
    ClearAimOut out = clear_aim(in);
    TEST_ASSERT_TRUE(out.valid);
    // Desempate por convención → derecha (+x). No es 0 (no empuja derecho).
    TEST_ASSERT_TRUE(push_x(out.push_heading_centideg) > 0.9f);
}

void test_ball_centered_goal_left_still_pushes_right(void) {
    // Pelota centrada pero arco claramente atrás-izquierda → manda el arco,
    // no hace falta desempate: push a la derecha.
    ClearAimIn in{};
    in.ball_x_mm = 0;
    in.ball_y_mm = 0;
    in.goal_own_angle_centideg = -13500;
    in.goal_own_visible = true;
    ClearAimOut out = clear_aim(in);
    TEST_ASSERT_TRUE(out.valid);
    TEST_ASSERT_TRUE(push_x(out.push_heading_centideg) > 0.0f);
}

// ============================================================================
// El push es siempre PERPENDICULAR al eje del arco (∼90° de separación): nunca
// empuja directo al arco ni directo en contra (esos serían 0/180 respecto al
// eje). Verificamos que el push NO apunta hacia el arco.
// ============================================================================

void test_push_is_not_toward_goal(void) {
    // Arco atrás-izquierda (−135°). El push (+135°) está a 90° del arco, no
    // hacia él (que sería −135°) ni 180° opuesto exacto.
    ClearAimIn in{};
    in.ball_y_mm = 100;
    in.goal_own_angle_centideg = -13500;
    in.goal_own_visible = true;
    ClearAimOut out = clear_aim(in);
    const float goal = -135.0f;
    const float push = out.push_heading_centideg / 100.0f;
    float diff = push - goal;
    while (diff > 180.0f)  diff -= 360.0f;
    while (diff <= -180.0f) diff += 360.0f;
    // Separación ≈ 90° (perpendicular), no 0 (hacia el arco) ni ±180.
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 90.0f, std::fabs(diff));
}

// ============================================================================
// Salida siempre en rango int16 centideg válido (−18000..18000).
// ============================================================================

void test_output_in_centideg_range(void) {
    for (int16_t g = -18000; g <= 18000; g += 1500) {
        ClearAimIn in{};
        in.ball_x_mm = 0;
        in.ball_y_mm = 100;
        in.goal_own_angle_centideg = g;
        in.goal_own_visible = true;
        ClearAimOut out = clear_aim(in);
        TEST_ASSERT_TRUE(out.valid);
        TEST_ASSERT_TRUE(out.push_heading_centideg >= -18000);
        TEST_ASSERT_TRUE(out.push_heading_centideg <= 18000);
    }
}

// ============================================================================
// Runner
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();

    // Fallback exacto.
    RUN_TEST(test_not_visible_returns_invalid);
    RUN_TEST(test_not_visible_ignores_all_other_inputs);

    // Arco izquierda → push derecha.
    RUN_TEST(test_goal_behind_left_pushes_right);
    RUN_TEST(test_goal_side_left_pushes_to_right_side);

    // Arco derecha → push izquierda.
    RUN_TEST(test_goal_behind_right_pushes_left);
    RUN_TEST(test_goal_side_right_pushes_to_left_side);

    // Simetría.
    RUN_TEST(test_left_right_symmetry);

    // Arco directamente atrás + desempate por pelota.
    RUN_TEST(test_goal_directly_behind_ball_right_pushes_right);
    RUN_TEST(test_goal_directly_behind_ball_left_pushes_left);

    // Pelota centrada.
    RUN_TEST(test_ball_centered_goal_behind_defaults_right);
    RUN_TEST(test_ball_centered_goal_left_still_pushes_right);

    // Propiedades geométricas.
    RUN_TEST(test_push_is_not_toward_goal);
    RUN_TEST(test_output_in_centideg_range);

    return UNITY_END();
}
