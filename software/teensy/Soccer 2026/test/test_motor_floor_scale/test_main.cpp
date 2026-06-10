// test_motor_floor_scale — piso por ESCALADO UNIFORME + eficiencia por rueda.
// Corre host: bash scripts/run-host-tests.sh test_motor_floor_scale
//
// Ancla las garantías del módulo (banco robot2 2026-06-09):
//   • strafe puro comandado bajo reproduce la mezcla VALIDADA {70,70,~107};
//   • una corrección chica de ω PASA (diferencial en delanteras) en vez de comerse;
//   • componente ínfima (≤ ruido) → 0, JAMÁS disparada al piso (anti bang-bang);
//   • retroceso (fronts ±, rear 0) queda intacto;
//   • tope térmico 150 por escalado proporcional;
//   • signos conservados.

#include <unity.h>
#include "motor_floor_scale.h"

using namespace iitasoccer;

namespace {
// Config del arquero en robot2 (config_central.h ROBOT2 + eficiencia del banco).
FloorScaleCfg cfg() {
    FloorScaleCfg c;
    c.floor_pwm[0] = 70;  c.floor_pwm[1] = 70;  c.floor_pwm[2] = 107;
    c.noise_thresh = 5;
    c.eff_x100[0] = 100;  c.eff_x100[1] = 100;  c.eff_x100[2] = 131;
    c.burn_cap = 150;
    return c;
}
}

void setUp(void) {}
void tearDown(void) {}

// Strafe puro vx=200: crudo {25,25,-51} → eficiencia {25,25,-38.9} → k=70/25=2.8
// → {70,70,-109} ≈ LA MEZCLA VALIDADA EN BANCO {70,70,107}. Consistencia exacta.
void test_strafe_reproduces_validated_mix(void) {
    int pwm[3] = { 25, 25, -51 };
    FloorScaleCfg c = cfg();
    motor_floor_scale(pwm, c);
    TEST_ASSERT_INT_WITHIN(2, 70, pwm[0]);
    TEST_ASSERT_INT_WITHIN(2, 70, pwm[1]);
    TEST_ASSERT_INT_WITHIN(4, -108, pwm[2]);   // ~107-109 (redondeos)
}

// Corrección de gyro chica durante el strafe: crudo {27,23,-51} (ω mete ±2 en las
// delanteras). HOY el clamp por-rueda la COMÍA (70/70). Acá PASA: tras escalar, las
// delanteras quedan DISTINTAS (diferencial ∝ corrección) → autoridad de rumbo real.
void test_small_omega_correction_passes(void) {
    int pwm[3] = { 27, 23, -51 };
    FloorScaleCfg c = cfg();
    motor_floor_scale(pwm, c);
    TEST_ASSERT_TRUE(pwm[0] > pwm[1]);                 // el diferencial sobrevive
    TEST_ASSERT_TRUE(pwm[1] >= 70 - 1);                // y ambas ≥ piso
    const float ratio = static_cast<float>(pwm[0]) / static_cast<float>(pwm[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.08f, 27.0f / 23.0f, ratio);  // proporción exacta
}

// Componente ínfima: retroceso + ω chico → la trasera recibe ~4 PWM (≤ ruido 5).
// HOY: el piso la disparaba a 107 (la patada). ACÁ: queda en 0 y las delanteras
// (ya sobre su piso) pasan SIN tocar (k=1).
void test_tiny_rear_component_zeroed_not_slammed(void) {
    int pwm[3] = { -93, 89, 4 };
    FloorScaleCfg c = cfg();
    motor_floor_scale(pwm, c);
    TEST_ASSERT_EQUAL_INT(0, pwm[2]);      // silencio, no patada a 107
    TEST_ASSERT_EQUAL_INT(-93, pwm[0]);    // k=1: intactas
    TEST_ASSERT_EQUAL_INT(89, pwm[1]);
}

// Retroceso puro {−93, +93, 0}: nada que escalar (k=1), rear inactivo → intacto.
void test_backward_unchanged(void) {
    int pwm[3] = { -93, 93, 0 };
    FloorScaleCfg c = cfg();
    motor_floor_scale(pwm, c);
    TEST_ASSERT_EQUAL_INT(-93, pwm[0]);
    TEST_ASSERT_EQUAL_INT(93, pwm[1]);
    TEST_ASSERT_EQUAL_INT(0, pwm[2]);
}

// Tope térmico: comando grande {120,120,-200} → eficiencia rear 200/1.31=152.7;
// k=1 pero la rear supera 150 → se escala TODO por 150/152.7 (proporcional).
void test_burn_cap_scales_proportionally(void) {
    int pwm[3] = { 120, 120, -200 };
    FloorScaleCfg c = cfg();
    motor_floor_scale(pwm, c);
    TEST_ASSERT_TRUE(pwm[2] >= -150 && pwm[2] <= -148);   // pegada al cap
    TEST_ASSERT_INT_WITHIN(2, 118, pwm[0]);               // bajadas en proporción
    TEST_ASSERT_EQUAL_INT(pwm[0], pwm[1]);
}

// Todo en cero / bajo ruido → todo cero (sin inventar movimiento).
void test_all_quiet_stays_quiet(void) {
    int pwm[3] = { 3, -4, 2 };
    FloorScaleCfg c = cfg();
    motor_floor_scale(pwm, c);
    TEST_ASSERT_EQUAL_INT(0, pwm[0]);
    TEST_ASSERT_EQUAL_INT(0, pwm[1]);
    TEST_ASSERT_EQUAL_INT(0, pwm[2]);
}

// ⚠️ REGRESIÓN del banco (gira "a lo loco"): AVANCE + corrección ω → la trasera lleva
// una componente AUXILIAR (~14 PWM, sobre el ruido pero lejos de su piso 107). Si esa
// auxiliar definiera k, exigiría k≈8 → cap térmico → latigazo a máxima potencia.
// Política correcta: las PRINCIPALES (delanteras) definen k; la trasera auxiliar → 0.
void test_aux_component_does_not_explode_scale(void) {
    int pwm[3] = { 84, -49, 18 };   // avance vy + ω: M1/M2 principales, M3 auxiliar
    FloorScaleCfg c = cfg();
    motor_floor_scale(pwm, c);
    TEST_ASSERT_EQUAL_INT(0, pwm[2]);                  // auxiliar bajo piso → 0
    TEST_ASSERT_TRUE(pwm[0] <= 130 && pwm[0] >= 70);   // sin latigazo al cap
    TEST_ASSERT_TRUE(pwm[1] <= -70 && pwm[1] >= -130);
    const float ratio = static_cast<float>(pwm[0]) / static_cast<float>(-pwm[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.12f, 84.0f / 49.0f, ratio);  // proporción M1/M2 exacta
}

// Signos conservados en strafe al otro lado.
void test_signs_preserved(void) {
    int pwm[3] = { -25, -25, 51 };
    FloorScaleCfg c = cfg();
    motor_floor_scale(pwm, c);
    TEST_ASSERT_TRUE(pwm[0] < 0 && pwm[1] < 0 && pwm[2] > 0);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_strafe_reproduces_validated_mix);
    RUN_TEST(test_small_omega_correction_passes);
    RUN_TEST(test_tiny_rear_component_zeroed_not_slammed);
    RUN_TEST(test_backward_unchanged);
    RUN_TEST(test_burn_cap_scales_proportionally);
    RUN_TEST(test_all_quiet_stays_quiet);
    RUN_TEST(test_aux_component_does_not_explode_scale);
    RUN_TEST(test_signs_preserved);
    return UNITY_END();
}
