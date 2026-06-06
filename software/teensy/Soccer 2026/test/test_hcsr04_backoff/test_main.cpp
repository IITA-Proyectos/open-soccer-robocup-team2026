// test_hcsr04_backoff — backoff de muestreo del HC-SR04 (audit 2026-06-05, MEDIUM).
// Corre host con: bash scripts/run-host-tests.sh test_hcsr04_backoff
//
// Cubre la lógica PURA que decide CUÁNDO disparar el pulseIn() bloqueante del
// HC-SR04 (read_hcsr04 en src/top/sensors_tof.cpp): cuando el sensor flota/no ve
// eco, en vez de robar 12 ms cada ~90 ms para confirmar "sigue sin nada",
// espacia el muestreo a ~cada 900 ms. Verifica:
//   • lectura NORMAL: con eco siempre samplea al ritmo normal (cada N ticks),
//   • N disparos sin eco => entra en backoff (samplea mucho más espaciado),
//   • vuelve un eco => resetea el backoff (retoma el ritmo rápido),
//   • bordes: primer disparo siembra, racha satura, reset limpia, wrap del tick.

#include <unity.h>
#include "hcsr04_backoff.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Estado inicial: .bss zero-init == no primed, sin backoff.
void test_zero_init(void) {
    Hcsr04Backoff s{};
    TEST_ASSERT_FALSE(s.primed);
    TEST_ASSERT_EQUAL_UINT16(0, s.misses);
    Hcsr04BackoffCfg cfg = hcsr04_backoff_default_cfg();
    TEST_ASSERT_FALSE(hcsr04_backoff_active(s, cfg));
    // Primer should_fire dispara y siembra.
    TEST_ASSERT_TRUE(hcsr04_backoff_should_fire(s, 1000, cfg));
    TEST_ASSERT_TRUE(s.primed);
    TEST_ASSERT_EQUAL_UINT32(1000, s.last_fire_tick);
}

// Lectura NORMAL: viendo eco siempre, samplea cada normal_period_ticks.
void test_normal_period_with_echo(void) {
    Hcsr04Backoff s{};
    Hcsr04BackoffCfg cfg = hcsr04_backoff_default_cfg();
    uint32_t tick = 0;

    // Primer tick dispara (siembra).
    TEST_ASSERT_TRUE(hcsr04_backoff_should_fire(s, tick, cfg));
    hcsr04_backoff_note_result(s, tick, /*got_echo=*/true);

    int fires = 0;
    for (tick = 1; tick <= 30; ++tick) {
        if (hcsr04_backoff_should_fire(s, tick, cfg)) {
            hcsr04_backoff_note_result(s, tick, /*got_echo=*/true);
            ++fires;
        }
    }
    // 30 ticks a ritmo normal (cada 3) => ~10 disparos. Nunca entra en backoff
    // porque cada disparo ve eco.
    TEST_ASSERT_FALSE(hcsr04_backoff_active(s, cfg));
    TEST_ASSERT_EQUAL_INT(10, fires);
}

// No dispara ANTES de cumplir el periodo normal.
void test_normal_period_spacing(void) {
    Hcsr04Backoff s{};
    Hcsr04BackoffCfg cfg = hcsr04_backoff_default_cfg();  // normal=3
    TEST_ASSERT_TRUE(hcsr04_backoff_should_fire(s, 0, cfg));   // siembra en tick 0
    hcsr04_backoff_note_result(s, 0, true);
    TEST_ASSERT_FALSE(hcsr04_backoff_should_fire(s, 1, cfg));  // dt=1 < 3
    TEST_ASSERT_FALSE(hcsr04_backoff_should_fire(s, 2, cfg));  // dt=2 < 3
    TEST_ASSERT_TRUE(hcsr04_backoff_should_fire(s, 3, cfg));   // dt=3 >= 3
}

// N disparos consecutivos sin eco => entra en backoff.
void test_enters_backoff_after_n_misses(void) {
    Hcsr04Backoff s{};
    Hcsr04BackoffCfg cfg = hcsr04_backoff_default_cfg();  // enter=3, normal=3
    uint32_t tick = 0;

    // Disparar al ritmo normal pero SIN eco cada vez.
    for (int i = 0; i < (int)cfg.enter_misses; ++i) {
        TEST_ASSERT_TRUE(hcsr04_backoff_should_fire(s, tick, cfg));
        hcsr04_backoff_note_result(s, tick, /*got_echo=*/false);
        TEST_ASSERT_EQUAL_UINT16(i + 1, s.misses);
        tick += cfg.normal_period_ticks;
    }
    TEST_ASSERT_TRUE(hcsr04_backoff_active(s, cfg));
}

