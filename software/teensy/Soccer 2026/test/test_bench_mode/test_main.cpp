// test_bench_mode — tests del módulo puro bench_mode.{h,cpp}.
// Corre con: bash scripts/run-host-tests.sh test_bench_mode
//            (o pio test -e test_native -f test_bench_mode con red)
//
// El módulo es fail-safe a COMPETENCIA: lo que más cubrimos es que NUNCA quede
// en PRUEBA por accidente (timeout, magic incorrecto, wrap de millis, y un
// barrido ADVERSARIAL de cientos de magics aleatorios).

#include <unity.h>
#include "bench_mode.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ── Defaults / init ──────────────────────────────────────────────────────────

void test_init_is_competencia(void) {
    BenchHold b;
    bench_hold_init(b);
    TEST_ASSERT_FALSE(b.ever);
    TEST_ASSERT_EQUAL(BenchMode::COMPETENCIA, bench_hold_mode(b, 0));
    TEST_ASSERT_EQUAL(BenchMode::COMPETENCIA, bench_hold_mode(b, 1000));
    TEST_ASSERT_EQUAL(BenchMode::COMPETENCIA, bench_hold_mode(b, 0xFFFFFFFFu));
    TEST_ASSERT_FALSE(bench_hold_in_prueba(b, 1000));
}

// ── Heartbeat válido sostiene PRUEBA ─────────────────────────────────────────

void test_valid_magic_enters_prueba_within_timeout(void) {
    BenchHold b;
    bench_hold_init(b);
    bench_hold_feed(b, BENCH_TOKEN_MAGIC, 1000);
    TEST_ASSERT_TRUE(b.ever);
    // Mismo instante y dentro de la ventana → PRUEBA.
    TEST_ASSERT_EQUAL(BenchMode::PRUEBA, bench_hold_mode(b, 1000));
    TEST_ASSERT_EQUAL(BenchMode::PRUEBA, bench_hold_mode(b, 1000 + 1));
    TEST_ASSERT_EQUAL(BenchMode::PRUEBA, bench_hold_mode(b, 1000 + BENCH_HOLD_TIMEOUT_MS));
    TEST_ASSERT_TRUE(bench_hold_in_prueba(b, 1000 + BENCH_HOLD_TIMEOUT_MS));
}

// ── Timeout revierte a COMPETENCIA ───────────────────────────────────────────

