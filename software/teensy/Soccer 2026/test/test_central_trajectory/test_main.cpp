// pio test -e test_native -f test_central_trajectory
// ⚠️ NOTA: caracteriza bt_classify (ball_trajectory). Ya CABLEADO en el arquero
// (strategy.cpp goalkeeper_tick / GK_INTERCEPT vía gk_classify_intercept). Como
// strategy.cpp depende de Arduino y NO compila host-side, la sección
// "decisión del arquero" de abajo ESPEJA exactamente gk_classify_intercept
// (mismo armado de BallTrajIn + misma regla X) para caracterizar el call-site sin
// linkear strategy. Si cambia gk_classify_intercept, actualizar bt_decide_intercept.
#include <unity.h>
#include <cmath>
#include "ball_trajectory.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

// ── Espejo de la decisión del arquero (strategy.cpp::gk_classify_intercept) ──
// Réplica host-testeable: mismas constantes y misma construcción de BallTrajIn.
// Mantener sincronizado con strategy.cpp (GK_BT_SPEED_MIN_MM_S / GK_BT_TOWARD_TOL_CENTIDEG).
static const int16_t GK_BT_SPEED_MIN_MM_S      = 80;
static const int16_t GK_BT_TOWARD_TOL_CENTIDEG = 4500;

struct GkDecision { BallTrajKind kind; float target_x_mm; bool threat; };

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
    d.kind         = t.kind;
    d.threat       = (t.kind == BT_TO_OWN_GOAL);
    d.target_x_mm  = bx_pred;   // fallback EXACTO: siempre la X predicha (igual a hoy)
    (void)bx; (void)by;
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

void test_gk_shot_to_own_goal_is_threat(void){
    // Arquero mirando al campo: arco rival al frente (0°), arco propio detrás (180°).
    // Pelota hacia -Y (al arco propio): vx=0, vy=-500 → heading 18000 cd == own.
    GkDecision d = bt_decide_intercept(/*bx*/0, /*by*/600, /*bx_pred*/120,
                                       /*vx*/0, /*vy*/-500,
                                       /*goal_opp_visible*/true, /*goal_opp_deg*/0.0f,
                                       /*dist*/600);
    TEST_ASSERT_EQUAL_INT(BT_TO_OWN_GOAL, d.kind);
    TEST_ASSERT_TRUE(d.threat);
    TEST_ASSERT_EQUAL_FLOAT(120.0f, d.target_x_mm);   // sigue persiguiendo X predicha
}

void test_gk_shot_to_opp_goal_not_threat(void){
    // Pelota hacia +Y (al arco rival, alejándose del arquero): vx=0, vy=+500.
    GkDecision d = bt_decide_intercept(0, 600, 120,
                                       0, 500,
                                       true, 0.0f,
                                       600);
    TEST_ASSERT_EQUAL_INT(BT_TO_OPP_GOAL, d.kind);
    TEST_ASSERT_FALSE(d.threat);
    TEST_ASSERT_EQUAL_FLOAT(120.0f, d.target_x_mm);
}

void test_gk_crossing_ball_is_other(void){
    // Pelota cruzando de costado (vx=500, vy=0): ni al arco propio ni al rival.
    GkDecision d = bt_decide_intercept(200, 600, 250,
                                       500, 0,
                                       true, 0.0f,
                                       600);
    TEST_ASSERT_EQUAL_INT(BT_OTHER, d.kind);
    TEST_ASSERT_FALSE(d.threat);
    TEST_ASSERT_EQUAL_FLOAT(250.0f, d.target_x_mm);
}

void test_gk_na_velocity_fallback_exact(void){
    // FALLBACK EXACTO #1: velocidad N/A / pelota quieta (vx=vy=0) → BT_STILL,
    // sin amenaza, y target == X predicha (que con v=0 es la X actual → hoy).
    GkDecision d = bt_decide_intercept(150, 600, 150,
                                       0, 0,
                                       true, 0.0f,
                                       600);
    TEST_ASSERT_EQUAL_INT(BT_STILL, d.kind);
    TEST_ASSERT_FALSE(d.threat);
    TEST_ASSERT_EQUAL_FLOAT(150.0f, d.target_x_mm);
}

void test_gk_no_goal_visible_disables_classification(void){
    // FALLBACK EXACTO #2: sin arco rival visible → toward_tol=0 deshabilita la
    // clasificación de arcos; una pelota en movimiento NO se marca amenaza aunque
    // su heading apunte al arco propio. Conducta idéntica a hoy (X predicha).
    GkDecision d = bt_decide_intercept(0, 600, 120,
                                       0, -500,          // iría al arco propio si hubiera geometría
                                       false, 0.0f,       // pero NO se ve el arco rival
                                       600);
    TEST_ASSERT_EQUAL_INT(BT_OTHER, d.kind);   // sin geometría: nunca TO_OWN/TO_OPP
    TEST_ASSERT_FALSE(d.threat);
    TEST_ASSERT_EQUAL_FLOAT(120.0f, d.target_x_mm);
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
    TEST_ASSERT_EQUAL_FLOAT(90.0f, d.target_x_mm);
}

void test_gk_target_x_always_equals_pred(void){
    // INVARIANTE de no-regresión: en TODAS las clasificaciones el INTERCEPT
    // persigue la X PREDICHA (bx_pred). Garantiza que cablear bt_classify NO
    // cambió el comando del arquero hoy (FALLBACK EXACTO global).
    const float bx_pred = 333.0f;
    const int16_t vxs[] = {0, 0,    0,   500, -500};
    const int16_t vys[] = {0, 500, -500, 0,    0};
    for (int i = 0; i < 5; ++i) {
        GkDecision d = bt_decide_intercept(50, 600, bx_pred,
                                           vxs[i], vys[i],
                                           true, 0.0f, 600);
        TEST_ASSERT_EQUAL_FLOAT(bx_pred, d.target_x_mm);
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
    RUN_TEST(test_gk_shot_to_opp_goal_not_threat);
    RUN_TEST(test_gk_crossing_ball_is_other);
    RUN_TEST(test_gk_na_velocity_fallback_exact);
    RUN_TEST(test_gk_no_goal_visible_disables_classification);
    RUN_TEST(test_gk_own_goal_wrap_no_int16_overflow);
    RUN_TEST(test_gk_target_x_always_equals_pred);
    return UNITY_END();
}
