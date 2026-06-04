// pio test -e test_native -f test_central_trajectory
// ⚠️ NOTA: caracteriza bt_classify (ball_trajectory). Ya CABLEADO en el arquero
// (strategy.cpp goalkeeper_tick / GK_INTERCEPT vía gk_classify_intercept). Como
// strategy.cpp depende de Arduino y NO compila host-side, la sección
// "decisión del arquero" de abajo ESPEJA exactamente gk_classify_intercept +
// gk_threat_response (mismo armado de BallTrajIn + misma regla X/KP) para
// caracterizar el call-site sin linkear strategy. Si cambia gk_classify_intercept
// o gk_threat_response, actualizar bt_decide_intercept abajo.
#include <unity.h>
#include <cmath>
#include "ball_trajectory.h"
#include "ball_predict.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

// ── Espejo de la decisión del arquero (strategy.cpp::gk_classify_intercept) ──
// Réplica host-testeable: mismas constantes y misma construcción de BallTrajIn.
// Mantener sincronizado con strategy.cpp (GK_BT_* / GK_BT_THREAT_*).
static const int16_t GK_BT_SPEED_MIN_MM_S       = 80;
static const int16_t GK_BT_TOWARD_TOL_CENTIDEG  = 4500;
static const float   GK_BT_THREAT_LEAD_FACTOR   = 1.5f;
static const float   GK_BT_THREAT_KP_FACTOR     = 1.5f;

struct GkDecision { BallTrajKind kind; float target_x_mm; bool threat; float kp_scale; };

// Espejo de strategy.cpp::gk_threat_response. Con threat==false → factores 1.0 →
// target == ball_predict(default).px (== bx_pred del caller) y kp_scale=1.0.
static GkDecision gk_threat_response_mirror(bool threat,
                                            int16_t bx, int16_t by,
                                            int16_t vx, int16_t vy) {
    const BallPredictParams base = ball_predict_default_params();
    const float lf = threat ? GK_BT_THREAT_LEAD_FACTOR : 1.0f;
    BallPredictParams p{ base.lookahead_s * lf, base.max_lead_mm * lf };
    const BallPredictOut pred = ball_predict(bx, by, vx, vy, p);
    GkDecision d{};
    d.target_x_mm = pred.px_mm;
    d.kp_scale    = threat ? GK_BT_THREAT_KP_FACTOR : 1.0f;
    return d;
}

static GkDecision bt_decide_intercept(float bx, float by, float bx_pred,
                                      int16_t vx, int16_t vy,
                                      bool goal_opp_visible,
                                      float goal_opp_angle_deg,
                                      float dist_mm) {
    BallTrajIn in{};
    in.ball_vx_mm_s        = vx;
    in.ball_vy_mm_s        = vy;
    in.ball_speed_min_mm_s = GK_BT_SPEED_MIN_MM_S;
    in.ball_dist_mm        = (int16_t)(dist_mm > 32767.0f ? 32767.0f : dist_mm);
    in.reach_mm            = 0;
    if (goal_opp_visible) {
        float own_deg = goal_opp_angle_deg + 180.0f;
        while (own_deg >  180.0f) own_deg -= 360.0f;
        while (own_deg <= -180.0f) own_deg += 360.0f;
        in.goal_opp_angle_centideg = (int16_t)lroundf(goal_opp_angle_deg * 100.0f);
        in.goal_own_angle_centideg = (int16_t)lroundf(own_deg * 100.0f);
        in.toward_tol_centideg     = GK_BT_TOWARD_TOL_CENTIDEG;
    } else {
        in.toward_tol_centideg = 0;
    }
    const BallTraj t = bt_classify(in);
    GkDecision d{};
    d.kind   = t.kind;
    d.threat = (t.kind == BT_TO_OWN_GOAL);
    // RESPUESTA A AMENAZA ACTIVADA: target_x con más lead si amenaza; KP escalado.
    // No-amenaza → idéntico a bx_pred (default params) y kp_scale=1.0.
    const GkDecision tr = gk_threat_response_mirror(
        d.threat, (int16_t)lroundf(bx), (int16_t)lroundf(by), vx, vy);
    d.target_x_mm = tr.target_x_mm;
    d.kp_scale    = tr.kp_scale;
    (void)bx_pred;
    return d;
}

