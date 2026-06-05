// test_pids — tests unitarios para los PIDs de la placa CENTRAL.
// Corre en host con: pio test -e test_native -f test_pids

#include <unity.h>
#include <cmath>
#include "pids.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// wrap_diff_deg
// ============================================================================

void test_wrap_diff_zero_diff(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, wrap_diff_deg(0.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, wrap_diff_deg(45.0f, 45.0f));
}

void test_wrap_diff_small_positive(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.0f, wrap_diff_deg(45.0f, 15.0f));
}

void test_wrap_diff_small_negative(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -30.0f, wrap_diff_deg(15.0f, 45.0f));
}

void test_wrap_diff_wraps_across_180(void) {
    // setpoint=170, current=-170 → diff "corta" es +20° (no -340°).
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -20.0f, wrap_diff_deg(170.0f, -170.0f));
    // setpoint=-170, current=170 → diff "corta" es -20° (no +340°).
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, wrap_diff_deg(-170.0f, 170.0f));
}

void test_wrap_diff_exact_180(void) {
    // Caso edge: diff = ±180. El algoritmo elige uno consistente.
    float r = wrap_diff_deg(180.0f, 0.0f);
    TEST_ASSERT_TRUE(std::abs(r) <= 180.0f);
}

// ============================================================================
// HeadingPID
// ============================================================================

void test_heading_pid_zero_error_zero_output(void) {
    HeadingPID pid;
    pid.setpoint_deg = 0.0f;
    float out = heading_pid_tick(pid, 0.0f, 0);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, out);
}

void test_heading_pid_positive_error_positive_output(void) {
    HeadingPID pid;
    pid.setpoint_deg = 90.0f;
    float out = heading_pid_tick(pid, 0.0f, 0);
    // Error = +90, Kp=3 → output esperado ≈ 270 grados/s (sin integral/derivada en primer tick).
    TEST_ASSERT_TRUE(out > 100.0f);   // dirección correcta
    TEST_ASSERT_TRUE(out > 200.0f);
}

void test_heading_pid_output_clamped(void) {
    HeadingPID pid;
    pid.output_clamp = 360.0f;
    pid.setpoint_deg = 180.0f;
    float out = heading_pid_tick(pid, -180.0f, 0);
    // Error = ±180 (wrapped), Kp=3 → tendería a 540 deg/s, pero clamp = 360.
    TEST_ASSERT_TRUE(out <= 361.0f);   // tolerancia
    TEST_ASSERT_TRUE(out >= -361.0f);
}

void test_heading_pid_reset_zeros_state(void) {
    HeadingPID pid;
    pid.setpoint_deg = 90.0f;
    // Acumulamos integral con varios ticks.
    heading_pid_tick(pid, 0.0f, 0);
    heading_pid_tick(pid, 0.0f, 10);
    heading_pid_tick(pid, 0.0f, 20);
    TEST_ASSERT_TRUE(pid.integral != 0.0f);
    TEST_ASSERT_TRUE(pid.primed);

    heading_pid_reset(pid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid.integral);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid.prev_error);
    TEST_ASSERT_FALSE(pid.primed);
}

void test_heading_pid_handles_wrap_around(void) {
    HeadingPID pid;
    pid.setpoint_deg = 170.0f;
    // Current = -170 → error "corto" debe ser -20° (no +340°).
    float out = heading_pid_tick(pid, -170.0f, 0);
    // Output negativo (porque error es negativo según wrap_diff_deg(170, -170) = -20).
    TEST_ASSERT_TRUE(out < 0.0f);
    TEST_ASSERT_TRUE(std::abs(out) < 100.0f);  // no es 1000+ por interpretar mal
}

void test_heading_pid_first_tick_no_derivative_kick(void) {
    HeadingPID pid;
    pid.kd = 100.0f;  // ganancia derivativa muy alta
    pid.setpoint_deg = 90.0f;
    // Primer tick: prev_error = 0 default, error = 90. Sin la guarda primed,
    // derivada = (90-0)/dt = enorme. Con primed=false en init, no se usa.
    float out = heading_pid_tick(pid, 0.0f, 0);
    // El output debe estar dominado por kp*error = 3*90 = 270, no por derivativa.
    TEST_ASSERT_TRUE(out < 360.0f);  // si entra derivativa, salta a miles
    TEST_ASSERT_TRUE(out > 200.0f);
}

// ============================================================================
// Anti-windup real / conditional integration (fix #29)
// ============================================================================