void test_timeout_reverts_to_competencia(void) {
    BenchHold b;
    bench_hold_init(b);
    bench_hold_feed(b, BENCH_TOKEN_MAGIC, 1000);
    // Justo en el borde sigue PRUEBA; un ms más allá → COMPETENCIA.
    TEST_ASSERT_EQUAL(BenchMode::PRUEBA, bench_hold_mode(b, 1000 + BENCH_HOLD_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(BenchMode::COMPETENCIA, bench_hold_mode(b, 1000 + BENCH_HOLD_TIMEOUT_MS + 1));
    TEST_ASSERT_EQUAL(BenchMode::COMPETENCIA, bench_hold_mode(b, 1000 + 10000));
    TEST_ASSERT_FALSE(bench_hold_in_prueba(b, 1000 + BENCH_HOLD_TIMEOUT_MS + 1));
}

// ── Magic incorrecto NO refresca ─────────────────────────────────────────────

void test_wrong_magic_never_enters_prueba(void) {
    BenchHold b;
    bench_hold_init(b);
    // Magics "cercanos" y valores triviales de ruido: ninguno habilita PRUEBA.
    bench_hold_feed(b, 0x0000, 1000);
    bench_hold_feed(b, 0xFFFF, 1000);
    bench_hold_feed(b, (uint16_t)(BENCH_TOKEN_MAGIC + 1), 1000);
    bench_hold_feed(b, (uint16_t)(BENCH_TOKEN_MAGIC - 1), 1000);
    bench_hold_feed(b, (uint16_t)(BENCH_TOKEN_MAGIC ^ 0x0001), 1000);  // 1 bit flip
    TEST_ASSERT_FALSE(b.ever);
    TEST_ASSERT_EQUAL(BenchMode::COMPETENCIA, bench_hold_mode(b, 1000));
    TEST_ASSERT_EQUAL(BenchMode::COMPETENCIA, bench_hold_mode(b, 1100));
}

void test_wrong_magic_does_not_refresh_existing_hold(void) {
    BenchHold b;
    bench_hold_init(b);
    bench_hold_feed(b, BENCH_TOKEN_MAGIC, 1000);   // entra en PRUEBA
    TEST_ASSERT_EQUAL(BenchMode::PRUEBA, bench_hold_mode(b, 1200));
    // Un magic malo a t=1200 NO debe "renovar" el hold: a t=1400 (que está a
    // >300 ms del último token BUENO @1000) ya tiene que haber decaído.
    bench_hold_feed(b, 0xDEAD, 1200);
    TEST_ASSERT_EQUAL(BenchMode::COMPETENCIA, bench_hold_mode(b, 1400));
}

// ── Re-feed (heartbeat) sostiene PRUEBA más allá de un solo timeout ──────────

void test_refeed_sustains_prueba(void) {
    BenchHold b;
    bench_hold_init(b);
    uint32_t t = 1000;
    bench_hold_feed(b, BENCH_TOKEN_MAGIC, t);
    // Latido cada 100 ms (< timeout 300) durante mucho tiempo: siempre PRUEBA.
    for (int i = 0; i < 200; ++i) {
        t += 100;
        bench_hold_feed(b, BENCH_TOKEN_MAGIC, t);
        TEST_ASSERT_EQUAL(BenchMode::PRUEBA, bench_hold_mode(b, t));
        TEST_ASSERT_EQUAL(BenchMode::PRUEBA, bench_hold_mode(b, t + 50));
    }
    // Y si el latido se corta, decae al COMPETENCIA dentro del timeout.
    TEST_ASSERT_EQUAL(BenchMode::COMPETENCIA, bench_hold_mode(b, t + BENCH_HOLD_TIMEOUT_MS + 1));
}

// ── Wrap-safe: now < last_ok (millis envolvió) ──────────────────────────────

void test_wrap_safe_no_false_fresh(void) {
    BenchHold b;
    bench_hold_init(b);
    // Token cerca del tope de uint32 (justo antes del wrap a ~49.7 días).
    const uint32_t near_top = 0xFFFFFFFFu - 100;  // last_ok
    bench_hold_feed(b, BENCH_TOKEN_MAGIC, near_top);
    // now apenas después del wrap: elapsed real = 100 + 50 = 150 ms (< timeout)
    // → debe SEGUIR en PRUEBA gracias a la resta unsigned.
    const uint32_t after_wrap = 49;  // (near_top + 150) mod 2^32
    TEST_ASSERT_EQUAL(BenchMode::PRUEBA, bench_hold_mode(b, after_wrap));

    // Y un now mucho más adelante tras el wrap (elapsed real > timeout) revierte.
    const uint32_t after_wrap_late = 100 + BENCH_HOLD_TIMEOUT_MS + 1 - 100;  // elapsed=302
    // elapsed = after_wrap_late - near_top (unsigned) = (after_wrap_late + 100) + 1
    TEST_ASSERT_EQUAL(BenchMode::COMPETENCIA, bench_hold_mode(b, after_wrap_late));
}

void test_wrap_safe_now_less_than_last_is_not_huge_elapsed(void) {
    // Caso directo: last_ok grande, now chico (sin haber pasado el timeout real).
    // Una resta SIGNED o un branch "now < last → stale" daría un falso resultado;
    // la resta unsigned da el elapsed correcto.
    BenchHold b;
    bench_hold_init(b);
    b.ever = true;
    b.last_ok_ms = 0xFFFFFFF0u;          // last_ok ~ tope
    const uint32_t now = 0x00000005u;    // elapsed unsigned = 0x15 = 21 ms
    TEST_ASSERT_EQUAL(BenchMode::PRUEBA, bench_hold_mode(b, now));
}

// ── ADVERSARIAL: cientos de magics aleatorios NUNCA habilitan PRUEBA ─────────

void test_adversarial_random_magics_never_enter_prueba(void) {
    BenchHold b;
    bench_hold_init(b);
    uint32_t now = 0;
    for (uint32_t i = 0; i < 1000; ++i) {
        now += 7;  // el tiempo avanza un poco en cada iteración
        // "Aleatorio" determinista por índice (Knuth multiplicative hash),
        // truncado a 16 bits. Saltamos el valor correcto explícitamente.
        uint16_t magic = (uint16_t)((i * 2654435761u) & 0xFFFFu);
        if (magic == BENCH_TOKEN_MAGIC) {
            continue;  // jamás alimentamos el magic bueno
        }
        bench_hold_feed(b, magic, now);
        // En NINGÚN punto debe quedar en PRUEBA.
        TEST_ASSERT_FALSE(b.ever);
        TEST_ASSERT_EQUAL(BenchMode::COMPETENCIA, bench_hold_mode(b, now));
        TEST_ASSERT_FALSE(bench_hold_in_prueba(b, now));
    }
}

// Variante adversarial: el robot YA está en PRUEBA y lo bombardean con magics
// aleatorios. No deben prolongar el hold: si el magic bueno deja de llegar,
// decae igual aunque lluevan tokens basura.
void test_adversarial_garbage_cannot_prolong_hold(void) {
    BenchHold b;
    bench_hold_init(b);
    bench_hold_feed(b, BENCH_TOKEN_MAGIC, 1000);  // último token BUENO @1000
    uint32_t now = 1000;
    for (uint32_t i = 0; i < 1000; ++i) {
        now += 1;  // avanza ms a ms más allá del timeout
        uint16_t magic = (uint16_t)((i * 2654435761u) & 0xFFFFu);
        if (magic == BENCH_TOKEN_MAGIC) magic ^= 0x0001;  // forzar magic malo
        bench_hold_feed(b, magic, now);
    }
    // Pasaron ~1000 ms desde el último token bueno → COMPETENCIA, pese al spam.
    TEST_ASSERT_EQUAL(BenchMode::COMPETENCIA, bench_hold_mode(b, now));
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_init_is_competencia);
    RUN_TEST(test_valid_magic_enters_prueba_within_timeout);
    RUN_TEST(test_timeout_reverts_to_competencia);
    RUN_TEST(test_wrong_magic_never_enters_prueba);
    RUN_TEST(test_wrong_magic_does_not_refresh_existing_hold);
    RUN_TEST(test_refeed_sustains_prueba);
    RUN_TEST(test_wrap_safe_no_false_fresh);
    RUN_TEST(test_wrap_safe_now_less_than_last_is_not_huge_elapsed);
    RUN_TEST(test_adversarial_random_magics_never_enter_prueba);
    RUN_TEST(test_adversarial_garbage_cannot_prolong_hold);
    return UNITY_END();
}