void test_no_motion_is_still(void){
    BallTrajIn in{}; in.ball_vx_mm_s=0; in.ball_vy_mm_s=0;
    in.ball_speed_min_mm_s=80;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_EQUAL_INT(BT_STILL, t.kind);
}
void test_toward_opponent_goal(void){
    BallTrajIn in{};
    in.ball_vx_mm_s=0; in.ball_vy_mm_s=500;
    in.goal_opp_angle_centideg=0;
    in.goal_own_angle_centideg=18000;
    in.ball_speed_min_mm_s=80; in.toward_tol_centideg=4500;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_EQUAL_INT(BT_TO_OPP_GOAL, t.kind);
}
void test_toward_own_goal(void){
    BallTrajIn in{};
    in.ball_vx_mm_s=0; in.ball_vy_mm_s=-500;
    in.goal_opp_angle_centideg=0;
    in.goal_own_angle_centideg=18000;
    in.ball_speed_min_mm_s=80; in.toward_tol_centideg=4500;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_EQUAL_INT(BT_TO_OWN_GOAL, t.kind);
}
void test_sideways_is_other(void){
    BallTrajIn in{};
    in.ball_vx_mm_s=500; in.ball_vy_mm_s=0;
    in.goal_opp_angle_centideg=0;
    in.goal_own_angle_centideg=18000;
    in.ball_speed_min_mm_s=80; in.toward_tol_centideg=4500;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_EQUAL_INT(BT_OTHER, t.kind);
}
void test_in_reach_flag(void){
    BallTrajIn in{};
    in.ball_vx_mm_s=0; in.ball_vy_mm_s=300; in.ball_speed_min_mm_s=80;
    in.ball_dist_mm=250; in.reach_mm=400;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_TRUE(t.in_reach);
    in.ball_dist_mm=900;
    t = bt_classify(in);
    TEST_ASSERT_FALSE(t.in_reach);
}

void test_speed_threshold_boundary_strict(void){
    // speed exactamente == min NO es STILL (condicion es sp < min).
    BallTrajIn in{};
    in.ball_vx_mm_s=0; in.ball_vy_mm_s=80;          // |v| = 80
    in.goal_opp_angle_centideg=0; in.goal_own_angle_centideg=18000;
    in.ball_speed_min_mm_s=80; in.toward_tol_centideg=4500;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_NOT_EQUAL(BT_STILL, t.kind);         // 80 < 80 es false
    // y speed 79 < 80 SI es STILL
    in.ball_vy_mm_s=79;
    t = bt_classify(in);
    TEST_ASSERT_EQUAL_INT(BT_STILL, t.kind);
}

void test_opp_goal_precedence_when_equidistant(void){
    // Pelota cruzando a 90° (vx>0, vy=0) con AMBOS arcos a 90° de distancia
    // angular: opp gana sobre own por el tie-break d_opp<=d_own.
    // heading = to_cd(90 - atan2(0,500)*180/pi) = to_cd(90-0) = 9000 cd
    // d_opp = |9000-4500| = 4500, d_own = |9000-13500| = 4500 → empate
    BallTrajIn in{};
    in.ball_vx_mm_s=500; in.ball_vy_mm_s=0;          // heading 9000 cd
    in.goal_opp_angle_centideg=4500;                 // d_opp = 4500
    in.goal_own_angle_centideg=13500;                // d_own = 4500
    in.ball_speed_min_mm_s=80; in.toward_tol_centideg=4500;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_EQUAL_INT(BT_TO_OPP_GOAL, t.kind);   // empate => opp gana
}

// ─────────── Tests del call-site del arquero (bt_decide_intercept) ───────────

// Helper: X esperada con los DEFAULT params (lo que persigue hoy, sin amenaza).
static float pred_default_x(int16_t bx, int16_t by, int16_t vx, int16_t vy){
    return ball_predict(bx, by, vx, vy, ball_predict_default_params()).px_mm;
}

