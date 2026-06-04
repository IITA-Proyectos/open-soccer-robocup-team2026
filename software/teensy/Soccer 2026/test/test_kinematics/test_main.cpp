// test_kinematics — tests unitarios de cinemática inversa omni-3
// Corre en host con: pio test -e test_native -f test_kinematics

#include <unity.h>
#include <cmath>
#include "kinematics.h"

using namespace iitasoccer;

namespace {
constexpr float PI_F = 3.14159265358979323846f;

// Configuración estándar IITA tentativa: ruedas a 60°, -60°, 180°. Radio 100mm.
// ⚠️ Estos ángulos son TENTATIVOS — confirmar contra el montaje físico real.
const WheelConfig STANDARD_WHEELS[3] = {
    {  60.0f * PI_F / 180.0f, 100.0f },  // W1: derecha-frente
    { -60.0f * PI_F / 180.0f, 100.0f },  // W2: izquierda-frente (300° CCW)
    { 180.0f * PI_F / 180.0f, 100.0f },  // W3: atrás centro
};
}

void setUp(void) {}
void tearDown(void) {}

void test_stop_command_all_wheels_zero(void) {
    auto ws = inverse_kinematics(0.0f, 0.0f, 0.0f, STANDARD_WHEELS);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ws.wheel[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ws.wheel[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ws.wheel[2]);
}

void test_avanzar_puro(void) {
    // vy = 100mm/s (frente), vx = 0, omega = 0
    auto ws = inverse_kinematics(0.0f, 100.0f, 0.0f, STANDARD_WHEELS);
    // W1 @ 60°:  cos(60)*100 = 50
    // W2 @ -60°: cos(-60)*100 = 50
    // W3 @ 180°: cos(180)*100 = -100
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, ws.wheel[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, ws.wheel[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -100.0f, ws.wheel[2]);
}

void test_lateral_puro_derecha(void) {
    // vx = 100mm/s (derecha), vy = 0, omega = 0
    auto ws = inverse_kinematics(100.0f, 0.0f, 0.0f, STANDARD_WHEELS);
    // W1 @ 60°:  -sin(60)*100 = -86.6
    // W2 @ -60°: -sin(-60)*100 = 86.6
    // W3 @ 180°: -sin(180)*100 = 0
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -86.6f, ws.wheel[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f,  86.6f, ws.wheel[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, ws.wheel[2]);
}

void test_rotacion_pura_ccw(void) {
    // omega = 1 rad/s, vx = vy = 0 → todas las ruedas giran a velocidad omega * R
    auto ws = inverse_kinematics(0.0f, 0.0f, 1.0f, STANDARD_WHEELS);
    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, ws.wheel[i]);
    }
}

void test_rotacion_pura_cw(void) {
    // omega = -1 rad/s → todas las ruedas a -100
    auto ws = inverse_kinematics(0.0f, 0.0f, -1.0f, STANDARD_WHEELS);
    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, -100.0f, ws.wheel[i]);
    }
}

