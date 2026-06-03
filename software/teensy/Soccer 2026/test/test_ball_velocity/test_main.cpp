// test_ball_velocity — tests unitarios para src/shared/ball_velocity.cpp
// Corre en host con: pio test -e test_native -f test_ball_velocity
//                    bash scripts/run-host-tests.sh test_ball_velocity
//
// El estimador deriva velocidad (mm/s, marco robot) de la posición fusionada de
// la pelota. Reglas (ver ball_velocity.h):
//   • Sólo deriva cuando llega una muestra NUEVA (sample_ms > last_ms).
//   • Necesita 2 muestras → la primera sólo siembra (valid=false, v=0).
//   • Resetea al perder visibilidad (descarta el primer frame al reaparecer).
//   • Re-siembra (no clava un pico) si el gap entre muestras supera max_gap_ms.
//   • Suaviza con EMA (alpha) las derivadas sucesivas.
//   • EXPIRA por tiempo: si la pelota sigue `visible` pero los packets se cortan
//     (sample_ms no avanza) y now_ms-last_ms > max_gap_ms → invalida (v=0) para
//     no servir una velocidad fantasma; conserva last_x/y/ms y has_point para
//     re-derivar cuando vuelva el dato.

#include <unity.h>
#include "ball_velocity.h"

using namespace iitasoccer;

// Params explícitos para que los números den limpios e independientes del default.
static BallVelocityParams P;
static BallVelocityState  s;

void setUp(void) {
    P.ema_alpha  = 0.5f;   // mitad/mitad para EMA con cuentas redondas
    P.max_gap_ms = 200;    // gap > 200 ms → re-siembra
    ball_velocity_reset(s);
}
void tearDown(void) {}

// --- 1 muestra: todavía no hay velocidad ---------------------------------
void test_first_visible_sample_not_valid(void) {
    ball_velocity_update(s, P, 100, 50, /*visible=*/true, /*sample_ms=*/1000, /*now_ms=*/1000);
    TEST_ASSERT_FALSE(s.valid);
    TEST_ASSERT_EQUAL_INT16(0, ball_velocity_vx_mm_s(s));
    TEST_ASSERT_EQUAL_INT16(0, ball_velocity_vy_mm_s(s));
}

// --- 2 muestras moviéndose en +X: velocidad correcta ---------------------
void test_two_samples_x_velocity(void) {
    ball_velocity_update(s, P, 0,   0, true, 0,   0);
    ball_velocity_update(s, P, 100, 0, true, 100, 100);   // +100 mm en 100 ms → 1000 mm/s
    TEST_ASSERT_TRUE(s.valid);
    TEST_ASSERT_EQUAL_INT16(1000, ball_velocity_vx_mm_s(s));
    TEST_ASSERT_EQUAL_INT16(0,    ball_velocity_vy_mm_s(s));
}

// --- 2 muestras moviéndose en +Y -----------------------------------------
void test_two_samples_y_velocity(void) {
    ball_velocity_update(s, P, 0, 0,   true, 0,   0);
    ball_velocity_update(s, P, 0, 200, true, 100, 100);   // +200 mm en 100 ms → 2000 mm/s
    TEST_ASSERT_EQUAL_INT16(0,    ball_velocity_vx_mm_s(s));
    TEST_ASSERT_EQUAL_INT16(2000, ball_velocity_vy_mm_s(s));
}

// --- velocidad negativa (la pelota se acerca por -X) ----------------------
void test_negative_velocity(void) {
    ball_velocity_update(s, P, 100, 0, true, 0,   0);
    ball_velocity_update(s, P, 0,   0, true, 100, 100);   // -100 mm en 100 ms → -1000 mm/s
    TEST_ASSERT_EQUAL_INT16(-1000, ball_velocity_vx_mm_s(s));
}

// --- pelota quieta: válida pero v=0 (distinto de "no la veo") --------------
void test_stationary_is_valid_zero(void) {
    ball_velocity_update(s, P, 50, 50, true, 0,   0);
    ball_velocity_update(s, P, 50, 50, true, 100, 100);
    TEST_ASSERT_TRUE(s.valid);
    TEST_ASSERT_EQUAL_INT16(0, ball_velocity_vx_mm_s(s));
    TEST_ASSERT_EQUAL_INT16(0, ball_velocity_vy_mm_s(s));
}

