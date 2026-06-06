// test_motor_power_cap — cap de potencia (duty) de motor (SEGURIDAD E1).
// Corre host con: bash scripts/run-host-tests.sh test_motor_power_cap
//
// Cubre el módulo PURO motor_power_cap.h, que limita el % de duty que llega al
// analogWrite para no quemar los motores DC 5V comunes alimentados a 7,4V
// (MEJORAS-PENDIENTES.md fila E1). Verifica:
//   • passthrough cuando el cap es OFF (<=0) o >= la escala completa (255),
//   • clamp SIMÉTRICO conservando el signo (+ y -),
//   • el caso emblemático 70% de 255 = 178,
//   • bordes: duty 0, duty justo en el cap, INT_MIN sin UB,
//   • motor_cap_from_pct: 70→178, 0→0, 100→escala, y clamp de pct fuera de rango.

#include <unity.h>
#include <limits.h>
#include "motor_power_cap.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// PASSTHROUGH: cap >= MAX (255) no recorta nada, ni siquiera un duty al máximo.
void test_passthrough_when_cap_ge_max(void) {
    TEST_ASSERT_EQUAL_INT(255, motor_power_cap(255, 255));   // cap == MAX
    TEST_ASSERT_EQUAL_INT(255, motor_power_cap(255, 300));   // cap > MAX
    TEST_ASSERT_EQUAL_INT(200, motor_power_cap(200, 255));   // duty<cap, intacto
    TEST_ASSERT_EQUAL_INT(-255, motor_power_cap(-255, 255)); // simétrico al máximo
    TEST_ASSERT_EQUAL_INT(-255, motor_power_cap(-255, 1000));
}

// PASSTHROUGH: cap OFF (<=0) deja el duty intacto (cap no configurado).
void test_passthrough_when_cap_off(void) {
    TEST_ASSERT_EQUAL_INT(255, motor_power_cap(255, 0));     // cap 0 = OFF
    TEST_ASSERT_EQUAL_INT(255, motor_power_cap(255, -5));    // cap negativo = OFF
    TEST_ASSERT_EQUAL_INT(-255, motor_power_cap(-255, 0));
    TEST_ASSERT_EQUAL_INT(123, motor_power_cap(123, 0));
}

// CLAMP SIMÉTRICO: |duty| por encima del cap → ±cap, conservando el signo.
void test_clamp_symmetric_sign_preserving(void) {
    TEST_ASSERT_EQUAL_INT(178, motor_power_cap(255, 178));   // + recorta a +cap
    TEST_ASSERT_EQUAL_INT(-178, motor_power_cap(-255, 178)); // - recorta a -cap
    TEST_ASSERT_EQUAL_INT(100, motor_power_cap(250, 100));
    TEST_ASSERT_EQUAL_INT(-100, motor_power_cap(-250, 100));
    // Magnitudes iguales, signos opuestos → resultados opuestos exactos (simetría).
    TEST_ASSERT_EQUAL_INT(-motor_power_cap(250, 100), motor_power_cap(-250, 100));
}

// El caso emblemático del fix de seguridad: 70% de 255 = 178, y capea ahí.
void test_seventy_percent_of_255_is_178(void) {
    int cap = motor_cap_from_pct(255, 70);
    TEST_ASSERT_EQUAL_INT(178, cap);                          // 255*70/100 = 178 (trunc)
    TEST_ASSERT_EQUAL_INT(178, motor_power_cap(255, cap));    // duty 100% se recorta a 178
    TEST_ASSERT_EQUAL_INT(-178, motor_power_cap(-255, cap));  // y simétrico
    TEST_ASSERT_EQUAL_INT(150, motor_power_cap(150, cap));    // por debajo del cap, intacto
}

// BORDE: duty 0 siempre 0; duty exactamente en el cap NO se recorta.
void test_edge_zero_and_at_cap(void) {
    TEST_ASSERT_EQUAL_INT(0, motor_power_cap(0, 178));   // 0 con cap activo
    TEST_ASSERT_EQUAL_INT(0, motor_power_cap(0, 0));     // 0 con cap OFF
    TEST_ASSERT_EQUAL_INT(0, motor_power_cap(0, 255));   // 0 con cap=MAX
    TEST_ASSERT_EQUAL_INT(178, motor_power_cap(178, 178));   // justo en el cap (+)
    TEST_ASSERT_EQUAL_INT(-178, motor_power_cap(-178, 178)); // justo en el cap (-)
    TEST_ASSERT_EQUAL_INT(177, motor_power_cap(177, 178));   // un escalón por debajo
    TEST_ASSERT_EQUAL_INT(178, motor_power_cap(179, 178));   // un escalón por encima → cap
}

// BORDE numérico: INT_MIN no causa UB (no calculamos -INT_MIN). Se recorta a -cap.
void test_edge_int_min_no_ub(void) {
    TEST_ASSERT_EQUAL_INT(-178, motor_power_cap(INT_MIN, 178));
    TEST_ASSERT_EQUAL_INT(INT_MAX, motor_power_cap(INT_MAX, 0));   // cap OFF: passthrough
    TEST_ASSERT_EQUAL_INT(178, motor_power_cap(INT_MAX, 178));     // recorta a +cap
}

// motor_cap_from_pct: traducción y clamps de pct fuera de rango.
void test_cap_from_pct(void) {
    TEST_ASSERT_EQUAL_INT(178, motor_cap_from_pct(255, 70));
    TEST_ASSERT_EQUAL_INT(0,   motor_cap_from_pct(255, 0));     // 0% → 0 (motor_power_cap = OFF)
    TEST_ASSERT_EQUAL_INT(255, motor_cap_from_pct(255, 100));   // 100% → escala completa
    TEST_ASSERT_EQUAL_INT(127, motor_cap_from_pct(255, 50));    // 255*50/100 = 127 (trunc)
    // pct fuera de rango se clampea: negativo → 0, >100 → escala (no amplifica).
    TEST_ASSERT_EQUAL_INT(0,   motor_cap_from_pct(255, -10));
    TEST_ASSERT_EQUAL_INT(255, motor_cap_from_pct(255, 200));
    // Escala distinta de 255 también funciona (per-robot futuro).
    TEST_ASSERT_EQUAL_INT(70,  motor_cap_from_pct(100, 70));
}

// INTEGRACIÓN: pct → cap → power_cap, el camino real del wiring (banco).
void test_pct_to_cap_to_power_cap_pipeline(void) {
    int cap = motor_cap_from_pct(MOTOR_POWER_CAP_MAX, 70);
    TEST_ASSERT_EQUAL_INT(178, cap);
    // Un comando que pedía duty máximo queda limitado al 70% de seguridad.
    TEST_ASSERT_EQUAL_INT(178, motor_power_cap(MOTOR_POWER_CAP_MAX, cap));
    // Con pct=100 (cap OFF efectivo, == MAX) nada se recorta: byte-idéntico.
    int cap100 = motor_cap_from_pct(MOTOR_POWER_CAP_MAX, 100);
    TEST_ASSERT_EQUAL_INT(255, motor_power_cap(255, cap100));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_passthrough_when_cap_ge_max);
    RUN_TEST(test_passthrough_when_cap_off);
    RUN_TEST(test_clamp_symmetric_sign_preserving);
    RUN_TEST(test_seventy_percent_of_255_is_178);
    RUN_TEST(test_edge_zero_and_at_cap);
    RUN_TEST(test_edge_int_min_no_ub);
    RUN_TEST(test_cap_from_pct);
    RUN_TEST(test_pct_to_cap_to_power_cap_pipeline);
    return UNITY_END();
}
