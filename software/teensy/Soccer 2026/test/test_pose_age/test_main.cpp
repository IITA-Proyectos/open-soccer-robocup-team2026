// test_pose_age — edad (ms) de un dato con sello de recepción (INC-2 RT 2026-06-15).
// Corre host: bash scripts/run-host-tests.sh test_pose_age
//
// Cubre el contrato CLAVE: nunca-recibido => edad MÁXIMA (no 0), para que el gate de
// pose_fusion no confunda "nunca llegó" con "fresquísimo". + wrap-safe de millis().

#include <unity.h>
#include "pose_age.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ever_received=false => POSE_AGE_NEVER (no 0): el bug que el helper previene.
void test_never_received_is_max(void) {
    TEST_ASSERT_EQUAL_UINT32(POSE_AGE_NEVER, pose_age_ms_pure(0, 1000, false));
    // Aun con un last_rx_ms cualquiera, si ever_received=false => NEVER.
    TEST_ASSERT_EQUAL_UINT32(POSE_AGE_NEVER, pose_age_ms_pure(500, 1000, false));
}

// last_rx_ms==0 es centinela de "nunca" aunque ever_received venga true.
void test_zero_last_rx_is_max(void) {
    TEST_ASSERT_EQUAL_UINT32(POSE_AGE_NEVER, pose_age_ms_pure(0, 1000, true));
}

// Edad normal = now - last (recibido).
void test_normal_age(void) {
    TEST_ASSERT_EQUAL_UINT32(10u,  pose_age_ms_pure(990, 1000, true));
    TEST_ASSERT_EQUAL_UINT32(0u,   pose_age_ms_pure(1000, 1000, true));   // mismísimo ms
    TEST_ASSERT_EQUAL_UINT32(250u, pose_age_ms_pure(1000, 1250, true));
}

// WRAP de millis(): last cerca de 0xFFFFFFFF, now ya envuelto a un valor chico.
// La resta unsigned da el delta REAL chico, no un número gigante.
void test_wrap_safe(void) {
    // last = 0xFFFFFFF0 (16 antes del overflow), now = 5 (5 ms después del wrap) => 21 ms.
    TEST_ASSERT_EQUAL_UINT32(21u, pose_age_ms_pure(0xFFFFFFF0u, 5u, true));
    // Cruce EXACTO del overflow: last en el último tick, now en 0 => 1 ms.
    TEST_ASSERT_EQUAL_UINT32(1u, pose_age_ms_pure(0xFFFFFFFFu, 0u, true));
    // Mismo ms en el límite => 0.
    TEST_ASSERT_EQUAL_UINT32(0u, pose_age_ms_pure(0xFFFFFFFFu, 0xFFFFFFFFu, true));
}

// Gating fino: fresco si age < max; NEVER nunca es fresco.
void test_is_fresh_gate(void) {
    TEST_ASSERT_TRUE(pose_age_is_fresh(10u, 60u));
    TEST_ASSERT_FALSE(pose_age_is_fresh(60u, 60u));    // corte estricto (>=)
    TEST_ASSERT_FALSE(pose_age_is_fresh(100u, 60u));
    TEST_ASSERT_FALSE(pose_age_is_fresh(POSE_AGE_NEVER, 60u));   // nunca-recibido NO es fresco
    TEST_ASSERT_FALSE(pose_age_is_fresh(POSE_AGE_NEVER, 500u));  // ni con umbral grueso
    // Bordes inferiores: edad 0 con umbral 0 => no fresco (corte estricto); edad 0 con
    // umbral 60 => fresco; max_age==0 degenera a "nada es fresco".
    TEST_ASSERT_TRUE(pose_age_is_fresh(0u, 60u));
    TEST_ASSERT_FALSE(pose_age_is_fresh(0u, 0u));
    TEST_ASSERT_FALSE(pose_age_is_fresh(1u, 1u));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_never_received_is_max);
    RUN_TEST(test_zero_last_rx_is_max);
    RUN_TEST(test_normal_age);
    RUN_TEST(test_wrap_safe);
    RUN_TEST(test_is_fresh_gate);
    return UNITY_END();
}