void test_diagonal_45deg(void) {
    // Avanzar y lateral simultáneo (diagonal 45° frente-derecha)
    // v_x = v_y = 70.71 mm/s (magnitud 100, ángulo 45°)
    auto ws = inverse_kinematics(70.71f, 70.71f, 0.0f, STANDARD_WHEELS);
    // W1 @ 60°: -70.71*sin(60) + 70.71*cos(60) = -61.23 + 35.36 = -25.88
    // W2 @ -60°: -70.71*sin(-60) + 70.71*cos(-60) = 61.23 + 35.36 = 96.59
    // W3 @ 180°: 0 - 70.71 = -70.71
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -25.88f, ws.wheel[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f,  96.59f, ws.wheel[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -70.71f, ws.wheel[2]);
}

void test_pwm_conversion_basic(void) {
    TEST_ASSERT_EQUAL_INT(255, wheel_speed_to_pwm(1000.0f, 1000.0f, 255));
    TEST_ASSERT_EQUAL_INT(-255, wheel_speed_to_pwm(-1000.0f, 1000.0f, 255));
    TEST_ASSERT_EQUAL_INT(0, wheel_speed_to_pwm(0.0f, 1000.0f, 255));
}

void test_pwm_conversion_saturates(void) {
    // Si la velocidad supera el max, satura al PWM máximo
    TEST_ASSERT_EQUAL_INT(255, wheel_speed_to_pwm(2000.0f, 1000.0f, 255));
    TEST_ASSERT_EQUAL_INT(-255, wheel_speed_to_pwm(-2000.0f, 1000.0f, 255));
}

void test_pwm_conversion_linear(void) {
    // 500 / 1000 = 0.5 → PWM 127 (truncado de 127.5)
    TEST_ASSERT_EQUAL_INT(127, wheel_speed_to_pwm(500.0f, 1000.0f, 255));
}

void test_saturate_wheels_preserves_direction(void) {
    // Si una rueda excede max, todas se escalan proporcionalmente
    WheelSpeeds ws{};
    ws.wheel[0] = 1000.0f;
    ws.wheel[1] = 500.0f;
    ws.wheel[2] = 250.0f;
    saturate_wheels(ws, 500.0f);
    // Max abs era 1000, scale = 500/1000 = 0.5
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 500.0f, ws.wheel[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 250.0f, ws.wheel[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 125.0f, ws.wheel[2]);
}

void test_saturate_wheels_no_op_when_within_limit(void) {
    WheelSpeeds ws{};
    ws.wheel[0] = 100.0f;
    ws.wheel[1] = -200.0f;
    ws.wheel[2] = 150.0f;
    saturate_wheels(ws, 500.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, ws.wheel[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -200.0f, ws.wheel[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 150.0f, ws.wheel[2]);
}

// ── apply_pwm_floor (deadzone compensation / piso de PWM) ───────────────────

void test_pwm_floor_gate_off_is_noop(void) {
    // min_pwm <= 0 → GATE OFF: cualquier pwm pasa SIN cambios (binario competencia).
    TEST_ASSERT_EQUAL_INT(0,    apply_pwm_floor(0,    0, 0));
    TEST_ASSERT_EQUAL_INT(5,    apply_pwm_floor(5,    0, 0));
    TEST_ASSERT_EQUAL_INT(-5,   apply_pwm_floor(-5,   0, 0));
    TEST_ASSERT_EQUAL_INT(200,  apply_pwm_floor(200,  0, 0));
    TEST_ASSERT_EQUAL_INT(-255, apply_pwm_floor(-255, 0, 0));
    // min_pwm negativo también es OFF (defensivo): no-op aunque noise_thresh sea >0.
    TEST_ASSERT_EQUAL_INT(3,    apply_pwm_floor(3, -10, 50));
}

void test_pwm_floor_raises_small_to_min_positive(void) {
    // 0 < pwm < min_pwm → se eleva al piso, signo positivo conservado.
    TEST_ASSERT_EQUAL_INT(40, apply_pwm_floor(10, 40, 0));
    TEST_ASSERT_EQUAL_INT(40, apply_pwm_floor(39, 40, 0));
    TEST_ASSERT_EQUAL_INT(40, apply_pwm_floor(1,  40, 0));
}

void test_pwm_floor_raises_small_to_min_negative(void) {
    // Mismo caso, signo negativo conservado.
    TEST_ASSERT_EQUAL_INT(-40, apply_pwm_floor(-10, 40, 0));
    TEST_ASSERT_EQUAL_INT(-40, apply_pwm_floor(-39, 40, 0));
    TEST_ASSERT_EQUAL_INT(-40, apply_pwm_floor(-1,  40, 0));
}

void test_pwm_floor_noise_goes_to_zero(void) {
    // |pwm| <= noise_thresh → 0 (ruido/comando despreciable, no zumba parado).
    TEST_ASSERT_EQUAL_INT(0, apply_pwm_floor(0,   40, 5));
    TEST_ASSERT_EQUAL_INT(0, apply_pwm_floor(5,   40, 5));
    TEST_ASSERT_EQUAL_INT(0, apply_pwm_floor(-5,  40, 5));
    TEST_ASSERT_EQUAL_INT(0, apply_pwm_floor(3,   40, 5));
    // Justo por encima del ruido (6 > 5) pero por debajo del piso → sube al piso.
    TEST_ASSERT_EQUAL_INT(40,  apply_pwm_floor(6,  40, 5));
    TEST_ASSERT_EQUAL_INT(-40, apply_pwm_floor(-6, 40, 5));
}

void test_pwm_floor_large_pwm_untouched(void) {
    // |pwm| >= min_pwm → intacto (signo incluido).
    TEST_ASSERT_EQUAL_INT(40,   apply_pwm_floor(40,   40, 5));  // justo en el piso
    TEST_ASSERT_EQUAL_INT(100,  apply_pwm_floor(100,  40, 5));
    TEST_ASSERT_EQUAL_INT(-100, apply_pwm_floor(-100, 40, 5));
    TEST_ASSERT_EQUAL_INT(255,  apply_pwm_floor(255,  40, 5));
    TEST_ASSERT_EQUAL_INT(-255, apply_pwm_floor(-255, 40, 5));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_stop_command_all_wheels_zero);
    RUN_TEST(test_avanzar_puro);
    RUN_TEST(test_lateral_puro_derecha);
    RUN_TEST(test_rotacion_pura_ccw);
    RUN_TEST(test_rotacion_pura_cw);
    RUN_TEST(test_diagonal_45deg);
    RUN_TEST(test_pwm_conversion_basic);
    RUN_TEST(test_pwm_conversion_saturates);
    RUN_TEST(test_pwm_conversion_linear);
    RUN_TEST(test_saturate_wheels_preserves_direction);
    RUN_TEST(test_saturate_wheels_no_op_when_within_limit);
    RUN_TEST(test_pwm_floor_gate_off_is_noop);
    RUN_TEST(test_pwm_floor_raises_small_to_min_positive);
    RUN_TEST(test_pwm_floor_raises_small_to_min_negative);
    RUN_TEST(test_pwm_floor_noise_goes_to_zero);
    RUN_TEST(test_pwm_floor_large_pwm_untouched);
    return UNITY_END();
}