// Una vez en backoff, el muestreo se ESPACIA al periodo de backoff: no dispara a
// los normal_period_ticks, sí a los backoff_period_ticks.
void test_backoff_period_spacing(void) {
    Hcsr04Backoff s{};
    Hcsr04BackoffCfg cfg = hcsr04_backoff_default_cfg();  // normal=3, backoff=30, enter=3
    uint32_t tick = 0;

    // Llevarlo a backoff con enter_misses no-ecos al ritmo normal.
    for (int i = 0; i < (int)cfg.enter_misses; ++i) {
        hcsr04_backoff_should_fire(s, tick, cfg);
        hcsr04_backoff_note_result(s, tick, false);
        tick += cfg.normal_period_ticks;
    }
    TEST_ASSERT_TRUE(hcsr04_backoff_active(s, cfg));

    // El último disparo fue en (tick - normal). Ahora NO debe disparar al pasar
    // solo normal_period_ticks: ya está en backoff y exige backoff_period_ticks.
    const uint32_t last = s.last_fire_tick;
    TEST_ASSERT_FALSE(hcsr04_backoff_should_fire(s, last + cfg.normal_period_ticks, cfg));
    TEST_ASSERT_FALSE(hcsr04_backoff_should_fire(s, last + cfg.backoff_period_ticks - 1, cfg));
    // Recién al cumplir backoff_period_ticks dispara.
    TEST_ASSERT_TRUE(hcsr04_backoff_should_fire(s, last + cfg.backoff_period_ticks, cfg));
}

// Vuelve un eco => resetea el backoff (sale, retoma ritmo rápido).
void test_echo_resets_backoff(void) {
    Hcsr04Backoff s{};
    Hcsr04BackoffCfg cfg = hcsr04_backoff_default_cfg();
    uint32_t tick = 0;

    // Entrar en backoff.
    for (int i = 0; i < (int)cfg.enter_misses; ++i) {
        hcsr04_backoff_should_fire(s, tick, cfg);
        hcsr04_backoff_note_result(s, tick, false);
        tick += cfg.normal_period_ticks;
    }
    TEST_ASSERT_TRUE(hcsr04_backoff_active(s, cfg));

    // Esperar el periodo de backoff, disparar y AHORA ver eco.
    uint32_t fire_tick = s.last_fire_tick + cfg.backoff_period_ticks;
    TEST_ASSERT_TRUE(hcsr04_backoff_should_fire(s, fire_tick, cfg));
    hcsr04_backoff_note_result(s, fire_tick, /*got_echo=*/true);

    // El eco reseteó la racha: ya NO está en backoff.
    TEST_ASSERT_FALSE(hcsr04_backoff_active(s, cfg));
    TEST_ASSERT_EQUAL_UINT16(0, s.misses);
    // Y vuelve al ritmo NORMAL: dispara a los normal_period_ticks (no espera backoff).
    TEST_ASSERT_TRUE(hcsr04_backoff_should_fire(s, fire_tick + cfg.normal_period_ticks, cfg));
}

// Un solo no-eco aislado (< enter_misses) NO degrada el ritmo: sigue normal.
void test_single_miss_does_not_backoff(void) {
    Hcsr04Backoff s{};
    Hcsr04BackoffCfg cfg = hcsr04_backoff_default_cfg();  // enter=3
    hcsr04_backoff_should_fire(s, 0, cfg);
    hcsr04_backoff_note_result(s, 0, /*got_echo=*/false);   // miss 1
    hcsr04_backoff_should_fire(s, 3, cfg);
    hcsr04_backoff_note_result(s, 3, /*got_echo=*/true);    // eco: resetea
    hcsr04_backoff_should_fire(s, 6, cfg);
    hcsr04_backoff_note_result(s, 6, /*got_echo=*/false);   // miss 1 de nuevo
    TEST_ASSERT_FALSE(hcsr04_backoff_active(s, cfg));        // nunca llegó a 3 seguidos
}

// La racha de misses SATURA (no wrappea) en una sequía muy larga sin eco. Es el
// contador de note_result() el que incrementa, así que lo ejercemos directo más
// de 65535 veces (por should_fire solo se dispararía cada backoff_period_ticks,
// que en 2^16 lecturas tardaría una eternidad — la saturación es la red de
// seguridad ante esa eternidad, igual que el streak de imu_freeze).
void test_misses_saturate_no_wrap(void) {
    Hcsr04Backoff s{};
    uint32_t tick = 0;
    for (long i = 0; i < 70000; ++i) {     // > 0xFFFF iteraciones
        hcsr04_backoff_note_result(s, tick, /*got_echo=*/false);
        ++tick;
    }
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, s.misses);   // saturó, NO volvió a 0
    Hcsr04BackoffCfg cfg = hcsr04_backoff_default_cfg();
    TEST_ASSERT_TRUE(hcsr04_backoff_active(s, cfg));
}

