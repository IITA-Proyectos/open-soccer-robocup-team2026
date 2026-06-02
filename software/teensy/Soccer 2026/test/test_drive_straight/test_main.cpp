// test_drive_straight — tests unitarios para el módulo drive_straight (WP-2A, Capa 2).
// Corre en host con: pio test -e test_native -f test_drive_straight
//                o:  bash scripts/run-host-tests.sh test_drive_straight
//
// Contrato (definido por estos tests, TDD-first):
//   Ley de control (frente = +Y, +X = derecha, omega CCW+):
//     omega = kp_heading * angdiff(target_heading, cur_heading)   [normalizado ±180]
//     vy    = -kp_lateral * otos_vy_mm_s   (cancela la deriva lateral medida por OTOS)
//     vx    = fwd_speed_mm_s               (avance recto se respeta tal cual)
//
//   • Sin error de heading y sin deriva  -> omega ~ 0, vy ~ 0, vx = fwd.
//   • Error de heading +  -> omega del signo que REDUCE el error (CCW+).
//   • otos_vy > 0         -> vy correctiva < 0 (signo opuesto).
//   • fwd_speed se respeta exactamente en vx.

#include <unity.h>
#include <cmath>
#include "drive_straight.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Config de referencia para los tests (ganancias != 0 para ver el signo).
static DriveStraightCfg make_cfg() {
    DriveStraightCfg cfg;
    cfg.kp_heading = 3.0f;   // grados/s por grado de error
    cfg.kp_lateral = 1.0f;   // (mm/s correctivo) por (mm/s de deriva medida)
    return cfg;
}

// ============================================================================
// Caso nominal: sin error de heading, sin deriva lateral
// ============================================================================

void test_no_error_zero_omega_zero_vy_fwd_vx(void) {
    DriveStraightCfg cfg = make_cfg();
    DriveStraightIn in;
    in.target_heading_deg = 0.0f;
    in.cur_heading_deg    = 0.0f;
    in.otos_vy_mm_s       = 0.0f;
    in.fwd_speed_mm_s     = 400.0f;

    DriveStraightCmd cmd = drive_straight_compute(in, cfg);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmd.omega_deg_s);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmd.vy_mm_s);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 400.0f, cmd.vx_mm_s);
}

void test_no_error_holds_for_nonzero_heading(void) {
    // Si target == cur (ambos 90°), el error sigue siendo 0 -> omega ~ 0.
    DriveStraightCfg cfg = make_cfg();
    DriveStraightIn in;
    in.target_heading_deg = 90.0f;
    in.cur_heading_deg    = 90.0f;
    in.otos_vy_mm_s       = 0.0f;
    in.fwd_speed_mm_s     = 250.0f;

    DriveStraightCmd cmd = drive_straight_compute(in, cfg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmd.omega_deg_s);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 250.0f, cmd.vx_mm_s);
}

// ============================================================================
// Corrección de heading: signo correcto (reduce el error)
// ============================================================================

void test_positive_heading_error_positive_omega(void) {
    // target adelante de cur (target=30, cur=0) -> error +30.
    // Con CCW+, omega debe ser positivo para girar hacia +heading y reducir el error.
    DriveStraightCfg cfg = make_cfg();
    DriveStraightIn in;
    in.target_heading_deg = 30.0f;
    in.cur_heading_deg    = 0.0f;
    in.otos_vy_mm_s       = 0.0f;
    in.fwd_speed_mm_s     = 0.0f;

    DriveStraightCmd cmd = drive_straight_compute(in, cfg);
    TEST_ASSERT_TRUE(cmd.omega_deg_s > 0.0f);
    // Magnitud esperada = kp_heading * 30 = 90.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 90.0f, cmd.omega_deg_s);
}

void test_negative_heading_error_negative_omega(void) {
    // target detrás de cur (target=0, cur=30) -> error -30 -> omega negativo.
    DriveStraightCfg cfg = make_cfg();
    DriveStraightIn in;
    in.target_heading_deg = 0.0f;
    in.cur_heading_deg    = 30.0f;
    in.otos_vy_mm_s       = 0.0f;
    in.fwd_speed_mm_s     = 0.0f;

    DriveStraightCmd cmd = drive_straight_compute(in, cfg);
    TEST_ASSERT_TRUE(cmd.omega_deg_s < 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -90.0f, cmd.omega_deg_s);
}

