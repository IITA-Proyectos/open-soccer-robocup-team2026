// test_state_timer — tests del timeout/watchdog POR ESTADO no bloqueante
// (src/shared/state_timer.h; lazo RT CENTRAL, pedido Gustavo 2026-06-14).
// Corre en host con: bash scripts/run-host-tests.sh test_state_timer
//
// Contrato:
//   • Recién armado → NO expira (hay que esperar el timeout completo).
//   • Justo en el borde (elapsed == timeout) → expira (corte >=, no >).
//   • Pasado el timeout → expira.
//   • Sin armar → NUNCA expira, y elapsed = 0 (un estado sin watchdog no escapa
//     por tiempo).
//   • Re-armar reinicia la cuenta desde cero (resetea el reloj de arena).
//   • timeout == 0 sobre un timer armado → expira de inmediato (paso fugaz).
//   • WRAP-SAFE: si millis() da la vuelta (0xFFFFFFFF → 0) mientras se espera, la
//     resta unsigned igual da el intervalo real → expira cuando corresponde.

#include <unity.h>
#include "state_timer.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// --- Recién armado: todavía no expira -----------------------------------------
void test_recien_armado_no_expira(void){
    StateTimer t{};                 // zero-init: armed=false
    st_arm(t, 1000u);               // se arma en t=1000
    // Mismo instante y un poco después, pero antes del timeout → NO expira.
    TEST_ASSERT_FALSE(st_expired(t, 1000u, 500u));   // elapsed 0
    TEST_ASSERT_FALSE(st_expired(t, 1499u, 500u));   // elapsed 499 < 500
}

// --- Expira al alcanzar / pasar el timeout ------------------------------------
void test_expira_en_el_borde_y_despues(void){
    StateTimer t{};
    st_arm(t, 1000u);
    TEST_ASSERT_TRUE(st_expired(t, 1500u, 500u));    // elapsed 500 == timeout → expira
    TEST_ASSERT_TRUE(st_expired(t, 1700u, 500u));    // elapsed 700 >  timeout → expira
}

// --- elapsed mide el tiempo transcurrido --------------------------------------
void test_elapsed_cuenta_desde_el_arm(void){
    StateTimer t{};
    st_arm(t, 2000u);
    TEST_ASSERT_EQUAL_UINT32(0u,   st_elapsed(t, 2000u));
    TEST_ASSERT_EQUAL_UINT32(350u, st_elapsed(t, 2350u));
}

// --- Sin armar: nunca expira, elapsed 0 ---------------------------------------
void test_sin_armar_nunca_expira(void){
    StateTimer t{};                 // zero-init, jamás armado
    TEST_ASSERT_FALSE(st_expired(t, 5000u, 100u));   // sin watchdog → no escapa
    TEST_ASSERT_FALSE(st_expired(t, 0u,    0u));
    TEST_ASSERT_EQUAL_UINT32(0u, st_elapsed(t, 9999u));
}

// --- Re-armar reinicia la cuenta ----------------------------------------------
void test_rearm_reinicia(void){
    StateTimer t{};
    st_arm(t, 1000u);
    TEST_ASSERT_TRUE(st_expired(t, 1600u, 500u));    // ya había vencido...
    st_arm(t, 1600u);                                // ...pero re-armo el reloj
    TEST_ASSERT_FALSE(st_expired(t, 1600u, 500u));   // recién armado → no expira
    TEST_ASSERT_EQUAL_UINT32(0u, st_elapsed(t, 1600u));
    TEST_ASSERT_TRUE(st_expired(t, 2100u, 500u));    // 500 ms después del re-arm → expira
}

// --- timeout 0 sobre timer armado: expira de inmediato ------------------------
void test_timeout_cero_expira_ya(void){
    StateTimer t{};
    st_arm(t, 1234u);
    TEST_ASSERT_TRUE(st_expired(t, 1234u, 0u));      // paso fugaz: vence al instante
}

// --- WRAP-SAFE: millis() da la vuelta mientras se espera ----------------------
void test_wrap_safe(void){
    // Se arma 100 ms ANTES del overflow de uint32 y se evalúa 300 ms DESPUÉS del
    // overflow: el intervalo real es 400 ms. La resta unsigned lo da correcto.
    const uint32_t casi_max = 0xFFFFFFFFu - 100u + 1u;  // arm: faltan 100 para el wrap
    StateTimer t{};
    st_arm(t, casi_max);
    const uint32_t now = 300u;        // 100 ms hasta el wrap + 300 ms después = 400 ms
    TEST_ASSERT_EQUAL_UINT32(400u, st_elapsed(t, now));   // intervalo real, sin salto
    TEST_ASSERT_FALSE(st_expired(t, now, 500u));          // 400 < 500 → no expira
    TEST_ASSERT_TRUE (st_expired(t, now, 400u));          // 400 >= 400 → expira
    TEST_ASSERT_TRUE (st_expired(t, now, 350u));          // 400 >= 350 → expira
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_recien_armado_no_expira);
    RUN_TEST(test_expira_en_el_borde_y_despues);
    RUN_TEST(test_elapsed_cuenta_desde_el_arm);
    RUN_TEST(test_sin_armar_nunca_expira);
    RUN_TEST(test_rearm_reinicia);
    RUN_TEST(test_timeout_cero_expira_ya);
    RUN_TEST(test_wrap_safe);
    return UNITY_END();
}