void test_gk_shot_to_own_goal_is_threat(void){
    // Arquero mirando al campo: arco rival al frente (0°), arco propio detrás (180°).
    // Pelota hacia -Y (al arco propio): vx=0, vy=-500 → heading 18000 cd == own.
    // AMENAZA: kp_scale debe subir a 1.5. Con vx=0 el lead lateral (X) es 0 igual
    // (la pelota no cruza), así que target_x == X actual; el refuerzo acá es el KP.
    GkDecision d = bt_decide_intercept(/*bx*/0, /*by*/600, /*bx_pred*/120,
                                       /*vx*/0, /*vy*/-500,
                                       /*goal_opp_visible*/true, /*goal_opp_deg*/0.0f,
                                       /*dist*/600);
    TEST_ASSERT_EQUAL_INT(BT_TO_OWN_GOAL, d.kind);
    TEST_ASSERT_TRUE(d.threat);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, d.target_x_mm);             // vx=0 → sin lead lateral
    TEST_ASSERT_EQUAL_FLOAT(1.5f, d.kp_scale);               // amenaza → KP reforzado
}

void test_gk_threat_with_lateral_vel_increases_lead(void){
    // AMENAZA con componente lateral (vx≠0): la X predicha bajo amenaza debe
    // adelantar MÁS (en magnitud) que con los default params (×1.5 lookahead) →
    // mejor anticipación del tiro entrante. Geometría: arco rival al frente (0°),
    // arco propio detrás (180°); pelota yendo atrás-izquierda (vx=-300, vy=-400)
    // ⇒ heading apunta al arco propio dentro del cono ±45° ⇒ TO_OWN_GOAL.
    const int16_t bx=0, by=600, vx=-300, vy=-400;
    GkDecision d = bt_decide_intercept(bx, by, 0, vx, vy,
                                       true, 0.0f, 600);
    TEST_ASSERT_EQUAL_INT(BT_TO_OWN_GOAL, d.kind);
    TEST_ASSERT_TRUE(d.threat);
    const float base_x   = pred_default_x(bx, by, vx, vy);   // X con default params
    // Amenaza ⇒ más lead lateral ⇒ |target| mayor que |base| (vx≠0).
    TEST_ASSERT_TRUE(std::fabs(d.target_x_mm) > std::fabs(base_x));
    // Concretamente: base = 0 + (-300*0.2) = -60 ; amenaza = 0 + (-300*0.3) = -90.
    TEST_ASSERT_EQUAL_FLOAT(-90.0f, d.target_x_mm);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, d.kp_scale);
}

void test_gk_shot_to_opp_goal_not_threat(void){
    // Pelota hacia +Y (al arco rival, alejándose del arquero): vx=0, vy=+500.
    GkDecision d = bt_decide_intercept(0, 600, 120,
                                       0, 500,
                                       true, 0.0f,
                                       600);
    TEST_ASSERT_EQUAL_INT(BT_TO_OPP_GOAL, d.kind);
    TEST_ASSERT_FALSE(d.threat);
    TEST_ASSERT_EQUAL_FLOAT(pred_default_x(0,600,0,500), d.target_x_mm);  // idéntico a hoy
    TEST_ASSERT_EQUAL_FLOAT(1.0f, d.kp_scale);                            // sin refuerzo
}

void test_gk_crossing_ball_is_other(void){
    // Pelota cruzando de costado (vx=500, vy=0): ni al arco propio ni al rival.
    GkDecision d = bt_decide_intercept(200, 600, 250,
                                       500, 0,
                                       true, 0.0f,
                                       600);
    TEST_ASSERT_EQUAL_INT(BT_OTHER, d.kind);
    TEST_ASSERT_FALSE(d.threat);
    TEST_ASSERT_EQUAL_FLOAT(pred_default_x(200,600,500,0), d.target_x_mm);  // default lead
    TEST_ASSERT_EQUAL_FLOAT(1.0f, d.kp_scale);
}

void test_gk_na_velocity_fallback_exact(void){
    // FALLBACK EXACTO #1: velocidad N/A / pelota quieta (vx=vy=0) → BT_STILL,
    // sin amenaza, y target == X actual (lead 0). KP sin escalar.
    GkDecision d = bt_decide_intercept(150, 600, 150,
                                       0, 0,
                                       true, 0.0f,
                                       600);
    TEST_ASSERT_EQUAL_INT(BT_STILL, d.kind);
    TEST_ASSERT_FALSE(d.threat);
    TEST_ASSERT_EQUAL_FLOAT(150.0f, d.target_x_mm);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, d.kp_scale);
}