// reset() limpia el estado.
void test_reset_clears_state(void) {
    Hcsr04Backoff s{};
    Hcsr04BackoffCfg cfg = hcsr04_backoff_default_cfg();
    uint32_t tick = 0;
    for (int i = 0; i < (int)cfg.enter_misses; ++i) {
        hcsr04_backoff_should_fire(s, tick, cfg);
        hcsr04_backoff_note_result(s, tick, false);
        tick += cfg.normal_period_ticks;
    }
    TEST_ASSERT_TRUE(hcsr04_backoff_active(s, cfg));
    hcsr04_backoff_reset(s);
    TEST_ASSERT_FALSE(s.primed);
    TEST_ASSERT_EQUAL_UINT16(0, s.misses);
    TEST_ASSERT_EQUAL_UINT32(0, s.last_fire_tick);
    // Tras reset, el primer should_fire vuelve a sembrar y disparar.
    TEST_ASSERT_TRUE(hcsr04_backoff_should_fire(s, 12345, cfg));
}

// WRAP del contador de ticks: should_fire usa resta unsigned. Sembramos cerca
// del overflow de uint32 y cruzamos a 0; el periodo debe medirse correcto (no
// disparar ANTES de tiempo por un dt "gigante" mal calculado).
void test_tick_wrap_safe(void) {
    Hcsr04Backoff s{};
    Hcsr04BackoffCfg cfg = hcsr04_backoff_default_cfg();  // normal=3
    uint32_t tick = 0xFFFFFFFFu - 1u;   // dos ticks antes del overflow

    TEST_ASSERT_TRUE(hcsr04_backoff_should_fire(s, tick, cfg));   // siembra en 0xFFFFFFFE
    hcsr04_backoff_note_result(s, tick, true);

    // +1 => 0xFFFFFFFF: dt=1 < 3, no dispara.
    TEST_ASSERT_FALSE(hcsr04_backoff_should_fire(s, tick + 1, cfg));
    // +2 => 0x00000000 (wrap): dt unsigned = 2 < 3, sigue sin disparar (NO explota).
    TEST_ASSERT_FALSE(hcsr04_backoff_should_fire(s, tick + 2, cfg));
    // +3 => 0x00000001: dt unsigned = 3 >= 3, dispara. Demuestra resta wrap-safe.
    TEST_ASSERT_TRUE(hcsr04_backoff_should_fire(s, tick + 3, cfg));
}

// Config custom: respeta los periodos/umbrales pasados (no hardcodea defaults).
void test_custom_cfg_respected(void) {
    Hcsr04Backoff s{};
    Hcsr04BackoffCfg cfg;
    cfg.normal_period_ticks  = 5;
    cfg.backoff_period_ticks = 50;
    cfg.enter_misses         = 2;
    uint32_t tick = 0;

    hcsr04_backoff_should_fire(s, tick, cfg);          // siembra
    hcsr04_backoff_note_result(s, tick, false);        // miss 1
    TEST_ASSERT_FALSE(hcsr04_backoff_active(s, cfg));
    tick += 5;
    hcsr04_backoff_should_fire(s, tick, cfg);
    hcsr04_backoff_note_result(s, tick, false);        // miss 2 => backoff (enter=2)
    TEST_ASSERT_TRUE(hcsr04_backoff_active(s, cfg));
    // Ahora exige 50 ticks, no 5.
    TEST_ASSERT_FALSE(hcsr04_backoff_should_fire(s, tick + 5, cfg));
    TEST_ASSERT_TRUE(hcsr04_backoff_should_fire(s, tick + 50, cfg));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_init);
    RUN_TEST(test_normal_period_with_echo);
    RUN_TEST(test_normal_period_spacing);
    RUN_TEST(test_enters_backoff_after_n_misses);
    RUN_TEST(test_backoff_period_spacing);
    RUN_TEST(test_echo_resets_backoff);
    RUN_TEST(test_single_miss_does_not_backoff);
    RUN_TEST(test_misses_saturate_no_wrap);
    RUN_TEST(test_reset_clears_state);
    RUN_TEST(test_tick_wrap_safe);
    RUN_TEST(test_custom_cfg_respected);
    return UNITY_END();
}