void test_heading_pid_no_windup_when_output_saturated(void) {
    // Error grande y sostenido que SATURA el output (kp*180 = 540 > clamp 360).
    // Con conditional integration, el integral NO debe crecer hacia integral_clamp
    // mientras el error empuje en la misma dirección de la saturación.
    // (Antes: el integral llegaba a +50 = integral_clamp -> overshoot al volver.)
    HeadingPID pid;
    pid.setpoint_deg = 180.0f;
    uint32_t t = 0;
    for (int i = 0; i < 200; ++i) {
        heading_pid_tick(pid, 0.0f, t);  // error = +180 (wrapped), satura
        t += 10;
    }
    // El integral queda acotado cerca de 0 (no se cargó), muy por debajo de 50.
    TEST_ASSERT_TRUE(std::abs(pid.integral) < 5.0f);
}

void test_heading_pid_integral_accumulates_when_not_saturated(void) {
    // REGRESION: error chico no-saturante (kp*10 = 30 << clamp 360).
    // El integral DEBE seguir acumulando como antes (comportamiento idéntico).
    HeadingPID pid;
    pid.setpoint_deg = 10.0f;
    uint32_t t = 0;
    for (int i = 0; i < 50; ++i) {
        heading_pid_tick(pid, 0.0f, t);  // error = +10, output ~30..., no satura
        t += 10;
    }
    // error*dt = 10*0.01 = 0.1 por tick * 50 = 5.0 (sin clamp de integral todavía).
    TEST_ASSERT_TRUE(pid.integral > 4.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 5.0f, pid.integral);
}

void test_lateral_pid_no_windup_when_output_saturated(void) {
    // Error grande sostenido que satura el output lateral.
    LateralPID pid;
    pid.setpoint = 1.0f;
    pid.kp = 1000.0f;          // ganancia alta -> satura facil
    pid.output_clamp = 800.0f;
    uint32_t t = 0;
    for (int i = 0; i < 200; ++i) {
        lateral_pid_tick(pid, 100.0f, t);  // error = -99, kp*error = -99000, satura
        t += 10;
    }
    // El integral NO se cargó hasta integral_clamp (20). Queda acotado cerca de 0.
    TEST_ASSERT_TRUE(std::abs(pid.integral) < 2.0f);
}

void test_lateral_pid_integral_accumulates_when_not_saturated(void) {
    // REGRESION: error chico no-saturante. El integral acumula como antes.
    LateralPID pid;
    pid.setpoint = 1.0f;
    pid.kp = 1.0f;             // ganancia baja -> no satura
    pid.ki = 5.0f;
    pid.output_clamp = 800.0f;
    uint32_t t = 0;
    for (int i = 0; i < 10; ++i) {
        lateral_pid_tick(pid, 2.0f, t);  // error = -1, output pequeño, no satura
        t += 10;
    }
    // error*dt = -1*0.01 = -0.01 por tick * 10 = -0.1 (sin clamp todavía).
    TEST_ASSERT_TRUE(pid.integral < -0.05f);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, -0.1f, pid.integral);
}

// ============================================================================
// omega_degps_to_centideg (fix #9 — saturación de omega, evitar overflow int16)
// ============================================================================

void test_omega_centideg_max_saturates(void) {
    // 360 deg/s -> 36000 centideg/s, fuera del rango int16 (+-32767).
    // DEBE saturar a +32767, NUNCA envolver a ~-29536 (sign-flip = gira al reves).
    int16_t out = omega_degps_to_centideg(360.0f);
    TEST_ASSERT_EQUAL_INT16(32767, out);
}

void test_omega_centideg_min_saturates(void) {
    int16_t out = omega_degps_to_centideg(-360.0f);
    TEST_ASSERT_EQUAL_INT16(-32767, out);
}

void test_omega_centideg_normal_value_rounds_and_keeps_sign(void) {
    // Valor normal dentro de rango: 12.34 deg/s -> 1234 centideg/s.
    TEST_ASSERT_EQUAL_INT16(1234, omega_degps_to_centideg(12.34f));
    // Conserva signo en valor negativo normal.
    TEST_ASSERT_EQUAL_INT16(-1234, omega_degps_to_centideg(-12.34f));
}

void test_omega_centideg_zero(void) {
    TEST_ASSERT_EQUAL_INT16(0, omega_degps_to_centideg(0.0f));
}

void test_omega_centideg_no_sign_flip_above_327deg(void) {
    // REGRESION del bug exacto: con > 327.67 deg/s, (int16_t)(omega*100) envolvia
    // y cambiaba de signo (positivo -> negativo). El helper NUNCA cambia de signo.
    int16_t out = omega_degps_to_centideg(350.0f);  // 35000 > 32767
    TEST_ASSERT_TRUE(out > 0);          // sigue siendo positivo (NO sign-flip)
    TEST_ASSERT_EQUAL_INT16(32767, out);  // saturado al tope
}

