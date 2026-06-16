// test_bno_read_sm — TDD del SCHEDULER PURO de la lectura del BNO secundario.
//
// Cubre src/shared/bno_read_sm.h: el SM decide CUÁNDO el caller puede leer el BNO
// del bus compartido (Wire con los ToF), heredando del banco dos reglas:
//   • CADENCIA  — no más seguido que min_period_ms (50 default / 10 fast).
//   • DECONFLICT— no si hace < tof_gap_ms (8) que se leyó un ToF en el bus.
// Y el FAIL-SAFE: nunca fuerza, cfg peligrosa (min_period<50 sin primary_only)
// → cfg_valid=false → may_read=false. Restas unsigned wrap-safe.
//
// Corre host (sin Arduino) con:
//   bash scripts/run-host-tests.sh test_bno_read_sm

#include <unity.h>
#include "bno_read_sm.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Helper: un BnoSched ya "rodado" (ever_read=true) con last_bno y last_tof dados.
static BnoSched sched_at(uint32_t last_bno, uint32_t last_tof) {
    BnoSched sm{};
    sm.last_bno_read_ms = last_bno;
    sm.last_tof_read_ms = last_tof;
    sm.ever_read        = true;
    return sm;
}

// ============================================================================
// (A) DEFAULTS / FAST cfg — los valores del contrato
// ============================================================================

void test_default_cfg_values(void) {
    const BnoSchedCfg c = bno_sched_default_cfg();
    TEST_ASSERT_EQUAL_UINT32(50u, c.min_period_ms);
    TEST_ASSERT_EQUAL_UINT32(8u,  c.tof_gap_ms);
    TEST_ASSERT_FALSE(c.primary_only);
}

void test_fast_cfg_values(void) {
    const BnoSchedCfg c = bno_sched_fast_cfg();
    TEST_ASSERT_EQUAL_UINT32(10u, c.min_period_ms);
    TEST_ASSERT_EQUAL_UINT32(8u,  c.tof_gap_ms);
    TEST_ASSERT_TRUE(c.primary_only);
}

// ============================================================================
// (B) cfg_valid — atrapa el caso que RECONGELA el yaw
//     {10,8,false} -> false ; {10,8,true} -> true ; {50,8,false} -> true
// ============================================================================

void test_cfg_valid_fast_without_primary_only_is_invalid(void) {
    // min_period < 50 SIN primary_only = leer rápido el secundario del bus
    // compartido = el caso del freeze. DEBE rechazarse.
    BnoSchedCfg c{10u, 8u, false};
    TEST_ASSERT_FALSE(bno_sched_cfg_valid(c));
}

void test_cfg_valid_fast_with_primary_only_is_valid(void) {
    // min_period < 50 PERO primary_only = primario aislado en Wire2, sin ToF.
    // Leer rápido es seguro. VÁLIDA.
    BnoSchedCfg c{10u, 8u, true};
    TEST_ASSERT_TRUE(bno_sched_cfg_valid(c));
}

void test_cfg_valid_default_is_valid(void) {
    BnoSchedCfg c{50u, 8u, false};
    TEST_ASSERT_TRUE(bno_sched_cfg_valid(c));
}

void test_cfg_valid_builtin_defaults_are_valid(void) {
    TEST_ASSERT_TRUE(bno_sched_cfg_valid(bno_sched_default_cfg()));
    TEST_ASSERT_TRUE(bno_sched_cfg_valid(bno_sched_fast_cfg()));
}

// Borde: exactamente 50 ms SIN primary_only sigue válido (el corte es < 50).
void test_cfg_valid_exactly_50_without_primary_only_is_valid(void) {
    BnoSchedCfg c{50u, 8u, false};
    TEST_ASSERT_TRUE(bno_sched_cfg_valid(c));
}
// Borde: 49 ms SIN primary_only es inválido (< 50).
void test_cfg_valid_49_without_primary_only_is_invalid(void) {
    BnoSchedCfg c{49u, 8u, false};
    TEST_ASSERT_FALSE(bno_sched_cfg_valid(c));
}

// FAIL-SAFE: may_read NUNCA autoriza con cfg inválida, aunque cadencia y gap estén
// holgadísimos (never-ok / default-to-safe).
void test_may_read_false_when_cfg_invalid(void) {
    BnoSchedCfg bad{10u, 8u, false};   // inválida
    BnoSched sm = sched_at(0u, 0u);    // last_bno=last_tof=0
    // now muy lejos: cadencia y gap re-cumplidos de sobra, pero cfg manda.
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 100000u, bad));
}

// ============================================================================
// (C) DECONFLICT — tras note_tof_read(t), may_read=false hasta t+gap,
//     AUNQUE la cadencia de 50 ms ya esté cumplida.
// ============================================================================

