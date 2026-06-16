// test_rx_calib_defer — buzón último-gana que difiere el calibrate bloqueante (DOWN RT §8.1, F5).
// Corre host: bash scripts/run-host-tests.sh test_rx_calib_defer

#include <unity.h>
#include "rx_calib_defer.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Zero-init (.bss) == buzón vacío: take sin pending devuelve false y no toca kind_out.
void test_zero_init_vacio(void) {
    RxCalibDefer s{};                       // value-init = ceros
    TEST_ASSERT_FALSE(rxcd_pending(s));
    uint8_t kind = 0xAB;                    // centinela: take NO debe pisarlo si no hay pedido
    TEST_ASSERT_FALSE(rxcd_take(s, &kind));
    TEST_ASSERT_EQUAL_UINT8(0xAB, kind);    // intacto
}

// request luego take: devuelve true y entrega el kind pedido. Tras take, el buzón queda vacío.
void test_request_take_devuelve_kind(void) {
    RxCalibDefer s{};
    rxcd_request(s, 7);
    TEST_ASSERT_TRUE(rxcd_pending(s));
    uint8_t kind = 0;
    TEST_ASSERT_TRUE(rxcd_take(s, &kind));
    TEST_ASSERT_EQUAL_UINT8(7, kind);
    // Consumido: el mismo pedido NO se re-ejecuta.
    TEST_ASSERT_FALSE(rxcd_pending(s));
    uint8_t kind2 = 0x55;
    TEST_ASSERT_FALSE(rxcd_take(s, &kind2));
    TEST_ASSERT_EQUAL_UINT8(0x55, kind2);   // intacto: no había nada
}

// Doble request ÚLTIMO-GANA: el segundo pisa al primero; take entrega el más reciente.
void test_doble_request_ultimo_gana(void) {
    RxCalibDefer s{};
    rxcd_request(s, 1);
    rxcd_request(s, 2);                     // pisa al 1 (un solo casillero)
    uint8_t kind = 0;
    TEST_ASSERT_TRUE(rxcd_take(s, &kind));
    TEST_ASSERT_EQUAL_UINT8(2, kind);       // gana el último
    TEST_ASSERT_FALSE(rxcd_pending(s));     // un solo pedido pendiente, ya consumido
}

// take con kind_out nullptr: consume igual (no crashea), limpia el buzón y devuelve true.
void test_take_nullptr_consume_igual(void) {
    RxCalibDefer s{};
    rxcd_request(s, 9);
    TEST_ASSERT_TRUE(rxcd_take(s, nullptr));   // descarta el kind pero consume
    TEST_ASSERT_FALSE(rxcd_pending(s));
}

// Ciclo completo: request → take → vacío → request de nuevo → take. El buzón se reutiliza.
void test_ciclo_reutilizable(void) {
    RxCalibDefer s{};
    uint8_t kind = 0;

    rxcd_request(s, 3);
    TEST_ASSERT_TRUE(rxcd_take(s, &kind));
    TEST_ASSERT_EQUAL_UINT8(3, kind);

    TEST_ASSERT_FALSE(rxcd_take(s, &kind));    // ya vacío

    rxcd_request(s, 4);                        // nuevo pedido tras consumir el anterior
    TEST_ASSERT_TRUE(rxcd_take(s, &kind));
    TEST_ASSERT_EQUAL_UINT8(4, kind);
    TEST_ASSERT_FALSE(rxcd_pending(s));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_init_vacio);
    RUN_TEST(test_request_take_devuelve_kind);
    RUN_TEST(test_doble_request_ultimo_gana);
    RUN_TEST(test_take_nullptr_consume_igual);
    RUN_TEST(test_ciclo_reutilizable);
    return UNITY_END();
}