void test_gk_no_goal_visible_disables_classification(void){
    // FALLBACK EXACTO #2: sin arco rival visible → toward_tol=0 deshabilita la
    // clasificación de arcos; una pelota en movimiento NO se marca amenaza aunque
    // su heading apunte al arco propio. Conducta idéntica a hoy (default params).
    GkDecision d = bt_decide_intercept(0, 600, 120,
                                       0, -500,          // iría al arco propio si hubiera geometría
                                       false, 0.0f,       // pero NO se ve el arco rival
                                       600);
    TEST_ASSERT_EQUAL_INT(BT_OTHER, d.kind);   // sin geometría: nunca TO_OWN/TO_OPP
    TEST_ASSERT_FALSE(d.threat);
    TEST_ASSERT_EQUAL_FLOAT(pred_default_x(0,600,0,-500), d.target_x_mm);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, d.kp_scale);                          // sin amenaza → sin refuerzo
}

void test_gk_own_goal_wrap_no_int16_overflow(void){
    // Borde: arco rival visto casi DETRÁS (170°) → own = 350° debe normalizar a
    // -10° (no desbordar int16 a *100). Pelota hacia +Y (al frente del robot,
    // hacia donde está el arco propio en -10°) debe clasificar TO_OWN_GOAL.
    // heading(vx=0,vy=+500)=0 cd; own=-10°=-1000 cd; d_own=1000<=4500 → own.
    GkDecision d = bt_decide_intercept(0, 600, 90,
                                       0, 500,
                                       true, 170.0f,
                                       600);
    TEST_ASSERT_EQUAL_INT(BT_TO_OWN_GOAL, d.kind);
    TEST_ASSERT_TRUE(d.threat);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, d.target_x_mm);   // vx=0 → sin lead lateral
    TEST_ASSERT_EQUAL_FLOAT(1.5f, d.kp_scale);      // amenaza → KP reforzado
}

void test_gk_not_threat_target_equals_default_pred(void){
    // INVARIANTE de no-regresión: en TODAS las clasificaciones NO-amenaza, el
    // INTERCEPT persigue la X de ball_predict con DEFAULT params (== conducta hoy)
    // y el KP no se escala (kp_scale=1.0). Garantiza FALLBACK EXACTO global salvo
    // en la única rama de amenaza (TO_OWN_GOAL), cubierta por los tests de threat.
    const int16_t vxs[] = {0,    0,   500, -500};   // sin vy=-500 (ése sería TO_OWN con arco visible)
    const int16_t vys[] = {500,  0,   0,    0};
    for (int i = 0; i < 4; ++i) {
        GkDecision d = bt_decide_intercept(50, 600, /*bx_pred ignorado*/333.0f,
                                           vxs[i], vys[i],
                                           true, 0.0f, 600);
        TEST_ASSERT_FALSE(d.threat);
        TEST_ASSERT_EQUAL_FLOAT(pred_default_x(50,600,vxs[i],vys[i]), d.target_x_mm);
        TEST_ASSERT_EQUAL_FLOAT(1.0f, d.kp_scale);
    }
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_no_motion_is_still);
    RUN_TEST(test_toward_opponent_goal);
    RUN_TEST(test_toward_own_goal);
    RUN_TEST(test_sideways_is_other);
    RUN_TEST(test_in_reach_flag);
    RUN_TEST(test_speed_threshold_boundary_strict);
    RUN_TEST(test_opp_goal_precedence_when_equidistant);
    RUN_TEST(test_gk_shot_to_own_goal_is_threat);
    RUN_TEST(test_gk_threat_with_lateral_vel_increases_lead);
    RUN_TEST(test_gk_shot_to_opp_goal_not_threat);
    RUN_TEST(test_gk_crossing_ball_is_other);
    RUN_TEST(test_gk_na_velocity_fallback_exact);
    RUN_TEST(test_gk_no_goal_visible_disables_classification);
    RUN_TEST(test_gk_own_goal_wrap_no_int16_overflow);
    RUN_TEST(test_gk_not_threat_target_equals_default_pred);
    return UNITY_END();
}
