// test_loop_monitor — supervisor de loop-time (audit 2026-06-05, R2 CENTRAL).
// Corre host con: bash scripts/run-host-tests.sh test_loop_monitor
//
// Cubre la lógica PURA que mide el tiempo de cada vuelta del loop:
//   • primer tick: solo siembra (primed), sin dt medible,
//   • dt normal: max_us y ema_us se actualizan correctamente,
//   • wrap de micros()/millis() (uint32 da la vuelta): el dt sale wrap-safe,
//   • max_us se queda con el PEOR dt (no lo olvida cuando bajan los siguientes),
//   • el EMA arranca en el primer dt real (no arrastra el 0 de la siembra),
//   • el EMA converge hacia un dt constante.

#include <unity.h>
#include "loop_monitor.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Estado inicial: .bss zero-init == no primed, sin historia.
void test_zero_init_not_primed(void) {
    LoopMonitor m{};
    TEST_ASSERT_FALSE(m.primed);
    TEST_ASSERT_EQUAL_UINT32(0, m.max_us);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, m.ema_us);
}

// El primer tick solo siembra: queda primed, sin medir un dt (no hay vuelta previa).
void test_first_tick_only_primes(void) {
    LoopMonitor m{};
    loop_monitor_update(m, 123456);
    TEST_ASSERT_TRUE(m.primed);
    TEST_ASSERT_EQUAL_UINT32(123456, m.last_us);
    // Sin dt medible todavía: max y ema siguen en 0.
    TEST_ASSERT_EQUAL_UINT32(0, m.max_us);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, m.ema_us);
}

// dt normal: la segunda llamada produce un dt = now - last y lo registra.
void test_normal_dt_updates_max_and_ema(void) {
    LoopMonitor m{};
    loop_monitor_update(m, 1000);     // siembra
    loop_monitor_update(m, 1200);     // dt = 200 us
    TEST_ASSERT_EQUAL_UINT32(200, m.max_us);
    // Primer dt real: el EMA arranca exactamente en el valor crudo.
    TEST_ASSERT_EQUAL_FLOAT(200.0f, m.ema_us);
    TEST_ASSERT_EQUAL_UINT32(1200, m.last_us);
}

// max_us se queda con el PEOR caso aunque las vueltas siguientes sean más rápidas.
void test_max_keeps_worst_case(void) {
    LoopMonitor m{};
    uint32_t now = 5000;
    loop_monitor_update(m, now);               // siembra
    now += 100;  loop_monitor_update(m, now);  // dt=100
    now += 900;  loop_monitor_update(m, now);  // dt=900  <- pico
    now += 50;   loop_monitor_update(m, now);  // dt=50
    now += 120;  loop_monitor_update(m, now);  // dt=120
    // El peor caso (900) NO se olvida pese a los dt chicos posteriores.
    TEST_ASSERT_EQUAL_UINT32(900, m.max_us);
}

// Wrap de micros()/millis(): now da la vuelta de cerca de 0xFFFFFFFF a un valor chico;
// la resta unsigned debe dar el dt REAL, no un número gigante.
void test_micros_wrap_is_handled(void) {
    LoopMonitor m{};
    const uint32_t before_wrap = 0xFFFFFF00u;   // ~256 us antes del overflow
    loop_monitor_update(m, before_wrap);        // siembra
    const uint32_t after_wrap = 0x00000064u;    // 100 us después de dar la vuelta
    loop_monitor_update(m, after_wrap);
    // dt real = (0x100 - 0x00) + 0x64 = 256 + 100 = 356 us (resta unsigned modular).
    const uint32_t expected = after_wrap - before_wrap;  // = 356 en aritmética uint32
    TEST_ASSERT_EQUAL_UINT32(356u, expected);            // sanity de la expectativa
    TEST_ASSERT_EQUAL_UINT32(356u, m.max_us);
    TEST_ASSERT_EQUAL_FLOAT(356.0f, m.ema_us);
}

// El wrap NO debe inflar el max con un dt espurio gigante (regresión clásica si se
// usara aritmética con signo o un if mal puesto).
void test_wrap_does_not_inflate_max(void) {
    LoopMonitor m{};
    uint32_t now = 1000;
    loop_monitor_update(m, now);               // siembra
    now += 250; loop_monitor_update(m, now);   // dt=250 (max real)
    // Ahora forzamos un cruce de wrap con un dt CHICO real.
    loop_monitor_update(m, 0xFFFFFFF0u);       // dt grande PERO legítimo desde 1250
    const uint32_t big_legit = 0xFFFFFFF0u - now;
    TEST_ASSERT_EQUAL_UINT32(big_legit, m.max_us);  // ese sí es el peor real
    // Cruce de wrap con dt chico: no debe superar a big_legit.
    loop_monitor_update(m, 0x00000010u);       // dt = 0x20 = 32 us
    TEST_ASSERT_EQUAL_UINT32(big_legit, m.max_us);  // el wrap no metió un pico falso
}

// El EMA converge hacia un dt constante tras muchas vueltas iguales.
void test_ema_converges_to_constant_dt(void) {
    LoopMonitor m{};
    uint32_t now = 0;
    loop_monitor_update(m, now);   // siembra
    // Muchas vueltas de exactamente 100 us cada una.
    for (int i = 0; i < 500; ++i) {
        now += 100;
        loop_monitor_update(m, now);
    }
    // Con dt constante, el EMA debe quedar pegado al valor (tolerancia chica).
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 100.0f, m.ema_us);
    TEST_ASSERT_EQUAL_UINT32(100, m.max_us);
}

// El EMA NO arranca arrastrando el 0 de la siembra: tras el primer dt real, un
// segundo dt distinto mueve el promedio entre ambos, no entre 0 y el dt.
void test_ema_does_not_drag_seed_zero(void) {
    LoopMonitor m{};
    loop_monitor_update(m, 0);       // siembra
    loop_monitor_update(m, 1000);    // dt=1000 -> ema=1000 exacto (primer real)
    TEST_ASSERT_EQUAL_FLOAT(1000.0f, m.ema_us);
    loop_monitor_update(m, 1100);    // dt=100 -> ema = 1000 + 0.05*(100-1000) = 955
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 955.0f, m.ema_us);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_init_not_primed);
    RUN_TEST(test_first_tick_only_primes);
    RUN_TEST(test_normal_dt_updates_max_and_ema);
    RUN_TEST(test_max_keeps_worst_case);
    RUN_TEST(test_micros_wrap_is_handled);
    RUN_TEST(test_wrap_does_not_inflate_max);
    RUN_TEST(test_ema_converges_to_constant_dt);
    RUN_TEST(test_ema_does_not_drag_seed_zero);
    return UNITY_END();
}
