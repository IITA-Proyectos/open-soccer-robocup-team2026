// test_otos_health — máquina de salud por OTOS (audit 2026-06-03 #6/#13/#15).
// Corre host con: bash scripts/run-host-tests.sh test_otos_health
//
// Cubre la lógica PURA que reemplaza los viejos ready-flags de solo-boot:
//   • N fallos consecutivos bajan `alive` (histéresis anti-flapping).
//   • Un fallo aislado NO baja alive (resetea el contador).
//   • Una vez caído, lecturas OK NO re-habilitan (política latch hasta reset).
//   • La confidence emitida se deriva de la salud ACTUAL de ambos OTOS.

#include <unity.h>
#include "otos_health.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Estado inicial: .bss zero-init == no presente, no vivo.
void test_zero_init_is_absent(void) {
    OtosHealth h{};
    TEST_ASSERT_FALSE(h.present);
    TEST_ASSERT_FALSE(h.alive);
    TEST_ASSERT_EQUAL_UINT8(0, h.consec_fail);
}

// Detección de boot OK → presente y vivo.
void test_set_present_detected_makes_alive(void) {
    OtosHealth h{};
    oh_set_present(h, true);
    TEST_ASSERT_TRUE(h.present);
    TEST_ASSERT_TRUE(h.alive);
}

// Detección de boot fallida → presente=false, no vivo; oh_update no lo revive.
void test_set_present_not_detected_stays_dead(void) {
    OtosHealth h{};
    oh_set_present(h, false);
    TEST_ASSERT_FALSE(h.present);
    TEST_ASSERT_FALSE(h.alive);
    // Aunque lleguen lecturas "OK", nunca estuvo presente → sigue muerto.
    TEST_ASSERT_FALSE(oh_update(h, true));
    TEST_ASSERT_FALSE(h.alive);
}

// Histéresis: hasta N-1 fallos consecutivos NO baja alive.
void test_fewer_than_threshold_fails_stays_alive(void) {
    OtosHealth h{};
    oh_set_present(h, true);
    for (uint8_t i = 0; i < OTOS_HEALTH_FAIL_THRESHOLD - 1; ++i) {
        TEST_ASSERT_TRUE(oh_update(h, false));   // sigue vivo
    }
    TEST_ASSERT_TRUE(h.alive);
}

// N fallos consecutivos bajan alive (caída).
void test_threshold_fails_drops_alive(void) {
    OtosHealth h{};
    oh_set_present(h, true);
    for (uint8_t i = 0; i < OTOS_HEALTH_FAIL_THRESHOLD; ++i) {
        oh_update(h, false);
    }
    TEST_ASSERT_FALSE(h.alive);
}

// Un OK aislado en medio de fallos RESETEA el contador → no cae por glitch.
void test_isolated_ok_resets_fail_counter(void) {
    OtosHealth h{};
    oh_set_present(h, true);
    // 2 fallos (umbral=3, todavía no cae), luego 1 OK que limpia la racha.
    oh_update(h, false);
    oh_update(h, false);
    TEST_ASSERT_TRUE(oh_update(h, true));
    TEST_ASSERT_EQUAL_UINT8(0, h.consec_fail);
    // Otros 2 fallos: como el contador se reinició, sigue vivo (no acumuló 3+).
    oh_update(h, false);
    TEST_ASSERT_TRUE(oh_update(h, false));
    TEST_ASSERT_TRUE(h.alive);
}

// LATCH: una vez caído, lecturas OK NO re-habilitan solo (audit #6 decisión).
void test_latch_ok_does_not_rehabilitate(void) {
    OtosHealth h{};
    oh_set_present(h, true);
    for (uint8_t i = 0; i < OTOS_HEALTH_FAIL_THRESHOLD; ++i) oh_update(h, false);
    TEST_ASSERT_FALSE(h.alive);
    // Muchas lecturas OK: sigue caído (hasta un oh_reset/oh_set_present externo).
    for (int i = 0; i < 20; ++i) {
        TEST_ASSERT_FALSE(oh_update(h, true));
    }
    TEST_ASSERT_FALSE(h.alive);
    // Re-detección explícita (CENTRAL_RESET_OTOS / reboot) sí lo revive.
    oh_set_present(h, true);
    TEST_ASSERT_TRUE(h.alive);
}

