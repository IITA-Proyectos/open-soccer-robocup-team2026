// test_tof_stale — TDD de la lógica PURA de frescura del ToF (P1-TOF-STALE).
//
// Cubre la función host-testeable tof_fresh_or_no_reading() de
// src/top/sensors_tof.h: dado el valor cacheado + el millis() de la última
// lectura buena + el now + un timeout, decide si devolver el dato (fresco) o
// TOF_NO_READING (vencido / nunca leyó). El glue Arduino (millis(), el array de
// sensores, getRangingData) queda fino en sensors_tof.cpp y NO se testea acá.
//
// Compila host (sin Arduino) gracias a -DTOF_PURE_HOST_TEST, que evita el
// include de config_top.h. Binario de track ÚNICO: /tmp/t_TRACK_A_tof_stale.
//
//   g++ -std=gnu++17 -DTOF_PURE_HOST_TEST -I "src/top" -I lib/Unity/src \
//       lib/Unity/src/unity.c test/test_tof_stale/test_main.cpp \
//       -o /tmp/t_TRACK_A_tof_stale && /tmp/t_TRACK_A_tof_stale

#define TOF_PURE_HOST_TEST 1
#include <unity.h>
// Ruta relativa: run-host-tests.sh compila con -I src/shared + -I <test_dir>,
// NO con -I src/top (mismo patron que test_link_health).
#include "../../src/top/sensors_tof.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

static constexpr uint32_t TO = TOF_STALE_TIMEOUT_MS;  // 250 ms

// ============================================================================
// FRESCO -> devuelve el valor cacheado
// ============================================================================

void test_fresh_returns_value(void) {
    // last_ok=1000, now=1100 -> elapsed 100 < 250 -> fresco.
    TEST_ASSERT_EQUAL_UINT16(
        80, tof_fresh_or_no_reading(80, 1000, /*ever_ok*/ true, 1100, TO));
}

void test_fresh_at_zero_elapsed(void) {
    // Lectura recién sellada (elapsed 0) -> fresco.
    TEST_ASSERT_EQUAL_UINT16(
        1234, tof_fresh_or_no_reading(1234, 5000, true, 5000, TO));
}

void test_fresh_exactly_at_timeout_boundary(void) {
    // elapsed == timeout (250) NO está vencido (la condición es elapsed > timeout).
    TEST_ASSERT_EQUAL_UINT16(
        300, tof_fresh_or_no_reading(300, 1000, true, 1000 + TO, TO));
}

// ============================================================================
// VENCIDO -> devuelve TOF_NO_READING
// ============================================================================

void test_stale_one_ms_past_timeout(void) {
    // elapsed = 251 > 250 -> vencido.
    TEST_ASSERT_EQUAL_UINT16(
        TOF_NO_READING,
        tof_fresh_or_no_reading(80, 1000, true, 1000 + TO + 1, TO));
}

void test_stale_far_past_timeout(void) {
    // El caso del backlog: un ToF colgado en 80 mm que dejó de leer hace 5 s.
    // Sin el fix se propagaba para siempre como min_obstacle; ahora expira.
    TEST_ASSERT_EQUAL_UINT16(
        TOF_NO_READING, tof_fresh_or_no_reading(80, 1000, true, 6000, TO));
}

// ============================================================================
// NUNCA LEYÓ OK -> TOF_NO_READING (no hay base de frescura)
// ============================================================================

void test_never_ok_returns_no_reading(void) {
    // ever_ok=false: aunque el cache tuviera un valor, no hay lectura buena base.
    TEST_ASSERT_EQUAL_UINT16(
        TOF_NO_READING, tof_fresh_or_no_reading(80, 0, /*ever_ok*/ false, 100, TO));
}

void test_cached_no_reading_stays_no_reading(void) {
    // El cache ya es NO_READING (sensor mirando al vacío) -> NO_READING aunque fresco.
    TEST_ASSERT_EQUAL_UINT16(
        TOF_NO_READING,
        tof_fresh_or_no_reading(TOF_NO_READING, 1000, true, 1050, TO));
}

// ============================================================================
// RECUPERACIÓN: tras vencer, una lectura nueva vuelve a estar fresca
// ============================================================================

void test_recovery_after_stale(void) {
    // Fase 1: vencido (último OK en t=1000, ahora t=2000).
    TEST_ASSERT_EQUAL_UINT16(
        TOF_NO_READING, tof_fresh_or_no_reading(80, 1000, true, 2000, TO));
    // Fase 2: llega una lectura nueva (cache=120, last_ok actualizado a t=2000);
    // al consultar en t=2050 vuelve a dar el dato bueno.
    TEST_ASSERT_EQUAL_UINT16(
        120, tof_fresh_or_no_reading(120, 2000, true, 2050, TO));
}

// ============================================================================
// WRAP-SAFE: si now retrocede (millis() wrap a ~49.7 días), la resta unsigned
// sigue dando el elapsed correcto y NO dispara stale espurio.
// ============================================================================

void test_wrap_around_does_not_falsely_stale(void) {
    // last_ok cerca del tope de uint32_t; now wrapeó a un valor chico.
    // elapsed real = 100 ms (50 antes del wrap + 50 después). Unsigned: 100.
    const uint32_t last_ok = 0xFFFFFFFFu - 49;  // 50 ms antes del wrap
    const uint32_t now     = 50;                 // 50 ms después del wrap
    // elapsed = now - last_ok = 50 - (0xFFFFFFFF-49) = 100 (mod 2^32).
    TEST_ASSERT_EQUAL_UINT32(100u, now - last_ok);
    TEST_ASSERT_EQUAL_UINT16(
        80, tof_fresh_or_no_reading(80, last_ok, true, now, TO));
}

void test_wrap_around_still_detects_real_stale(void) {
    // Mismo escenario de wrap pero el elapsed real SÍ supera el timeout (300 ms).
    const uint32_t last_ok = 0xFFFFFFFFu - 49;  // 50 ms antes del wrap
    const uint32_t now     = 250;                // 250 ms después del wrap
    TEST_ASSERT_EQUAL_UINT32(300u, now - last_ok);
    TEST_ASSERT_EQUAL_UINT16(
        TOF_NO_READING, tof_fresh_or_no_reading(80, last_ok, true, now, TO));
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fresh_returns_value);
    RUN_TEST(test_fresh_at_zero_elapsed);
    RUN_TEST(test_fresh_exactly_at_timeout_boundary);
    RUN_TEST(test_stale_one_ms_past_timeout);
    RUN_TEST(test_stale_far_past_timeout);
    RUN_TEST(test_never_ok_returns_no_reading);
    RUN_TEST(test_cached_no_reading_stays_no_reading);
    RUN_TEST(test_recovery_after_stale);
    RUN_TEST(test_wrap_around_does_not_falsely_stale);
    RUN_TEST(test_wrap_around_still_detects_real_stale);
    return UNITY_END();
}
