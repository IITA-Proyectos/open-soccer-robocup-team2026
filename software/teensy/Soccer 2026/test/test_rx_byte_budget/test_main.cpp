// test_rx_byte_budget — presupuesto de bytes/tick que acota el WCET del tick RX (DOWN RT §8.1, F5).
// Corre host: bash scripts/run-host-tests.sh test_rx_byte_budget

#include <unity.h>
#include "rx_byte_budget.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Hasta max devuelve true; el (max+1)-ésimo devuelve false. Corte ESTRICTO en el techo.
void test_allow_hasta_max_luego_false(void) {
    RxByteBudget b;
    rxbb_init(b, 3);
    TEST_ASSERT_TRUE(rxbb_allow(b));    // 1
    TEST_ASSERT_TRUE(rxbb_allow(b));    // 2
    TEST_ASSERT_TRUE(rxbb_allow(b));    // 3 (== max)
    TEST_ASSERT_FALSE(rxbb_allow(b));   // 4 → techo alcanzado, corta
    TEST_ASSERT_FALSE(rxbb_allow(b));   // sigue cortando, no desborda
}

// reset reinicia el tick: tras agotar el presupuesto, reset vuelve a permitir max bytes.
void test_reset_reinicia(void) {
    RxByteBudget b;
    rxbb_init(b, 2);
    TEST_ASSERT_TRUE(rxbb_allow(b));
    TEST_ASSERT_TRUE(rxbb_allow(b));
    TEST_ASSERT_FALSE(rxbb_allow(b));   // agotado
    rxbb_reset(b);                      // arranca un tick nuevo
    TEST_ASSERT_TRUE(rxbb_allow(b));    // de nuevo permite max bytes
    TEST_ASSERT_TRUE(rxbb_allow(b));
    TEST_ASSERT_FALSE(rxbb_allow(b));
}

// reset conserva el techo (no lo borra): sigue siendo max_per_tick tras varios resets.
void test_reset_conserva_techo(void) {
    RxByteBudget b;
    rxbb_init(b, 5);
    rxbb_allow(b); rxbb_allow(b);
    rxbb_reset(b);
    TEST_ASSERT_EQUAL_INT(5, b.max_per_tick);
    TEST_ASSERT_EQUAL_INT(0, b.used);
    TEST_ASSERT_EQUAL_INT(5, rxbb_remaining(b));
}

// fail-safe: max_per_tick = 0 (o zero-init .bss) → allow SIEMPRE false (presupuesto cerrado,
// nunca monopoliza el loop). Degrada a "no recibo", jamás a "robo el tick".
void test_techo_cero_deniega(void) {
    RxByteBudget b{};                   // zero-init: max_per_tick = 0
    TEST_ASSERT_FALSE(rxbb_allow(b));
    TEST_ASSERT_EQUAL_INT(0, rxbb_remaining(b));
    rxbb_init(b, 0);                    // explícito: techo 0
    TEST_ASSERT_FALSE(rxbb_allow(b));
}

// fail-safe: techo negativo (mal seteado) también deniega — nunca abre el grifo.
void test_techo_negativo_deniega(void) {
    RxByteBudget b;
    rxbb_init(b, -10);
    TEST_ASSERT_FALSE(rxbb_allow(b));
    TEST_ASSERT_EQUAL_INT(0, rxbb_remaining(b));
}

// remaining va decreciendo con cada allow y nunca baja de 0.
void test_remaining_decrece(void) {
    RxByteBudget b;
    rxbb_init(b, 2);
    TEST_ASSERT_EQUAL_INT(2, rxbb_remaining(b));
    rxbb_allow(b);
    TEST_ASSERT_EQUAL_INT(1, rxbb_remaining(b));
    rxbb_allow(b);
    TEST_ASSERT_EQUAL_INT(0, rxbb_remaining(b));
    rxbb_allow(b);                      // ya agotado: no desborda
    TEST_ASSERT_EQUAL_INT(0, rxbb_remaining(b));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_allow_hasta_max_luego_false);
    RUN_TEST(test_reset_reinicia);
    RUN_TEST(test_reset_conserva_techo);
    RUN_TEST(test_techo_cero_deniega);
    RUN_TEST(test_techo_negativo_deniega);
    RUN_TEST(test_remaining_decrece);
    return UNITY_END();
}