// El contador de fallos satura, no wrappea (wrap-safe).
void test_fail_counter_saturates_no_wrap(void) {
    OtosHealth h{};
    oh_set_present(h, true);
    for (int i = 0; i < 1000; ++i) oh_update(h, false);
    TEST_ASSERT_EQUAL_UINT8(0xFF, h.consec_fail);   // saturó, no volvió a 0
    TEST_ASSERT_FALSE(h.alive);
}

// oh_reset deja el estado limpio (no presente, no vivo).
void test_reset_clears_state(void) {
    OtosHealth h{};
    oh_set_present(h, true);
    oh_update(h, false);
    oh_reset(h);
    TEST_ASSERT_FALSE(h.present);
    TEST_ASSERT_FALSE(h.alive);
    TEST_ASSERT_EQUAL_UINT8(0, h.consec_fail);
}

// Confidence: ambos vivos=100, uno vivo=60 (single), ninguno=0.
void test_pose_confidence_from_health(void) {
    OtosHealth l{}, r{};
    oh_set_present(l, true);
    oh_set_present(r, true);
    TEST_ASSERT_EQUAL_UINT8(100, oh_pose_confidence(l, r));

    // Tirar el derecho: degrada a single (60), no a 0.
    for (uint8_t i = 0; i < OTOS_HEALTH_FAIL_THRESHOLD; ++i) oh_update(r, false);
    TEST_ASSERT_EQUAL_UINT8(60, oh_pose_confidence(l, r));

    // Tirar también el izquierdo: 0 (la pose ya no es confiable).
    for (uint8_t i = 0; i < OTOS_HEALTH_FAIL_THRESHOLD; ++i) oh_update(l, false);
    TEST_ASSERT_EQUAL_UINT8(0, oh_pose_confidence(l, r));
}

// Escenario integrado del audit #6: OTOS se desconecta mid-match → confidence
// pasa de 100 a 60, NO se queda mintiendo 100.
void test_disconnect_midmatch_lowers_confidence(void) {
    OtosHealth l{}, r{};
    oh_set_present(l, true);
    oh_set_present(r, true);
    // Operación normal: muchas lecturas OK.
    for (int i = 0; i < 50; ++i) { oh_update(l, true); oh_update(r, true); }
    TEST_ASSERT_EQUAL_UINT8(100, oh_pose_confidence(l, r));
    // El cable Qwiic del derecho se afloja: lecturas fallan.
    for (int i = 0; i < 10; ++i) { oh_update(l, true); oh_update(r, false); }
    TEST_ASSERT_TRUE(l.alive);
    TEST_ASSERT_FALSE(r.alive);
    TEST_ASSERT_EQUAL_UINT8(60, oh_pose_confidence(l, r));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_init_is_absent);
    RUN_TEST(test_set_present_detected_makes_alive);
    RUN_TEST(test_set_present_not_detected_stays_dead);
    RUN_TEST(test_fewer_than_threshold_fails_stays_alive);
    RUN_TEST(test_threshold_fails_drops_alive);
    RUN_TEST(test_isolated_ok_resets_fail_counter);
    RUN_TEST(test_latch_ok_does_not_rehabilitate);
    RUN_TEST(test_fail_counter_saturates_no_wrap);
    RUN_TEST(test_reset_clears_state);
    RUN_TEST(test_pose_confidence_from_health);
    RUN_TEST(test_disconnect_midmatch_lowers_confidence);
    return UNITY_END();
}