void test_deconflict_blocks_until_gap_even_if_period_ok(void) {
    const BnoSchedCfg c = bno_sched_default_cfg();   // {50,8,false}
    // BNO leído hace MUCHO (period 50 sobradamente cumplido), ToF leído en t=1000.
    BnoSched sm = sched_at(/*last_bno=*/0u, /*last_tof=*/1000u);

    // t=1000..1007: dentro del gap de 8 ms vs ToF → NO autoriza pese a period OK.
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 1000u, c));  // since_tof=0
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 1004u, c));  // since_tof=4
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 1007u, c));  // since_tof=7 (<8)

    // t=1008: since_tof=8 == gap → corte EXACTO autoriza.
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 1008u, c));
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 1100u, c));   // bien después
}

void test_note_tof_read_rearms_the_gap(void) {
    const BnoSchedCfg c = bno_sched_default_cfg();
    BnoSched sm = sched_at(/*last_bno=*/0u, /*last_tof=*/0u);
    // Sin ToF reciente y period cumplido → autoriza.
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 2000u, c));
    // Llega un ToF a las 2000 → re-arma el gap.
    bno_sched_note_tof_read(sm, 2000u);
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 2003u, c));  // dentro del gap
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 2008u, c));   // gap cumplido
}

// ============================================================================
// (D) CADENCIA — con el gap OK, respeta 50 (default) / 10 (fast)
// ============================================================================

void test_cadence_default_50ms(void) {
    const BnoSchedCfg c = bno_sched_default_cfg();   // period 50
    // BNO leído en t=1000, ToF viejo (gap no estorba).
    BnoSched sm = sched_at(/*last_bno=*/1000u, /*last_tof=*/0u);

    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 1000u, c));  // since_bno=0
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 1049u, c));  // 49 < 50
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 1050u, c));   // 50 == period → corte exacto
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 1051u, c));
}

void test_cadence_fast_10ms(void) {
    const BnoSchedCfg c = bno_sched_fast_cfg();   // period 10, primary_only=true → válida
    BnoSched sm = sched_at(/*last_bno=*/1000u, /*last_tof=*/0u);

    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 1000u, c));  // since_bno=0
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 1009u, c));  // 9 < 10
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 1010u, c));   // 10 == period
}

void test_note_bno_read_advances_cadence(void) {
    const BnoSchedCfg c = bno_sched_default_cfg();
    BnoSched sm = sched_at(/*last_bno=*/1000u, /*last_tof=*/0u);
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 1060u, c));   // autoriza
    bno_sched_note_bno_read(sm, 1060u);                   // el caller leyó
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 1060u, c));  // recién leído, no de nuevo
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 1109u, c));  // 49 < 50
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 1110u, c));   // 50 cumplido
}

// ============================================================================
// (E) BOOT — ever_read=false: la cadencia NO bloquea; solo manda el gap de ToF
// ============================================================================

void test_boot_zero_init_can_read_when_bus_free(void) {
    const BnoSchedCfg c = bno_sched_default_cfg();
    BnoSched sm{};   // zero-init: ever_read=false, last_*=0
    // En el boot, una vez cumplido el gap de ToF contra t=0, autoriza aunque "now"
    // sea chico: la CADENCIA no aplica (no hubo lectura previa) — no espera 50 ms
    // falsos desde t=0, solo manda el gap del bus.
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 8u, c));   // since_tof=8 >= gap; cadencia no aplica
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 30u, c));  // mucho menos que 50, igual autoriza
}
// En el boot el gap SÍ aplica contra last_tof=0 (no hay lectura de BNO previa que medir).
void test_boot_gap_still_applies_against_initial_tof(void) {
    const BnoSchedCfg c = bno_sched_default_cfg();
    BnoSched sm{};   // last_tof=0
    // now=5 → since_tof=5 < 8 → bloquea por el gap (no por la cadencia).
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 5u, c));
    // now=8 → since_tof=8 → autoriza (gap cumplido; cadencia no aplica en boot).
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 8u, c));
}

void test_boot_first_read_then_cadence_kicks_in(void) {
    const BnoSchedCfg c = bno_sched_default_cfg();
    BnoSched sm{};
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 20u, c));   // boot: lee
    bno_sched_note_bno_read(sm, 20u);                   // primera lectura
    // Ahora ever_read=true → la cadencia de 50 ms manda.
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 60u, c));  // 40 < 50
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 70u, c));   // 50 cumplido
}

// ============================================================================
// (F) INTERCALADO ToF-BNO-ToF — el segundo ToF re-arma el gap del BNO
// ============================================================================