void test_heading_error_wraps_across_180(void) {
    // target=170, cur=-170 -> error "corto" = -20 (no +340). omega negativo, pequeño.
    DriveStraightCfg cfg = make_cfg();
    DriveStraightIn in;
    in.target_heading_deg = 170.0f;
    in.cur_heading_deg    = -170.0f;
    in.otos_vy_mm_s       = 0.0f;
    in.fwd_speed_mm_s     = 0.0f;

    DriveStraightCmd cmd = drive_straight_compute(in, cfg);
    TEST_ASSERT_TRUE(cmd.omega_deg_s < 0.0f);
    // |error| corto = 20 -> |omega| = 60, NO 1020 (mal interpretado sin wrap).
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -60.0f, cmd.omega_deg_s);
}

// ============================================================================
// Cancelación de deriva lateral (OTOS vy)
// ============================================================================

void test_positive_otos_vy_negative_correction(void) {
    // Deriva medida +200 -> corrección vy NEGATIVA (se opone a la deriva).
    DriveStraightCfg cfg = make_cfg();
    DriveStraightIn in;
    in.target_heading_deg = 0.0f;
    in.cur_heading_deg    = 0.0f;
    in.otos_vy_mm_s       = 200.0f;
    in.fwd_speed_mm_s     = 0.0f;

    DriveStraightCmd cmd = drive_straight_compute(in, cfg);
    TEST_ASSERT_TRUE(cmd.vy_mm_s < 0.0f);
    // kp_lateral=1 -> vy = -1 * 200 = -200.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -200.0f, cmd.vy_mm_s);
}

void test_negative_otos_vy_positive_correction(void) {
    // Deriva medida -150 -> corrección vy POSITIVA.
    DriveStraightCfg cfg = make_cfg();
    DriveStraightIn in;
    in.target_heading_deg = 0.0f;
    in.cur_heading_deg    = 0.0f;
    in.otos_vy_mm_s       = -150.0f;
    in.fwd_speed_mm_s     = 0.0f;

    DriveStraightCmd cmd = drive_straight_compute(in, cfg);
    TEST_ASSERT_TRUE(cmd.vy_mm_s > 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 150.0f, cmd.vy_mm_s);
}

// ============================================================================
// fwd_speed se respeta (no lo toca la corrección)
// ============================================================================

void test_fwd_speed_respected_independent_of_corrections(void) {
    // Aún con error de heading y deriva, vx == fwd_speed exacto.
    DriveStraightCfg cfg = make_cfg();
    DriveStraightIn in;
    in.target_heading_deg = 45.0f;
    in.cur_heading_deg    = 0.0f;
    in.otos_vy_mm_s       = 100.0f;
    in.fwd_speed_mm_s     = 533.0f;

    DriveStraightCmd cmd = drive_straight_compute(in, cfg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 533.0f, cmd.vx_mm_s);
    // y las correcciones siguen presentes con su signo:
    TEST_ASSERT_TRUE(cmd.omega_deg_s > 0.0f);   // error +45
    TEST_ASSERT_TRUE(cmd.vy_mm_s < 0.0f);       // deriva +100
}

void test_zero_gains_pass_through_fwd_only(void) {
    // Ganancias 0 -> drive_straight degenera a "solo avanzar" (omega=0, vy=0).
    DriveStraightCfg cfg;
    cfg.kp_heading = 0.0f;
    cfg.kp_lateral = 0.0f;
    DriveStraightIn in;
    in.target_heading_deg = 90.0f;
    in.cur_heading_deg    = 0.0f;
    in.otos_vy_mm_s       = 300.0f;
    in.fwd_speed_mm_s     = 480.0f;

    DriveStraightCmd cmd = drive_straight_compute(in, cfg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmd.omega_deg_s);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmd.vy_mm_s);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 480.0f, cmd.vx_mm_s);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_no_error_zero_omega_zero_vy_fwd_vx);
    RUN_TEST(test_no_error_holds_for_nonzero_heading);

    RUN_TEST(test_positive_heading_error_positive_omega);
    RUN_TEST(test_negative_heading_error_negative_omega);
    RUN_TEST(test_heading_error_wraps_across_180);

    RUN_TEST(test_positive_otos_vy_negative_correction);
    RUN_TEST(test_negative_otos_vy_positive_correction);

    RUN_TEST(test_fwd_speed_respected_independent_of_corrections);
    RUN_TEST(test_zero_gains_pass_through_fwd_only);

    return UNITY_END();
}
