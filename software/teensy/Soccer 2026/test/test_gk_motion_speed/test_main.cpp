// test_gk_motion_speed — ancla la equivalencia "PWM 2025 x1.1 ↔ velocidad de comando 2026".
// Corre en host: bash scripts/run-host-tests.sh test_gk_motion_speed
#include <unity.h>
#include <cmath>
#include "gk_motion_speed.h"
#include "kinematics.h"

using namespace iitasoccer;

namespace {
constexpr float PI_F = 3.14159265358979323846f;
constexpr float MAXS = 1000.0f;
constexpr int   MAXPWM = 255;
// Cinemática real del arquero 2026 (config_central.h ROBOT1).
const WheelConfig GK_WHEELS[3] = {
    { 330.0f * PI_F / 180.0f, 100.0f },  // M1 del-izq
    { 210.0f * PI_F / 180.0f, 100.0f },  // M2 del-der
    {  90.0f * PI_F / 180.0f, 100.0f },  // M3 trasera
};
}

void setUp(void) {}
void tearDown(void) {}

// La cuenta inversa: querer delanteras=55 PWM en strafe → ~430 mm/s.
void test_strafe_speed_for_front55_is_about_430(void) {
    float vx = gk_strafe_speed_from_front_pwm(55, MAXS, MAXPWM);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 431.0f, vx);
}

// A esa velocidad, la CINEMÁTICA REAL reparte {55,55,110} PWM (trasera = 2× delanteras = 2025).
void test_strafe_at_target_speed_reproduces_2025_ratio(void) {
    float vx = gk_strafe_speed_from_front_pwm(55, MAXS, MAXPWM);  // ~431, no clampea (rear 110 < cap)
    WheelSpeeds ws = inverse_kinematics(vx, 0.0f, 0.0f, GK_WHEELS);
    int p0 = wheel_speed_to_pwm(ws.wheel[0], MAXS, MAXPWM);
    int p1 = wheel_speed_to_pwm(ws.wheel[1], MAXS, MAXPWM);
    int p2 = wheel_speed_to_pwm(ws.wheel[2], MAXS, MAXPWM);
    TEST_ASSERT_INT_WITHIN(2, 55,  p0);                  // delantera izq
    TEST_ASSERT_INT_WITHIN(2, 55,  p1);                  // delantera der
    TEST_ASSERT_INT_WITHIN(3, 110, std::abs(p2));        // trasera FUERTE (~2× delanteras)
}

// El clamp del cap: el x1.5 (intercept) NO debe dejar la trasera por encima de 150.
void test_intercept_speed_clamped_to_cap(void) {
    float patrol = gk_strafe_speed_from_front_pwm(55, MAXS, MAXPWM);     // ~431
    float intercept = gk_clamp_strafe_speed_to_cap(patrol * 1.5f, MAXS, MAXPWM);  // 646 -> clamp 588
    WheelSpeeds ws = inverse_kinematics(intercept, 0.0f, 0.0f, GK_WHEELS);
    int rear = std::abs(wheel_speed_to_pwm(ws.wheel[2], MAXS, MAXPWM));
    TEST_ASSERT_TRUE(rear <= GK_PWM_CAP);                 // nunca quema
    TEST_ASSERT_INT_WITHIN(2, 150, rear);                // queda pegado al cap
}

// Avance: la trasera queda APAGADA (factor cos90=0), igual que el arquero 2025 (100/100/0).
void test_forward_keeps_rear_off(void) {
    float vy = gk_forward_speed_from_front_pwm(110, MAXS, MAXPWM);  // avanzar 100 x1.1
    WheelSpeeds ws = inverse_kinematics(0.0f, vy, 0.0f, GK_WHEELS);
    int rear = wheel_speed_to_pwm(ws.wheel[2], MAXS, MAXPWM);
    TEST_ASSERT_INT_WITHIN(1, 0, rear);                  // M3 off en avance
    int front = std::abs(wheel_speed_to_pwm(ws.wheel[0], MAXS, MAXPWM));
    TEST_ASSERT_INT_WITHIN(3, 110, front);
}

// Strafe a izquierda (vx<0): el clamp conserva el signo.
void test_clamp_sign_preserving_left(void) {
    float vx = gk_clamp_strafe_speed_to_cap(-1000.0f, MAXS, MAXPWM);  // pediría rear 255 -> clamp
    TEST_ASSERT_TRUE(vx < 0.0f);
    int rear = std::abs(wheel_speed_to_pwm(
        inverse_kinematics(vx, 0.0f, 0.0f, GK_WHEELS).wheel[2], MAXS, MAXPWM));
    TEST_ASSERT_TRUE(rear <= GK_PWM_CAP);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_strafe_speed_for_front55_is_about_430);
    RUN_TEST(test_strafe_at_target_speed_reproduces_2025_ratio);
    RUN_TEST(test_intercept_speed_clamped_to_cap);
    RUN_TEST(test_forward_keeps_rear_off);
    RUN_TEST(test_clamp_sign_preserving_left);
    return UNITY_END();
}