void test_interleaved_tof_bno_tof_rearms_gap(void) {
    const BnoSchedCfg c = bno_sched_default_cfg();
    BnoSched sm{};

    // t=0: ToF leído.
    bno_sched_note_tof_read(sm, 0u);
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 4u, c));   // dentro del gap del 1er ToF

    // t=10: gap cumplido + cadencia no aplica (boot) → BNO lee.
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 10u, c));
    bno_sched_note_bno_read(sm, 10u);

    // t=12: llega OTRO ToF → re-arma el gap (aunque la cadencia esté por verse).
    bno_sched_note_tof_read(sm, 12u);
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 15u, c));  // dentro del gap del 2do ToF (3<8)

    // t=20: gap del 2do ToF cumplido (8), PERO la cadencia (50 desde t=10) NO →
    // sigue bloqueado por el period, no por el gap.
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 20u, c));  // since_tof=8 ok, since_bno=10<50
    // t=60: ambas condiciones cumplidas (since_bno=50, since_tof=48) → autoriza.
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 60u, c));
}

// ============================================================================
// (G) WRAP de millis() — restas unsigned, no dispara falsos cortes
// ============================================================================

void test_wrap_deconflict_across_millis_rollover(void) {
    const BnoSchedCfg c = bno_sched_default_cfg();
    const uint32_t NEAR_MAX = 0xFFFFFFFEu;   // -2
    // ToF leído en NEAR_MAX, BNO viejo (cadencia OK). now envuelve a +5.
    BnoSched sm = sched_at(/*last_bno=*/0xF0000000u, /*last_tof=*/NEAR_MAX);

    // now = NEAR_MAX + 3 = 0x00000001 (wrap). since_tof = 1 - (-2) = 3 (unsigned) < 8.
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 1u, c));     // gap NO cumplido a través del wrap
    // now = NEAR_MAX + 10 = 8. since_tof = 8 - (-2) = 10 >= 8 → autoriza.
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 8u, c));
}

void test_wrap_cadence_across_millis_rollover(void) {
    const BnoSchedCfg c = bno_sched_default_cfg();
    const uint32_t NEAR_MAX = 0xFFFFFFE0u;   // -32
    // BNO leído en NEAR_MAX, ToF viejo respecto al wrap (gap no estorba).
    // last_tof lo ponemos lejos hacia atrás para que since_tox sea grande siempre.
    BnoSched sm = sched_at(/*last_bno=*/NEAR_MAX, /*last_tof=*/0xFFFF0000u);

    // now = NEAR_MAX + 30 = 14 (wrap). since_bno = 14 - (-32) = 30 (<50) → bloquea.
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 14u, c));
    // now = NEAR_MAX + 50 = 34. since_bno = 50 == period → autoriza a través del wrap.
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 34u, c));
}

// ============================================================================
// (H) AMBAS condiciones juntas — necesita period Y gap a la vez
// ============================================================================

void test_both_conditions_required(void) {
    const BnoSchedCfg c = bno_sched_default_cfg();
    // BNO leído en 1000, ToF leído en 1045.
    BnoSched sm = sched_at(/*last_bno=*/1000u, /*last_tof=*/1045u);

    // t=1050: since_bno=50 (period OK) PERO since_tof=5 (<8, gap NO) → bloquea.
    TEST_ASSERT_FALSE(bno_sched_may_read(sm, 1050u, c));
    // t=1053: since_tof=8 (gap OK) y since_bno=53 (period OK) → autoriza.
    TEST_ASSERT_TRUE(bno_sched_may_read(sm, 1053u, c));
}

int main(int, char**) {
    UNITY_BEGIN();
    // (A) defaults
    RUN_TEST(test_default_cfg_values);
    RUN_TEST(test_fast_cfg_values);
    // (B) cfg_valid + never-ok
    RUN_TEST(test_cfg_valid_fast_without_primary_only_is_invalid);
    RUN_TEST(test_cfg_valid_fast_with_primary_only_is_valid);
    RUN_TEST(test_cfg_valid_default_is_valid);
    RUN_TEST(test_cfg_valid_builtin_defaults_are_valid);
    RUN_TEST(test_cfg_valid_exactly_50_without_primary_only_is_valid);
    RUN_TEST(test_cfg_valid_49_without_primary_only_is_invalid);
    RUN_TEST(test_may_read_false_when_cfg_invalid);
    // (C) deconflict
    RUN_TEST(test_deconflict_blocks_until_gap_even_if_period_ok);
    RUN_TEST(test_note_tof_read_rearms_the_gap);
    // (D) cadencia
    RUN_TEST(test_cadence_default_50ms);
    RUN_TEST(test_cadence_fast_10ms);
    RUN_TEST(test_note_bno_read_advances_cadence);
    // (E) boot
    RUN_TEST(test_boot_zero_init_can_read_when_bus_free);
    RUN_TEST(test_boot_gap_still_applies_against_initial_tof);
    RUN_TEST(test_boot_first_read_then_cadence_kicks_in);
    // (F) intercalado
    RUN_TEST(test_interleaved_tof_bno_tof_rearms_gap);
    // (G) wrap
    RUN_TEST(test_wrap_deconflict_across_millis_rollover);
    RUN_TEST(test_wrap_cadence_across_millis_rollover);
    // (H) ambas
    RUN_TEST(test_both_conditions_required);
    return UNITY_END();
}