void test_omega_centideg_nan_returns_zero(void) {
    // SC-01 (eval 2026-06-04): un heading corrupto del BNO puede producir NaN.
    // Sin la guarda, (int16_t)NaN es UB y mete basura al motor. ω = NaN ⇒ 0.
    const float nan_val = std::nan("");
    TEST_ASSERT_EQUAL_INT16(0, omega_degps_to_centideg(nan_val));
}

void test_omega_centideg_inf_saturates(void) {
    // ±Inf no es el caso que la guarda NaN cubre, pero los clamps lo saturan.
    TEST_ASSERT_EQUAL_INT16(32767, omega_degps_to_centideg(INFINITY));
    TEST_ASSERT_EQUAL_INT16(-32767, omega_degps_to_centideg(-INFINITY));
}

// ============================================================================
// clamp_velocity_mm_s (Track 3 — clamp defensivo float->int16 de velocidades)
// ============================================================================

void test_clamp_velocity_normal_value_byte_identical(void) {
    // En el rango normal es BYTE-IDÉNTICO a (int16_t)v: trunca hacia cero igual
    // que el cast directo. 123.9 -> 123 (no 124).
    TEST_ASSERT_EQUAL_INT16(123, clamp_velocity_mm_s(123.9f));
    TEST_ASSERT_EQUAL_INT16(0, clamp_velocity_mm_s(0.0f));
    TEST_ASSERT_EQUAL_INT16(800, clamp_velocity_mm_s(800.0f));
    TEST_ASSERT_EQUAL_INT16(32767, clamp_velocity_mm_s(32767.0f));   // tope exacto
}

void test_clamp_velocity_huge_positive_saturates(void) {
    // Valor enorme (> int16 max) DEBE saturar a +32767, NUNCA envolver a negativo.
    TEST_ASSERT_EQUAL_INT16(32767, clamp_velocity_mm_s(100000.0f));
    TEST_ASSERT_EQUAL_INT16(32767, clamp_velocity_mm_s(32768.0f));   // primer fuera-de-rango
}

void test_clamp_velocity_huge_negative_saturates(void) {
    TEST_ASSERT_EQUAL_INT16(-32767, clamp_velocity_mm_s(-100000.0f));
    TEST_ASSERT_EQUAL_INT16(-32767, clamp_velocity_mm_s(-40000.0f));
}

void test_clamp_velocity_nan_returns_zero(void) {
    // NaN (heading/PID corrupto) -> 0. Sin la guarda, (int16_t)NaN es UB.
    const float nan_val = std::nan("");
    TEST_ASSERT_EQUAL_INT16(0, clamp_velocity_mm_s(nan_val));
}

void test_clamp_velocity_sign_preserved(void) {
    // El signo se conserva en valores normales (no hay flip).
    TEST_ASSERT_TRUE(clamp_velocity_mm_s(-500.0f) < 0);
    TEST_ASSERT_TRUE(clamp_velocity_mm_s(500.0f) > 0);
    TEST_ASSERT_EQUAL_INT16(-500, clamp_velocity_mm_s(-500.0f));
    // Y se conserva al saturar (positivo grande -> positivo tope, no negativo).
    TEST_ASSERT_TRUE(clamp_velocity_mm_s(99999.0f) > 0);
    TEST_ASSERT_TRUE(clamp_velocity_mm_s(-99999.0f) < 0);
}

void test_clamp_velocity_inf_saturates(void) {
    TEST_ASSERT_EQUAL_INT16(32767, clamp_velocity_mm_s(INFINITY));
    TEST_ASSERT_EQUAL_INT16(-32767, clamp_velocity_mm_s(-INFINITY));
}

// ============================================================================
// LateralPID
// ============================================================================

void test_lateral_pid_zero_error_zero_output(void) {
    LateralPID pid;
    pid.setpoint = 1.0f;
    float out = lateral_pid_tick(pid, 1.0f, 0);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, out);
}

void test_lateral_pid_negative_error_negative_output(void) {
    LateralPID pid;
    pid.setpoint = 1.0f;
    // measurement = 3 → error = setpoint - measurement = -2 → output negativo.
    float out = lateral_pid_tick(pid, 3.0f, 0);
    TEST_ASSERT_TRUE(out < 0.0f);
}

void test_lateral_pid_output_clamped(void) {
    LateralPID pid;
    pid.setpoint = 1.0f;
    pid.output_clamp = 500.0f;
    pid.kp = 1000.0f;  // ganancia muy alta para forzar saturación
    float out = lateral_pid_tick(pid, 100.0f, 0);
    TEST_ASSERT_TRUE(out >= -501.0f);
    TEST_ASSERT_TRUE(out <= +501.0f);
}

// ============================================================================
// Approach velocity profile
// ============================================================================

