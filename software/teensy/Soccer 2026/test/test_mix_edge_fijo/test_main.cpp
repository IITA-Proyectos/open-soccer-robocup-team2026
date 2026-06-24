// test_mix_edge_fijo — núcleo PURO del rodeo Edge POR POSICIÓN PURA (src/centraledgefijo/mix_edge.cpp).
// Corre host con: bash scripts/run-host-tests.sh test_mix_edge_fijo
//
// Igual que test_mix_edge pero para la versión SIN velocidad de pelota: la curva (cero, simetría,
// monotonía, continuidad, valores, tope) + la decisión de empuje. NO hay tests de feedforward
// porque esta versión no usa la velocidad (EdgeIn no tiene vx/vy).

#include <unity.h>
#include <cmath>

#include "../../src/centraledgefijo/mix_config.h"   // MIX_EDGE_* (host-safe: solo <stdint.h>)
#include "../../src/centraledgefijo/mix_edge.cpp"    // trae también mix_edge.h

using namespace iitasoccer::mix;

void setUp(void) {}
void tearDown(void) {}

// Parámetros = los DEFAULTS reales de mix_config.h (sin params de velocidad en esta versión).
static EdgeParams P() {
    EdgeParams p{};
    p.k_near        = MIX_EDGE_K_NEAR;
    p.b1_deg        = MIX_EDGE_B1_DEG;
    p.k_side        = MIX_EDGE_K_SIDE;
    p.b2_deg        = MIX_EDGE_B2_DEG;
    p.k_wide        = MIX_EDGE_K_WIDE;
    p.go_max_deg    = MIX_EDGE_GO_MAX_DEG;
    p.push_dist_cm  = MIX_EDGE_PUSH_DIST_CM;
    p.push_align_deg= MIX_EDGE_PUSH_ALIGN_DEG;
    p.push_goal_deg = MIX_EDGE_PUSH_GOAL_DEG;
    return p;
}

// Pelota en (ángulo, distancia) → x/y. (EdgeIn de esta versión NO tiene velocidad.)
static EdgeIn mk(float ang_deg, float dist_cm, bool ball, bool goal_vis, float goal_ang) {
    const float a = ang_deg * 3.14159265f / 180.0f;
    EdgeIn in{};
    in.ball_x_cm    = dist_cm * std::sin(a);
    in.ball_y_cm    = dist_cm * std::cos(a);
    in.ball_visible = ball;
    in.goal_visible = goal_vis;
    in.goal_angle_deg = goal_ang;
    return in;
}

// -------- CURVA (idéntica a la versión con velocidad) --------

void test_wrap_cero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, mix_edge_wrap_angle(0.0f, P()));
}
void test_wrap_simetrico(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -mix_edge_wrap_angle(40.0f, P()), mix_edge_wrap_angle(-40.0f, P()));
    TEST_ASSERT_TRUE(mix_edge_wrap_angle( 50.0f, P()) > 0.0f);
    TEST_ASSERT_TRUE(mix_edge_wrap_angle(-50.0f, P()) < 0.0f);
}
void test_wrap_monotono(void) {
    TEST_ASSERT_TRUE(mix_edge_wrap_angle(10.0f, P()) < mix_edge_wrap_angle(30.0f, P()));
    TEST_ASSERT_TRUE(mix_edge_wrap_angle(30.0f, P()) < mix_edge_wrap_angle(60.0f, P()));
    TEST_ASSERT_TRUE(mix_edge_wrap_angle(60.0f, P()) < mix_edge_wrap_angle(90.0f, P()));
}
void test_wrap_continua(void) {
    const float eps = 0.1f;
    TEST_ASSERT_FLOAT_WITHIN(0.5f, mix_edge_wrap_angle(20.0f - eps, P()), mix_edge_wrap_angle(20.0f + eps, P()));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, mix_edge_wrap_angle(75.0f - eps, P()), mix_edge_wrap_angle(75.0f + eps, P()));
}
void test_wrap_valores(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 24.0f,  mix_edge_wrap_angle(20.0f, P()));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 104.0f, mix_edge_wrap_angle(60.0f, P()));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 149.0f, mix_edge_wrap_angle(90.0f, P()));
}
void test_wrap_tope(void) {
    TEST_ASSERT_TRUE(mix_edge_wrap_angle(180.0f, P()) <= MIX_EDGE_GO_MAX_DEG + 0.01f);
}

// -------- go_ang usa SOLO la posición (no hay velocidad que cambie nada) --------

// El ángulo de avance es EXACTAMENTE la curva sobre el ángulo de la pelota (posición pura).
void test_go_es_solo_posicion(void) {
    EdgeOut o = mix_edge_attack(mk(60.0f, 50.0f, true, false, 0.0f), P());
    TEST_ASSERT_FLOAT_WITHIN(0.5f, mix_edge_wrap_angle(60.0f, P()), o.go_ang_deg);
}

// -------- DECISIÓN DE EMPUJE --------

void test_push_lejos_no(void) {
    TEST_ASSERT_FALSE(mix_edge_attack(mk(0.0f, 100.0f, true, true, 0.0f), P()).push_ready);
}
void test_push_alineado_si(void) {
    TEST_ASSERT_TRUE(mix_edge_attack(mk(5.0f, 10.0f, true, true, 5.0f), P()).push_ready);
}
void test_push_arco_desalineado_no(void) {
    TEST_ASSERT_FALSE(mix_edge_attack(mk(5.0f, 10.0f, true, true, 60.0f), P()).push_ready);
}
void test_push_sin_arco_si(void) {
    TEST_ASSERT_TRUE(mix_edge_attack(mk(5.0f, 10.0f, true, false, 0.0f), P()).push_ready);
}
void test_push_costado_no(void) {
    TEST_ASSERT_FALSE(mix_edge_attack(mk(60.0f, 10.0f, true, true, 0.0f), P()).push_ready);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_wrap_cero);
    RUN_TEST(test_wrap_simetrico);
    RUN_TEST(test_wrap_monotono);
    RUN_TEST(test_wrap_continua);
    RUN_TEST(test_wrap_valores);
    RUN_TEST(test_wrap_tope);
    RUN_TEST(test_go_es_solo_posicion);
    RUN_TEST(test_push_lejos_no);
    RUN_TEST(test_push_alineado_si);
    RUN_TEST(test_push_arco_desalineado_no);
    RUN_TEST(test_push_sin_arco_si);
    RUN_TEST(test_push_costado_no);
    return UNITY_END();
}