// --- EMA: la 2ª derivada se mezcla con la 1ª ------------------------------
void test_ema_blends_second_derivative(void) {
    ball_velocity_update(s, P, 0,   0, true, 0,   0);
    ball_velocity_update(s, P, 100, 0, true, 100, 100);   // siembra vx=1000
    ball_velocity_update(s, P, 300, 0, true, 200, 200);   // inst=2000; EMA .5 → 1500
    TEST_ASSERT_EQUAL_INT16(1500, ball_velocity_vx_mm_s(s));
}

// --- perder la pelota resetea --------------------------------------------
void test_not_visible_resets(void) {
    ball_velocity_update(s, P, 0,   0, true, 0,   0);
    ball_velocity_update(s, P, 100, 0, true, 100, 100);   // vx=1000, valid
    ball_velocity_update(s, P, 0,   0, /*visible=*/false, 200, 200);
    TEST_ASSERT_FALSE(s.valid);
    TEST_ASSERT_EQUAL_INT16(0, ball_velocity_vx_mm_s(s));
    TEST_ASSERT_EQUAL_INT16(0, ball_velocity_vy_mm_s(s));
}

// --- al reaparecer NO clava un pico (el primer frame sólo re-siembra) -----
void test_reappear_no_spike(void) {
    ball_velocity_update(s, P, 0,   0, true, 0,   0);
    ball_velocity_update(s, P, 100, 0, true, 100, 100);   // vx=1000
    ball_velocity_update(s, P, 0,   0, false, 200, 200);  // pierde
    ball_velocity_update(s, P, 5000, 0, true, 300, 300);  // reaparece LEJOS → no spike
    TEST_ASSERT_FALSE(s.valid);
    TEST_ASSERT_EQUAL_INT16(0, ball_velocity_vx_mm_s(s));
    ball_velocity_update(s, P, 5100, 0, true, 400, 400);  // recién acá vuelve a derivar
    TEST_ASSERT_TRUE(s.valid);
    TEST_ASSERT_EQUAL_INT16(1000, ball_velocity_vx_mm_s(s));
}

// --- misma muestra (sample_ms no avanzó) → no-op --------------------------
void test_no_new_sample_is_noop(void) {
    ball_velocity_update(s, P, 0,   0, true, 0,   0);
    ball_velocity_update(s, P, 100, 0, true, 100, 100);          // vx=1000
    // mismo sample_ms y poco tiempo → ignorar (no expira: 110-100 <= 200)
    ball_velocity_update(s, P, 999, 999, true, 100, 110);
    TEST_ASSERT_TRUE(s.valid);
    TEST_ASSERT_EQUAL_INT16(1000, ball_velocity_vx_mm_s(s));
}

// --- gap largo (> max_gap_ms) re-siembra en vez de spikear ----------------
void test_large_gap_reseeds(void) {
    ball_velocity_update(s, P, 0,   0, true, 0,   0);
    ball_velocity_update(s, P, 100, 0, true, 100, 100);   // vx=1000, valid
    ball_velocity_update(s, P, 500, 0, true, 400, 400);   // dt=300 > 200 → re-siembra
    TEST_ASSERT_FALSE(s.valid);
    TEST_ASSERT_EQUAL_INT16(0, ball_velocity_vx_mm_s(s));
    ball_velocity_update(s, P, 600, 0, true, 450, 450);   // dt=50 → 2000 mm/s
    TEST_ASSERT_TRUE(s.valid);
    TEST_ASSERT_EQUAL_INT16(2000, ball_velocity_vx_mm_s(s));
}

// --- el getter clampea a rango int16 (anti-overflow) ----------------------
void test_getter_clamps_positive(void) {
    ball_velocity_update(s, P, 0,     0, true, 0, 0);
    ball_velocity_update(s, P, 30000, 0, true, 1, 1);   // 3e7 mm/s → satura
    TEST_ASSERT_EQUAL_INT16(32767, ball_velocity_vx_mm_s(s));
}
void test_getter_clamps_negative(void) {
    ball_velocity_update(s, P, 0,      0, true, 0, 0);
    ball_velocity_update(s, P, -30000, 0, true, 1, 1);
    TEST_ASSERT_EQUAL_INT16(-32768, ball_velocity_vx_mm_s(s));
}

