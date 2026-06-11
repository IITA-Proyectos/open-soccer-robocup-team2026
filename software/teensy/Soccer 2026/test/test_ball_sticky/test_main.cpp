// test_ball_sticky — tests de la fusión "cámara pegajosa" (ball_sticky.h).
// Corre en host con: bash scripts/run-host-tests.sh test_ball_sticky
//
// Contrato:
//   • Solo una cámara ve → esa manda (conf 80) y queda TITULAR.
//   • Ambas ven (conflicto; imposible para una pelota física):
//       - titular manda; sin titular, la MÁS CERCANA al robot;
//       - la salida es EXACTAMENTE una de las dos observaciones (NUNCA el
//         promedio — el bug que este módulo reemplaza);
//       - confianza 60 (rebaja, no premio).
//   • Traspaso legítimo: la titular pierde la pelota y la otra la ve → cambio
//     instantáneo de titular.
//   • Nadie ve por > release_ms → titularidad expira (próximo conflicto se
//     resuelve por cercanía, no por historia vieja).
//   • Cámara muerta por watchdog (alive=false) se ignora aunque diga visible.

#include <unity.h>
#include "ball_sticky.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Helpers: observaciones ya en marco robot (como las produce cam_obs_to_robot_frame).
static CamObs obs(float x, float y, bool visible, uint8_t cam_id) {
    CamObs o{};
    o.x_mm = x; o.y_mm = y; o.visible = visible; o.camera_id = cam_id;
    return o;
}
static CamObs front(float x, float y, bool vis = true) { return obs(x, y, vis, 0); }
static CamObs back_(float x, float y, bool vis = true) { return obs(x, y, vis, 1); }

// ============================================================================
// Casos de una sola cámara (idénticos en espíritu a la fusión clásica)
// ============================================================================

