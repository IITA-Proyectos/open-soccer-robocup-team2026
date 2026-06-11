// test_atk_nogyro — tests unitarios de los helpers del delantero SIN GYRO
// (práctica R1 2026-06-12; src/shared/atk_nogyro.h).
// Corre en host con: bash scripts/run-host-tests.sh test_atk_nogyro
//
// Contrato:
//   nogyro_attack_axis: prioridad cámara > BNO > OTOS > inválido; los fallbacks
//     BNO/OTOS devuelven -heading normalizado (apuntar al 0 absoluto del boot).
//   nogyro_yaw_hold_omega_degps: P puro hacia hdg0, CCW+, con clamp simétrico y
//     bail-out (error grande → 0) y sin dato fresco → 0.
//   nogyro_push_dir: vector unitario a la pelota; degenerado → frente (0,1).
//   nogyro_obstacle_gate_vy: corta SOLO avance (vy>0) con obstáculo < umbral;
//     65535 (sin lectura) y snapshot stale NO frenan.

#include <unity.h>
#include <cmath>
#include "atk_nogyro.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// nogyro_attack_axis — cascada de fuentes del eje de ataque
// ============================================================================

void test_axis_camera_wins_over_everything(void) {
    // Cámara ve el arco → su ángulo manda, aunque BNO y OTOS estén disponibles.
    NoGyroAxis ax = nogyro_attack_axis(true, 37.0f, true, 90.0f, true, -90.0f);
    TEST_ASSERT_TRUE(ax.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 37.0f, ax.rel_deg);
}

void test_axis_bno_fallback_negates_heading(void) {
    // Sin cámara, BNO válido: robot girado +30 (CCW) → el 0 absoluto queda a -30.
    NoGyroAxis ax = nogyro_attack_axis(false, 0.0f, true, 30.0f, true, 999.0f);
    TEST_ASSERT_TRUE(ax.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -30.0f, ax.rel_deg);
}

void test_axis_otos_fallback_when_no_bno(void) {
    // Sin cámara ni BNO (el caso R1): el yaw del OTOS toma el rol del BNO.
    NoGyroAxis ax = nogyro_attack_axis(false, 0.0f, false, 0.0f, true, 45.0f);
    TEST_ASSERT_TRUE(ax.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -45.0f, ax.rel_deg);
}

void test_axis_otos_fallback_wraps(void) {
    // OTOS acumula vueltas: heading 350 → -350 normalizado = +10 (camino corto).
    NoGyroAxis ax = nogyro_attack_axis(false, 0.0f, false, 0.0f, true, 350.0f);
    TEST_ASSERT_TRUE(ax.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ax.rel_deg);
}

void test_axis_invalid_without_any_source(void) {
    NoGyroAxis ax = nogyro_attack_axis(false, 0.0f, false, 0.0f, false, 0.0f);
    TEST_ASSERT_FALSE(ax.valid);
}

// ============================================================================
// nogyro_yaw_hold_omega_degps — P con clamp y bail-out
// ============================================================================

void test_yaw_hold_zero_error_zero_omega(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
        nogyro_yaw_hold_omega_degps(true, 10.0f, 10.0f, 3.0f, 40.0f, 45.0f));
}

void test_yaw_hold_sign_reduces_error(void) {
    // hdg0=0, hdg=-10 (giró CW) → err=+10 → omega POSITIVO (CCW) lo trae de vuelta.
    const float w = nogyro_yaw_hold_omega_degps(true, 0.0f, -10.0f, 3.0f, 40.0f, 45.0f);
    TEST_ASSERT_TRUE(w > 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.0f, w);
    // Simétrico: giró CCW → omega negativo.
    const float w2 = nogyro_yaw_hold_omega_degps(true, 0.0f, 10.0f, 3.0f, 40.0f, 45.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -30.0f, w2);
}

void test_yaw_hold_clamps_to_max(void) {
    // err=20, kp=3 → 60 → clamp a 40.
    const float w = nogyro_yaw_hold_omega_degps(true, 0.0f, -20.0f, 3.0f, 40.0f, 45.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 40.0f, w);
}

void test_yaw_hold_bailout_on_big_error(void) {
    // err=90 > bailout 45 → 0 (no perseguir signos invertidos / yaw basura).
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
        nogyro_yaw_hold_omega_degps(true, 0.0f, -90.0f, 3.0f, 40.0f, 45.0f));
}

