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

#include "../../src/centraledge/mix_config.h"     // MIX_EDGE_* (host-safe: solo <stdint.h>)
#include "../../src/centraledge/mix_edge.cpp"      // trae también mix_edge.h

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
    p.vel_min_cm_s  = MIX_EDGE_VEL_MIN_CM_S;
    p.lead_s        = MIX_EDGE_LEAD_S;
    p.lead_max_cm   = MIX_EDGE_LEAD_MAX_CM;
    return p;
}

// Pelota en (ángulo, distancia) → x/y, velocidad 0 (estática). Para los tests sin velocidad.
static EdgeIn mk(float ang_deg, float dist_cm, bool ball, bool goal_vis, float goal_ang) {
    const float a = ang_deg * 3.14159265f / 180.0f;
    EdgeIn in{};
    in.ball_x_cm    = dist_cm * std::sin(a);
    in.ball_y_cm    = dist_cm * std::cos(a);
    in.ball_vx_cm_s = 0.0f;
    in.ball_vy_cm_s = 0.0f;
    in.ball_visible = ball;
    in.goal_visible = goal_vis;
    in.goal_angle_deg = goal_ang;
    return in;
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

// -------- FEEDFORWARD DE VELOCIDAD (anticipar la pelota en movimiento) --------

// Pelota al frente (ang 0) pero MOVIÉNDOSE a la derecha rápido → el rodeo apunta a la
// DERECHA (anticipa), no derecho (0). Sin el feedforward daría ~0.
void test_ff_anticipa_lado(void) {
    EdgeIn in{};
    in.ball_x_cm = 0.0f; in.ball_y_cm = 50.0f;   // pelota al frente, 50 cm
    in.ball_vx_cm_s = 100.0f; in.ball_vy_cm_s = 0.0f;  // se va a la derecha, 100 cm/s
    in.ball_visible = true; in.goal_visible = false; in.goal_angle_deg = 0.0f;
    EdgeOut o = mix_edge_attack(in, P());
    TEST_ASSERT_TRUE(o.go_ang_deg > 5.0f);   // apunta claramente a la derecha (anticipa)
}

// Misma pelota pero velocidad por DEBAJO del umbral → NO anticipa (≈ derecho a la pelota).
void test_ff_gate_ignora_lento(void) {
    EdgeIn in{};
    in.ball_x_cm = 0.0f; in.ball_y_cm = 50.0f;
    in.ball_vx_cm_s = 5.0f; in.ball_vy_cm_s = 0.0f;   // 5 cm/s < vel_min (30) → ruido
    in.ball_visible = true; in.goal_visible = false; in.goal_angle_deg = 0.0f;
    EdgeOut o = mix_edge_attack(in, P());
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, o.go_ang_deg);   // sin lead → casi 0
}

// Velocidad enorme → el adelanto se TOPEA (lead_max_cm): el ángulo no se dispara sin límite.
void test_ff_tope(void) {
    EdgeIn fast{};
    fast.ball_x_cm = 0.0f; fast.ball_y_cm = 50.0f;
    fast.ball_vx_cm_s = 100000.0f; fast.ball_vy_cm_s = 0.0f;  // absurdo (ruido extremo)
    fast.ball_visible = true; fast.goal_visible = false; fast.goal_angle_deg = 0.0f;
    EdgeOut o = mix_edge_attack(fast, P());
    // Con tope 40 cm de adelanto sobre 50 cm de frente → ángulo ≈ atan2(40,50)=38.7° → curva.
    // Sin tope, atan2(enorme,50)≈90° → sería mucho mayor. Verificamos que quedó acotado.
    const float a = std::atan2(MIX_EDGE_LEAD_MAX_CM, 50.0f) * 180.0f / 3.14159265f; // ≈38.7°
    TEST_ASSERT_TRUE(o.go_ang_deg <= mix_edge_wrap_angle(a + 1.0f, P()) + 0.5f);
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
    RUN_TEST(test_ff_anticipa_lado);
    RUN_TEST(test_ff_gate_ignora_lento);
    RUN_TEST(test_ff_tope);
    return UNITY_END();
}