// --- los params por defecto son sanos ------------------------------------
void test_default_params_sane(void) {
    BallVelocityParams d = ball_velocity_default_params();
    TEST_ASSERT_TRUE(d.ema_alpha > 0.0f && d.ema_alpha <= 1.0f);
    TEST_ASSERT_TRUE(d.max_gap_ms > 0);
}

// --- (a) velocidad válida que EXPIRA por tiempo --------------------------
// 2 muestras → valid con vx; luego el packet se corta (mismo sample_ms) pero la
// pelota sigue visible y now_ms avanza > max_gap → invalidar (v=0).
void test_valid_velocity_expires_on_time_gap(void) {
    ball_velocity_update(s, P, 0,   0, true, 0,   0);
    ball_velocity_update(s, P, 100, 0, true, 100, 100);   // vx=1000, valid
    TEST_ASSERT_TRUE(s.valid);
    // packets cortados: sample_ms NO avanza, pero now_ms - last_ms = 350 > 200
    ball_velocity_update(s, P, 100, 0, true, 100, 450);
    TEST_ASSERT_FALSE(s.valid);
    TEST_ASSERT_EQUAL_INT16(0, ball_velocity_vx_mm_s(s));
    TEST_ASSERT_EQUAL_INT16(0, ball_velocity_vy_mm_s(s));
    // conserva el punto de referencia para re-derivar (no borra has_point)
    TEST_ASSERT_TRUE(s.has_point);
}

// --- (b) NO expira mientras now_ms-last_ms <= max_gap --------------------
void test_does_not_expire_within_gap(void) {
    ball_velocity_update(s, P, 0,   0, true, 0,   0);
    ball_velocity_update(s, P, 100, 0, true, 100, 100);   // vx=1000, valid
    // misma muestra, poco tiempo: 100 + 200 = 300 ; 300-100 = 200 <= 200 → sigue valid
    ball_velocity_update(s, P, 100, 0, true, 100, 300);
    TEST_ASSERT_TRUE(s.valid);
    TEST_ASSERT_EQUAL_INT16(1000, ball_velocity_vx_mm_s(s));
}

// --- (c) tras expirar, una muestra nueva re-deriva normal ----------------
void test_reseed_after_expiry_derives_again(void) {
    ball_velocity_update(s, P, 0,   0, true, 0,   0);
    ball_velocity_update(s, P, 100, 0, true, 100, 100);   // vx=1000, valid
    ball_velocity_update(s, P, 100, 0, true, 100, 450);   // expira (gap de tiempo) → invalid
    TEST_ASSERT_FALSE(s.valid);
    // llega una muestra nueva: dt = 500-100 = 400 > 200 → re-siembra (no spike)
    ball_velocity_update(s, P, 300, 0, true, 500, 500);
    TEST_ASSERT_FALSE(s.valid);
    TEST_ASSERT_EQUAL_INT16(0, ball_velocity_vx_mm_s(s));
    // y la siguiente ya deriva normal: dt = 600-500 = 100 → +100 mm → 1000 mm/s
    ball_velocity_update(s, P, 400, 0, true, 600, 600);
    TEST_ASSERT_TRUE(s.valid);
    TEST_ASSERT_EQUAL_INT16(1000, ball_velocity_vx_mm_s(s));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_first_visible_sample_not_valid);
    RUN_TEST(test_two_samples_x_velocity);
    RUN_TEST(test_two_samples_y_velocity);
    RUN_TEST(test_negative_velocity);
    RUN_TEST(test_stationary_is_valid_zero);
    RUN_TEST(test_ema_blends_second_derivative);
    RUN_TEST(test_not_visible_resets);
    RUN_TEST(test_reappear_no_spike);
    RUN_TEST(test_no_new_sample_is_noop);
    RUN_TEST(test_large_gap_reseeds);
    RUN_TEST(test_getter_clamps_positive);
    RUN_TEST(test_getter_clamps_negative);
    RUN_TEST(test_default_params_sane);
    RUN_TEST(test_valid_velocity_expires_on_time_gap);
    RUN_TEST(test_does_not_expire_within_gap);
    RUN_TEST(test_reseed_after_expiry_derives_again);
    return UNITY_END();
}