void test_yaw_hold_zero_when_not_fresh(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
        nogyro_yaw_hold_omega_degps(false, 0.0f, -10.0f, 3.0f, 40.0f, 45.0f));
}

void test_yaw_hold_wraps_across_180(void) {
    // hdg0=170, hdg=-170 → camino corto err=-20 → omega negativo (no +1020).
    const float w = nogyro_yaw_hold_omega_degps(true, 170.0f, -170.0f, 1.0f, 40.0f, 45.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -20.0f, w);
}

// ============================================================================
// nogyro_push_dir — vector unitario congelado
// ============================================================================

void test_push_dir_unit_vector(void) {
    NoGyroPushDir d = nogyro_push_dir(30.0f, 40.0f);   // 3-4-5
    TEST_ASSERT_TRUE(d.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.6f, d.ux);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, d.uy);
}

void test_push_dir_degenerate_defaults_forward(void) {
    NoGyroPushDir d = nogyro_push_dir(0.0f, 0.0f);
    TEST_ASSERT_FALSE(d.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, d.ux);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, d.uy);
}

void test_push_dir_backward_ball(void) {
    // Pelota detrás (cámara trasera): el empuje va hacia atrás — válido, el
    // omni empuja con el paragolpes trasero.
    NoGyroPushDir d = nogyro_push_dir(0.0f, -200.0f);
    TEST_ASSERT_TRUE(d.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, d.ux);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, d.uy);
}

// ============================================================================
// nogyro_obstacle_gate_vy — freno anti-choque
// ============================================================================

void test_gate_cuts_forward_when_obstacle_close(void) {
    TEST_ASSERT_EQUAL_INT16(0, nogyro_obstacle_gate_vy(400, true, 180, 250));
}

void test_gate_passes_when_obstacle_far(void) {
    TEST_ASSERT_EQUAL_INT16(400, nogyro_obstacle_gate_vy(400, true, 600, 250));
}

void test_gate_ignores_no_reading_sentinel(void) {
    // 65535 = sensores sin lectura → NO inventa un freno.
    TEST_ASSERT_EQUAL_INT16(400, nogyro_obstacle_gate_vy(400, true, 65535, 250));
}

void test_gate_only_governs_forward(void) {
    // Retroceso y quieto pasan intactos aunque el obstáculo esté encima.
    TEST_ASSERT_EQUAL_INT16(-300, nogyro_obstacle_gate_vy(-300, true, 50, 250));
    TEST_ASSERT_EQUAL_INT16(0,    nogyro_obstacle_gate_vy(0,    true, 50, 250));
}

void test_gate_passes_when_snapshot_stale(void) {
    // Mundo viejo → la decisión no es de este gate (el watchdog de snapshot ya
    // para los motores en main_central).
    TEST_ASSERT_EQUAL_INT16(400, nogyro_obstacle_gate_vy(400, false, 50, 250));
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_axis_camera_wins_over_everything);
    RUN_TEST(test_axis_bno_fallback_negates_heading);
    RUN_TEST(test_axis_otos_fallback_when_no_bno);
    RUN_TEST(test_axis_otos_fallback_wraps);
    RUN_TEST(test_axis_invalid_without_any_source);

    RUN_TEST(test_yaw_hold_zero_error_zero_omega);
    RUN_TEST(test_yaw_hold_sign_reduces_error);
    RUN_TEST(test_yaw_hold_clamps_to_max);
    RUN_TEST(test_yaw_hold_bailout_on_big_error);
    RUN_TEST(test_yaw_hold_zero_when_not_fresh);
    RUN_TEST(test_yaw_hold_wraps_across_180);

    RUN_TEST(test_push_dir_unit_vector);
    RUN_TEST(test_push_dir_degenerate_defaults_forward);
    RUN_TEST(test_push_dir_backward_ball);

    RUN_TEST(test_gate_cuts_forward_when_obstacle_close);
    RUN_TEST(test_gate_passes_when_obstacle_far);
    RUN_TEST(test_gate_ignores_no_reading_sentinel);
    RUN_TEST(test_gate_only_governs_forward);
    RUN_TEST(test_gate_passes_when_snapshot_stale);

    return UNITY_END();
}