void test_approach_velocity_zero_at_close(void) {
    float v = approach_velocity(30.0f, 50.0f, 500.0f, 600.0f, 200.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, v);
}

void test_approach_velocity_max_at_far(void) {
    float v = approach_velocity(800.0f, 50.0f, 500.0f, 600.0f, 200.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 600.0f, v);
}

void test_approach_velocity_min_at_close_threshold(void) {
    float v = approach_velocity(50.0f, 50.0f, 500.0f, 600.0f, 200.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 200.0f, v);
}

void test_approach_velocity_mid_interpolation(void) {
    // distance = 275 = mitad entre close=50 y far=500 → velocidad mitad entre min y max.
    float v = approach_velocity(275.0f, 50.0f, 500.0f, 600.0f, 200.0f);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 400.0f, v);  // (200+600)/2 = 400
}

// ============================================================================
// INVARIANTE CRÍTICO: HeadingPID.output_clamp DEFAULT debe ser <= 327 deg/s.
// Por qué: cmd.omega_centideg_s = omega_degps * 100 es int16 (±32767). Con un
// clamp de 360 deg/s daba 36000 → OVERFLOW → el giro se INVERTÍA en cancha
// (eval 2026-06-03). Este test es el guard de regresión para que nadie vuelva
// a poner 360. (Audit 2026-06-05.)
// ============================================================================
void test_heading_pid_default_clamp_fits_int16(void) {
    HeadingPID pid;  // construcción por defecto
    // (1) el default NO debe superar 327 deg/s
    TEST_ASSERT_TRUE(pid.output_clamp <= 327.0f);
    // (2) y por las dudas, que omega*100 quepa en int16 con margen
    TEST_ASSERT_TRUE(pid.output_clamp * 100.0f <= 32767.0f);
    TEST_ASSERT_TRUE(pid.output_clamp > 0.0f);  // sanity: no quedó en 0
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_heading_pid_default_clamp_fits_int16);

    // wrap_diff_deg
    RUN_TEST(test_wrap_diff_zero_diff);
    RUN_TEST(test_wrap_diff_small_positive);
    RUN_TEST(test_wrap_diff_small_negative);
    RUN_TEST(test_wrap_diff_wraps_across_180);
    RUN_TEST(test_wrap_diff_exact_180);

    // HeadingPID
    RUN_TEST(test_heading_pid_zero_error_zero_output);
    RUN_TEST(test_heading_pid_positive_error_positive_output);
    RUN_TEST(test_heading_pid_output_clamped);
    RUN_TEST(test_heading_pid_reset_zeros_state);
    RUN_TEST(test_heading_pid_handles_wrap_around);
    RUN_TEST(test_heading_pid_first_tick_no_derivative_kick);

    // Anti-windup real / conditional integration (fix #29)
    RUN_TEST(test_heading_pid_no_windup_when_output_saturated);
    RUN_TEST(test_heading_pid_integral_accumulates_when_not_saturated);
    RUN_TEST(test_lateral_pid_no_windup_when_output_saturated);
    RUN_TEST(test_lateral_pid_integral_accumulates_when_not_saturated);

    // omega_degps_to_centideg (fix #9)
    RUN_TEST(test_omega_centideg_max_saturates);
    RUN_TEST(test_omega_centideg_min_saturates);
    RUN_TEST(test_omega_centideg_normal_value_rounds_and_keeps_sign);
    RUN_TEST(test_omega_centideg_zero);
    RUN_TEST(test_omega_centideg_no_sign_flip_above_327deg);
    RUN_TEST(test_omega_centideg_nan_returns_zero);
    RUN_TEST(test_omega_centideg_inf_saturates);

    // clamp_velocity_mm_s (Track 3)
    RUN_TEST(test_clamp_velocity_normal_value_byte_identical);
    RUN_TEST(test_clamp_velocity_huge_positive_saturates);
    RUN_TEST(test_clamp_velocity_huge_negative_saturates);
    RUN_TEST(test_clamp_velocity_nan_returns_zero);
    RUN_TEST(test_clamp_velocity_sign_preserved);
    RUN_TEST(test_clamp_velocity_inf_saturates);

    // LateralPID
    RUN_TEST(test_lateral_pid_zero_error_zero_output);
    RUN_TEST(test_lateral_pid_negative_error_negative_output);
    RUN_TEST(test_lateral_pid_output_clamped);

    // Approach
    RUN_TEST(test_approach_velocity_zero_at_close);
    RUN_TEST(test_approach_velocity_max_at_far);
    RUN_TEST(test_approach_velocity_min_at_close_threshold);
    RUN_TEST(test_approach_velocity_mid_interpolation);

    return UNITY_END();
}
