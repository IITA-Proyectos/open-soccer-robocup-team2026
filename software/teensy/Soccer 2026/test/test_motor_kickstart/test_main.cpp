// test_motor_kickstart — IMPULSO INICIAL anti-inercia (kickstart) — módulo PURO.
// Corre host con: bash scripts/run-host-tests.sh test_motor_kickstart
//
// Cubre motor_kickstart.h/.cpp: aplica un boost (factor ~1.8) al PWM de régimen
// durante una ventana corta (~40 ms) y luego deja pasar el base. Verifica:
//   • dentro de la ventana → base * factor (recortado al cap),
//   • fuera de la ventana (ms >= window) → base intacto,
//   • el cap de seguridad recorta el boost (anti-quemado),
//   • signo conservado en + y -,
//   • gates no-op: window<=0, factor<=10 (×1.0), pwm_base==0,
//   • bordes: ms=0, ms=window-1 (último tick con boost), ms=window (ya sin boost),
//     ms negativo, INT_MIN sin UB.

#include <unity.h>
#include <limits.h>
#include "motor_kickstart.h"

using namespace iitasoccer;

// Constantes 2025 portadas (espejo de config_central.h ROBOT1).
static const int W   = 40;    // KICKSTART_WINDOW_MS
static const int F18 = 18;    // MOTOR_KICKSTART_FACTOR_X10 (1.8)
static const int CAP = 153;   // KICKSTART_PWM_CAP

void setUp(void) {}
void tearDown(void) {}

// Dentro de la ventana: base 50 → 1.8*50 = 90 (el M1/M2 del arquero 2025).
void test_boost_inside_window_positive(void) {
    TEST_ASSERT_EQUAL_INT(90, motor_kickstart_pwm(50, 0,    W, F18, CAP));
    TEST_ASSERT_EQUAL_INT(90, motor_kickstart_pwm(50, 20,   W, F18, CAP));
    TEST_ASSERT_EQUAL_INT(90, motor_kickstart_pwm(50, W - 1, W, F18, CAP)); // último tick
}

// Signo conservado: base -50 → -90.
void test_boost_inside_window_negative(void) {
    TEST_ASSERT_EQUAL_INT(-90, motor_kickstart_pwm(-50, 0,     W, F18, CAP));
    TEST_ASSERT_EQUAL_INT(-90, motor_kickstart_pwm(-50, W - 1, W, F18, CAP));
}

// Fuera de la ventana (ms >= window): pasa el base SIN tocar.
void test_passthrough_after_window(void) {
    TEST_ASSERT_EQUAL_INT(50,  motor_kickstart_pwm(50,  W,      W, F18, CAP)); // justo al cerrar
    TEST_ASSERT_EQUAL_INT(50,  motor_kickstart_pwm(50,  W + 1,  W, F18, CAP));
    TEST_ASSERT_EQUAL_INT(50,  motor_kickstart_pwm(50,  10000,  W, F18, CAP));
    TEST_ASSERT_EQUAL_INT(-50, motor_kickstart_pwm(-50, W,      W, F18, CAP));
}

// Cap de seguridad: base 85 → 1.8*85 = 153 (= cap, no recorta). base 100 → 180 > 153 → 153.
void test_cap_clamps_boost(void) {
    TEST_ASSERT_EQUAL_INT(153,  motor_kickstart_pwm(85,  0, W, F18, CAP)); // M3 del arquero 2025
    TEST_ASSERT_EQUAL_INT(153,  motor_kickstart_pwm(100, 0, W, F18, CAP)); // 180 recortado a 153
    TEST_ASSERT_EQUAL_INT(-153, motor_kickstart_pwm(-100, 0, W, F18, CAP));
    // cap_abs <= 0 → usa el default del módulo (153).
    TEST_ASSERT_EQUAL_INT(153,  motor_kickstart_pwm(100, 0, W, F18, 0));
}

// GATE OFF: window<=0 → no-op (devuelve el base, binario idéntico).
void test_gate_off_window(void) {
    TEST_ASSERT_EQUAL_INT(50,  motor_kickstart_pwm(50, 0, 0,  F18, CAP));
    TEST_ASSERT_EQUAL_INT(50,  motor_kickstart_pwm(50, 0, -5, F18, CAP));
}

// GATE OFF: factor <= 10 (×1.0) → no-op aunque la ventana esté abierta.
void test_gate_off_factor(void) {
    TEST_ASSERT_EQUAL_INT(50, motor_kickstart_pwm(50, 0, W, 10, CAP)); // ×1.0
    TEST_ASSERT_EQUAL_INT(50, motor_kickstart_pwm(50, 0, W, 5,  CAP)); // <1.0 defensivo
}

// pwm_base == 0 → 0 (no se inventa arranque de una rueda parada).
void test_zero_base_stays_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, motor_kickstart_pwm(0, 0,  W, F18, CAP));
    TEST_ASSERT_EQUAL_INT(0, motor_kickstart_pwm(0, 99, W, F18, CAP));
}

// Bordes de tiempo raros: ms negativo → se trata como fuera (base intacto, seguro).
void test_negative_time_is_passthrough(void) {
    TEST_ASSERT_EQUAL_INT(50, motor_kickstart_pwm(50, -1, W, F18, CAP));
    TEST_ASSERT_EQUAL_INT(50, motor_kickstart_pwm(50, INT_MIN, W, F18, CAP));
}

// INT_MIN como pwm_base no debe romper (sin -INT_MIN). El cap lo lleva a -cap.
void test_int_min_base_no_ub(void) {
    TEST_ASSERT_EQUAL_INT(-153, motor_kickstart_pwm(INT_MIN, 0, W, F18, CAP));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_boost_inside_window_positive);
    RUN_TEST(test_boost_inside_window_negative);
    RUN_TEST(test_passthrough_after_window);
    RUN_TEST(test_cap_clamps_boost);
    RUN_TEST(test_gate_off_window);
    RUN_TEST(test_gate_off_factor);
    RUN_TEST(test_zero_base_stays_zero);
    RUN_TEST(test_negative_time_is_passthrough);
    RUN_TEST(test_int_min_base_no_ub);
    return UNITY_END();
}