void test_only_front_wins_and_becomes_holder(void) {
    BallStickyState s; ball_sticky_reset(s);
    const BallFused out = ball_sticky_fuse(s, ball_sticky_default_params(),
                                           front(100, 400), back_(0, 0, false),
                                           true, true, 1000);
    TEST_ASSERT_TRUE(out.visible);
    TEST_ASSERT_EQUAL_INT16(100, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(400, out.y_mm);
    TEST_ASSERT_EQUAL_UINT8(BS_CONF_SINGLE, out.confidence);
    TEST_ASSERT_EQUAL_UINT8(BS_HOLDER_FRONT, s.holder);
}

void test_only_back_wins_and_becomes_holder(void) {
    BallStickyState s; ball_sticky_reset(s);
    const BallFused out = ball_sticky_fuse(s, ball_sticky_default_params(),
                                           front(0, 0, false), back_(-50, -300),
                                           true, true, 1000);
    TEST_ASSERT_TRUE(out.visible);
    TEST_ASSERT_EQUAL_INT16(-50, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(-300, out.y_mm);
    TEST_ASSERT_EQUAL_UINT8(BS_HOLDER_BACK, s.holder);
}

void test_dead_camera_ignored_even_if_visible(void) {
    // Watchdog: front_alive=false → su "visible" es stale y no cuenta.
    BallStickyState s; ball_sticky_reset(s);
    const BallFused out = ball_sticky_fuse(s, ball_sticky_default_params(),
                                           front(100, 400), back_(-50, -300),
                                           /*front_alive=*/false, true, 1000);
    TEST_ASSERT_TRUE(out.visible);
    TEST_ASSERT_EQUAL_INT16(-50, out.x_mm);      // ganó la trasera (única viva)
    TEST_ASSERT_EQUAL_UINT8(BS_CONF_SINGLE, out.confidence);
}

void test_nobody_sees_invisible(void) {
    BallStickyState s; ball_sticky_reset(s);
    const BallFused out = ball_sticky_fuse(s, ball_sticky_default_params(),
                                           front(0, 0, false), back_(0, 0, false),
                                           true, true, 1000);
    TEST_ASSERT_FALSE(out.visible);
    TEST_ASSERT_EQUAL_UINT8(0, out.confidence);
}

// ============================================================================
// El conflicto (ambas ven) — el corazón del módulo
// ============================================================================

void test_conflict_holder_front_wins_no_average(void) {
    BallStickyState s; ball_sticky_reset(s);
    const BallStickyParams p = ball_sticky_default_params();
    // 1) La frontal venía viendo la pelota → titular FRONT.
    ball_sticky_fuse(s, p, front(0, 400), back_(0, 0, false), true, true, 1000);
    // 2) Aparece un naranja falso ATRÁS: ambas reportan.
    const BallFused out = ball_sticky_fuse(s, p,
                                           front(0, 400), back_(0, -600),
                                           true, true, 1033);
    // La clásica devolvía el PROMEDIO (0,-100): pelota fantasma DETRÁS.
    // La pegajosa: la titular (frontal) manda, posición EXACTA, conf rebajada.
    TEST_ASSERT_TRUE(out.visible);
    TEST_ASSERT_EQUAL_INT16(0, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(400, out.y_mm);
    TEST_ASSERT_EQUAL_UINT8(BS_CONF_CONFLICT, out.confidence);
    TEST_ASSERT_EQUAL_UINT8(BS_HOLDER_FRONT, s.holder);
}

void test_conflict_holder_back_wins(void) {
    BallStickyState s; ball_sticky_reset(s);
    const BallStickyParams p = ball_sticky_default_params();
    ball_sticky_fuse(s, p, front(0, 0, false), back_(0, -500), true, true, 1000);
    const BallFused out = ball_sticky_fuse(s, p,
                                           front(200, 300), back_(0, -500),
                                           true, true, 1033);
    TEST_ASSERT_EQUAL_INT16(0, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(-500, out.y_mm);
    TEST_ASSERT_EQUAL_UINT8(BS_CONF_CONFLICT, out.confidence);
}

void test_conflict_cold_start_nearest_wins(void) {
    // Sin historia (arranque en frío) ambas reportan: gana la MÁS CERCANA.
    BallStickyState s; ball_sticky_reset(s);
    const BallFused out = ball_sticky_fuse(s, ball_sticky_default_params(),
                                           front(0, 800), back_(0, -200),
                                           true, true, 1000);
    TEST_ASSERT_EQUAL_INT16(0, out.x_mm);
    TEST_ASSERT_EQUAL_INT16(-200, out.y_mm);     // 200 mm < 800 mm
    TEST_ASSERT_EQUAL_UINT8(BS_CONF_CONFLICT, out.confidence);
    TEST_ASSERT_EQUAL_UINT8(BS_HOLDER_BACK, s.holder);
}

void test_conflict_output_is_never_the_average(void) {
    // Propiedad clave: la salida coincide EXACTAMENTE con una observación.
    BallStickyState s; ball_sticky_reset(s);
    const BallFused out = ball_sticky_fuse(s, ball_sticky_default_params(),
                                           front(300, 400), back_(-300, -400),
                                           true, true, 1000);
    const bool is_front = (out.x_mm == 300 && out.y_mm == 400);
    const bool is_back  = (out.x_mm == -300 && out.y_mm == -400);
    TEST_ASSERT_TRUE(is_front || is_back);       // jamás (0,0) = el promedio
}

// ============================================================================
// Traspasos y expiración de titularidad
// ============================================================================

void test_legit_handoff_is_instant(void) {
    // La pelota rodea al robot: la frontal la pierde y la trasera la agarra →
    // el traspaso NO espera nada (caso "solo una ve").
    BallStickyState s; ball_sticky_reset(s);
    const BallStickyParams p = ball_sticky_default_params();
    ball_sticky_fuse(s, p, front(0, 400), back_(0, 0, false), true, true, 1000);
    const BallFused out = ball_sticky_fuse(s, p,
                                           front(0, 0, false), back_(0, -400),
                                           true, true, 1033);
    TEST_ASSERT_TRUE(out.visible);
    TEST_ASSERT_EQUAL_INT16(-400, out.y_mm);
    TEST_ASSERT_EQUAL_UINT8(BS_HOLDER_BACK, s.holder);
    TEST_ASSERT_EQUAL_UINT8(BS_CONF_SINGLE, out.confidence);
}

void test_holder_persists_through_short_blindness(void) {
    // Parpadeo corto (< release_ms): la titularidad se mantiene y el siguiente
    // conflicto lo sigue ganando la titular aunque la otra esté más cerca.
    BallStickyState s; ball_sticky_reset(s);
    const BallStickyParams p = ball_sticky_default_params();   // release 700 ms
    ball_sticky_fuse(s, p, front(0, 700), back_(0, 0, false), true, true, 1000);
    ball_sticky_fuse(s, p, front(0, 0, false), back_(0, 0, false), true, true, 1300);
    TEST_ASSERT_EQUAL_UINT8(BS_HOLDER_FRONT, s.holder);        // 300 ms < 700
    const BallFused out = ball_sticky_fuse(s, p,
                                           front(0, 700), back_(0, -100),
                                           true, true, 1400);
    TEST_ASSERT_EQUAL_INT16(700, out.y_mm);                    // titular manda
}

void test_holder_expires_after_release_ms(void) {
    // Ceguera larga (> release_ms): titularidad expirada → el conflicto se
    // resuelve por cercanía, no por la historia vieja.
    BallStickyState s; ball_sticky_reset(s);
    const BallStickyParams p = ball_sticky_default_params();
    ball_sticky_fuse(s, p, front(0, 700), back_(0, 0, false), true, true, 1000);
    ball_sticky_fuse(s, p, front(0, 0, false), back_(0, 0, false), true, true, 2000);
    TEST_ASSERT_EQUAL_UINT8(BS_HOLDER_NONE, s.holder);         // 1000 ms > 700
    const BallFused out = ball_sticky_fuse(s, p,
                                           front(0, 700), back_(0, -100),
                                           true, true, 2033);
    TEST_ASSERT_EQUAL_INT16(-100, out.y_mm);                   // ganó la cercana
}

void test_reset_clears_holder(void) {
    BallStickyState s; ball_sticky_reset(s);
    const BallStickyParams p = ball_sticky_default_params();
    ball_sticky_fuse(s, p, front(0, 400), back_(0, 0, false), true, true, 1000);
    ball_sticky_reset(s);
    TEST_ASSERT_EQUAL_UINT8(BS_HOLDER_NONE, s.holder);
    TEST_ASSERT_EQUAL_UINT32(0, s.last_seen_ms);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_only_front_wins_and_becomes_holder);
    RUN_TEST(test_only_back_wins_and_becomes_holder);
    RUN_TEST(test_dead_camera_ignored_even_if_visible);
    RUN_TEST(test_nobody_sees_invisible);

    RUN_TEST(test_conflict_holder_front_wins_no_average);
    RUN_TEST(test_conflict_holder_back_wins);
    RUN_TEST(test_conflict_cold_start_nearest_wins);
    RUN_TEST(test_conflict_output_is_never_the_average);

    RUN_TEST(test_legit_handoff_is_instant);
    RUN_TEST(test_holder_persists_through_short_blindness);
    RUN_TEST(test_holder_expires_after_release_ms);
    RUN_TEST(test_reset_clears_holder);

    return UNITY_END();
}
