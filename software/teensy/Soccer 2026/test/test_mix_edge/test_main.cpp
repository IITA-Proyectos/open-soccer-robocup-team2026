// test_mix_edge — núcleo PURO del rodeo estilo Edge (src/centralmix/mix_edge.cpp).
// Corre host con: bash scripts/run-host-tests.sh test_mix_edge
//
// Cubre:
//   • la CURVA |ángulo de pelota| → ángulo de avance: cero, simetría, monotonía,
//     CONTINUIDAD en los quiebres b1/b2, valores clave y tope go_max;
//   • la decisión de EMPUJE (push_ready): lejos / cerca+alineado / arco desalineado /
//     arco no visible / pelota al costado.
//
// mix_edge.cpp es PURO (solo <cmath>), no está en src/shared → se incluye por ruta para
// compilarlo dentro de esta unidad de traducción (el runner solo linkea src/shared).

#include <unity.h>
#include <cmath>

#include "../../src/centralmix/mix_config.h"     // MIX_EDGE_* (host-safe: solo <stdint.h>)
#include "../../src/centralmix/mix_edge.cpp"      // trae también mix_edge.h

using namespace iitasoccer::mix;

void setUp(void) {}
void tearDown(void) {}

// Parámetros = los DEFAULTS reales de mix_config.h (así el test detecta drift de tuneo).
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

// -------- CURVA --------

// Pelota justo al frente → avanzar al frente (0°).
void test_wrap_cero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, mix_edge_wrap_angle(0.0f, P()));
}

// Simetría: pelota a +a y −a dan ángulos opuestos del mismo módulo.
void test_wrap_simetrico(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -mix_edge_wrap_angle(40.0f, P()),
                                     mix_edge_wrap_angle(-40.0f, P()));
    TEST_ASSERT_TRUE(mix_edge_wrap_angle( 50.0f, P()) > 0.0f);  // pelota a la derecha → avanzo a la derecha
    TEST_ASSERT_TRUE(mix_edge_wrap_angle(-50.0f, P()) < 0.0f);
}

// Monotonía: a más ángulo de pelota, más ángulo de avance (rodea más).
void test_wrap_monotono(void) {
    const float w10 = mix_edge_wrap_angle(10.0f, P());
    const float w30 = mix_edge_wrap_angle(30.0f, P());
    const float w60 = mix_edge_wrap_angle(60.0f, P());
    const float w90 = mix_edge_wrap_angle(90.0f, P());
    TEST_ASSERT_TRUE(w10 < w30);
    TEST_ASSERT_TRUE(w30 < w60);
    TEST_ASSERT_TRUE(w60 < w90);
}

// CONTINUIDAD en los quiebres b1=20 y b2=75 (sin saltos: la curva es piecewise CONTINUA).
void test_wrap_continua(void) {
    const float eps = 0.1f;
    TEST_ASSERT_FLOAT_WITHIN(0.5f, mix_edge_wrap_angle(20.0f - eps, P()),
                                    mix_edge_wrap_angle(20.0f + eps, P()));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, mix_edge_wrap_angle(75.0f - eps, P()),
                                    mix_edge_wrap_angle(75.0f + eps, P()));
}

// Valores clave (con los defaults 1.2 / 20 / 2.0 / 75 / 1.0): el rodeo amplifica.
void test_wrap_valores(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 24.0f,  mix_edge_wrap_angle(20.0f, P()));  // 1.2*20
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 104.0f, mix_edge_wrap_angle(60.0f, P()));  // 24 + 2*40
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 149.0f, mix_edge_wrap_angle(90.0f, P()));  // 134 + 1*15
}

// Tope go_max: nunca apuntar 100% hacia atrás.
void test_wrap_tope(void) {
    TEST_ASSERT_TRUE(mix_edge_wrap_angle(180.0f, P()) <= MIX_EDGE_GO_MAX_DEG + 0.01f);
}

// -------- DECISIÓN DE EMPUJE --------

static EdgeIn mk(float ang, float dist, bool ball, bool goal_vis, float goal_ang) {
    EdgeIn in{};
    in.ball_angle_deg = ang;
    in.ball_dist_cm   = dist;
    in.ball_visible   = ball;
    in.goal_visible   = goal_vis;
    in.goal_angle_deg = goal_ang;
    return in;
}

// Pelota LEJOS → no empujar (sigue rodeando).
void test_push_lejos_no(void) {
    EdgeOut o = mix_edge_attack(mk(0.0f, 100.0f, true, true, 0.0f), P());
    TEST_ASSERT_FALSE(o.push_ready);
}

// Cerca + al frente + arco alineado → empujar.
void test_push_alineado_si(void) {
    EdgeOut o = mix_edge_attack(mk(5.0f, 10.0f, true, true, 5.0f), P());
    TEST_ASSERT_TRUE(o.push_ready);
}

// Cerca + al frente pero arco VISIBLE y DESALINEADO → NO empujar (no meter al lado).
void test_push_arco_desalineado_no(void) {
    EdgeOut o = mix_edge_attack(mk(5.0f, 10.0f, true, true, 60.0f), P());
    TEST_ASSERT_FALSE(o.push_ready);
}

// Cerca + al frente + arco NO visible → empujar igual (confía en el rumbo; evita cuelgue).
void test_push_sin_arco_si(void) {
    EdgeOut o = mix_edge_attack(mk(5.0f, 10.0f, true, false, 0.0f), P());
    TEST_ASSERT_TRUE(o.push_ready);
}

// Cerca pero la pelota está al COSTADO (no al frente) → no empujar (primero rodear).
void test_push_costado_no(void) {
    EdgeOut o = mix_edge_attack(mk(60.0f, 10.0f, true, true, 0.0f), P());
    TEST_ASSERT_FALSE(o.push_ready);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_wrap_cero);
    RUN_TEST(test_wrap_simetrico);
    RUN_TEST(test_wrap_monotono);
    RUN_TEST(test_wrap_continua);
    RUN_TEST(test_wrap_valores);
    RUN_TEST(test_wrap_tope);
    RUN_TEST(test_push_lejos_no);
    RUN_TEST(test_push_alineado_si);
    RUN_TEST(test_push_arco_desalineado_no);
    RUN_TEST(test_push_sin_arco_si);
    RUN_TEST(test_push_costado_no);
    return UNITY_END();
}
