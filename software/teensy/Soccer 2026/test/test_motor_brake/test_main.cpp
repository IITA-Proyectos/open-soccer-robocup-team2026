// test_motor_brake — plugging (reversa) + freno anticipado de la trasera.
// Corre host con: bash scripts/run-host-tests.sh test_motor_brake
//
// Cubre el módulo PURO motor_brake.h (Capa 2b). Verifica:
//   • plugging: reversa DENTRO de la ventana, coast FUERA; signo opuesto al
//     último comando; cap 150 sobre las crudas 275/187 (asimetría perdida);
//     gate OFF (cap<=0) = passthrough crudo; last==0 → coast.
//   • lead-time trasera: 0 si NO es paralela; escala lineal x VEL/100; clamp
//     0..300; bordes (vel 0, vel negativa).

#include <unity.h>
#include "motor_brake.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ── PLUGGING ────────────────────────────────────────────────────────────────

// Dentro de la ventana, con movimiento adelante (+), la reversa es NEGATIVA y
// capeada a 150 aunque la cruda sea 275.
void test_plug_reversa_dentro_ventana_capeada(void) {
    PlugBrakeIn in{ /*last*/ 200, /*raw*/ PLUG_PWM_M1_RAW, /*ms*/ 50 };
    TEST_ASSERT_EQUAL_INT(-150, plug_brake_compute(in, PLUG_WINDOW_MS, 150));
}

// Movimiento hacia atrás (last<0) → reversa POSITIVA (signo opuesto), capeada.
void test_plug_signo_opuesto(void) {
    PlugBrakeIn in{ -200, PLUG_PWM_M1_RAW, 50 };
    TEST_ASSERT_EQUAL_INT(150, plug_brake_compute(in, PLUG_WINDOW_MS, 150));
}

// Fuera de la ventana (ms >= window) → coast (0).
void test_plug_fuera_ventana_es_coast(void) {
    PlugBrakeIn in{ 200, PLUG_PWM_M1_RAW, PLUG_WINDOW_MS };       // justo al borde
    TEST_ASSERT_EQUAL_INT(0, plug_brake_compute(in, PLUG_WINDOW_MS, 150));
    PlugBrakeIn in2{ 200, PLUG_PWM_M1_RAW, PLUG_WINDOW_MS + 100 };
    TEST_ASSERT_EQUAL_INT(0, plug_brake_compute(in2, PLUG_WINDOW_MS, 150));
}

// Sin movimiento previo (last==0) → coast, no se inventa reversa.
void test_plug_sin_movimiento_previo_coast(void) {
    PlugBrakeIn in{ 0, PLUG_PWM_M1_RAW, 50 };
    TEST_ASSERT_EQUAL_INT(0, plug_brake_compute(in, PLUG_WINDOW_MS, 150));
}

// CAP: con cap 150, las DOS ruedas (cruda 275 y 187) salen a 150 → la asimetría
// 275/187 se PIERDE (las dos magnitudes iguales). Esto es lo que avisamos.
void test_plug_cap_pierde_asimetria(void) {
    PlugBrakeIn m1{ 200, PLUG_PWM_M1_RAW, 50 };   // cruda 275
    PlugBrakeIn m2{ 200, PLUG_PWM_M2_RAW, 50 };   // cruda 187
    int r1 = plug_brake_compute(m1, PLUG_WINDOW_MS, 150);
    int r2 = plug_brake_compute(m2, PLUG_WINDOW_MS, 150);
    TEST_ASSERT_EQUAL_INT(-150, r1);
    TEST_ASSERT_EQUAL_INT(-150, r2);
    TEST_ASSERT_EQUAL_INT(r1, r2);                // asimetría perdida (intencional)
}

// GATE OFF: cap<=0 → passthrough de la cruda (sin capear) — para test/tuneo suelto.
void test_plug_cap_off_passthrough(void) {
    PlugBrakeIn m1{ 200, PLUG_PWM_M1_RAW, 50 };
    PlugBrakeIn m2{ 200, PLUG_PWM_M2_RAW, 50 };
    TEST_ASSERT_EQUAL_INT(-275, plug_brake_compute(m1, PLUG_WINDOW_MS, 0));
    TEST_ASSERT_EQUAL_INT(-187, plug_brake_compute(m2, PLUG_WINDOW_MS, 0));  // asimetría visible
}

// window_ms<=0 deshabilita el plug (coast siempre).
void test_plug_window_cero_deshabilita(void) {
    PlugBrakeIn in{ 200, PLUG_PWM_M1_RAW, 0 };
    TEST_ASSERT_EQUAL_INT(0, plug_brake_compute(in, 0, 150));
}

// ── FRENO ANTICIPADO TRASERA ────────────────────────────────────────────────

// No paralela (delanteras) → nunca se anticipa.
void test_lead_no_paralela_es_cero(void) {
    TEST_ASSERT_EQUAL_INT(0, rear_brake_lead_ms(false, 100, BRAKE_LEAD_BASE_MS, BRAKE_LEAD_MAX_MS));
}

// Paralela a VEL=100 → lead = base (66 ms).
void test_lead_paralela_vel_nominal(void) {
    TEST_ASSERT_EQUAL_INT(66, rear_brake_lead_ms(true, 100, BRAKE_LEAD_BASE_MS, BRAKE_LEAD_MAX_MS));
}

// Escala lineal x VEL/100: a VEL=50 → la mitad (33); a VEL=150 → 99.
void test_lead_escala_lineal(void) {
    TEST_ASSERT_EQUAL_INT(33, rear_brake_lead_ms(true, 50,  BRAKE_LEAD_BASE_MS, BRAKE_LEAD_MAX_MS));
    TEST_ASSERT_EQUAL_INT(99, rear_brake_lead_ms(true, 150, BRAKE_LEAD_BASE_MS, BRAKE_LEAD_MAX_MS));
}

// Clamp superior a max_ms (300): una VEL absurda no dispara un lead enorme.
void test_lead_clamp_superior(void) {
    TEST_ASSERT_EQUAL_INT(300, rear_brake_lead_ms(true, 1000, BRAKE_LEAD_BASE_MS, BRAKE_LEAD_MAX_MS));
}

// Bordes: vel 0 → 0; vel negativa usa magnitud (signo de la velocidad no importa).
void test_lead_bordes_velocidad(void) {
    TEST_ASSERT_EQUAL_INT(0,  rear_brake_lead_ms(true, 0,    BRAKE_LEAD_BASE_MS, BRAKE_LEAD_MAX_MS));
    TEST_ASSERT_EQUAL_INT(66, rear_brake_lead_ms(true, -100, BRAKE_LEAD_BASE_MS, BRAKE_LEAD_MAX_MS));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_plug_reversa_dentro_ventana_capeada);
    RUN_TEST(test_plug_signo_opuesto);
    RUN_TEST(test_plug_fuera_ventana_es_coast);
    RUN_TEST(test_plug_sin_movimiento_previo_coast);
    RUN_TEST(test_plug_cap_pierde_asimetria);
    RUN_TEST(test_plug_cap_off_passthrough);
    RUN_TEST(test_plug_window_cero_deshabilita);
    RUN_TEST(test_lead_no_paralela_es_cero);
    RUN_TEST(test_lead_paralela_vel_nominal);
    RUN_TEST(test_lead_escala_lineal);
    RUN_TEST(test_lead_clamp_superior);
    RUN_TEST(test_lead_bordes_velocidad);
    return UNITY_END();
}
