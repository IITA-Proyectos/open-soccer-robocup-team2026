// test_read_stamp — TDD del "ratchet de vida" por-sensor (FASE 2 de la pizarra TOP).
//
// Qué cubre (src/shared/read_stamp.h)
// ------------------------------------
// read_stamp_tick() devuelve el TIMESTAMP con el que estampar el slot de la pizarra
// ESTE publish, desacoplando la frescura del slot del ritmo del LOOP (que publica
// cada vuelta) del ritmo del SENSOR (que vive o no). Hoy snapshot_emitter_publish()
// estampa todo con `now` → la frescura refleja la cadencia del LOOP (cubre loop-muerto
// pero NO la muerte de UN sensor con el loop vivo). Con este ratchet, el stamp avanza
// SOLO si el sensor dio señal de vida → un sensor caído deja de refrescar su slot y
// éste envejece a su ritmo → sentinela POR-SENSOR.
//
// Foco FAIL-SAFE:
//   • vivo → stamp = now (FRESCO aguas abajo).
//   • muerto tras haber vivido → CONGELA el último stamp con vida (envejece solo).
//   • NUNCA vivo (arranque en frío) → stamp forzado-VENCIDO, incluso con now < umbral
//     (un stamp 0 daría falso-FRESCO en los primeros ms post-boot).
//   • wrap-safe (resta unsigned; el wrap de millis() no produce falso-fresco).
//
// Las 2 últimas pruebas son INTEGRACIÓN con sensor_slot.h: prueban end-to-end que el
// ratchet hace envejecer el slot por-sensor (lo que el status-quo `now` plano NO hace).
//
// Corre host (sin Arduino) con:
//   bash scripts/run-host-tests.sh test_read_stamp

#include <unity.h>
#include "read_stamp.h"
#include "sensor_slot.h"   // integración: slot_publish / slot_is_fresh

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// (1) VIVO: el ratchet avanza a `now`, devuelve `now`, marca ever_alive.
// ============================================================================
void test_alive_returns_now_and_marks_ever_alive(void) {
    ReadStampState st{};
    const uint32_t stamp = read_stamp_tick(st, /*alive_now=*/true, /*now=*/1000u);
    TEST_ASSERT_EQUAL_UINT32(1000u, stamp);
    TEST_ASSERT_EQUAL_UINT32(1000u, st.last_alive_ms);
    TEST_ASSERT_TRUE(st.ever_alive);
}

// ============================================================================
// (2) MUERTO en arranque en frío (nunca vivo) → stamp forzado-VENCIDO.
// Invariante exacto: now - stamp == READ_STAMP_STALE_OFFSET_MS. Estado intacto.
// ============================================================================
void test_dead_cold_start_is_forced_stale(void) {
    ReadStampState st{};
    const uint32_t now = 5000u;
    const uint32_t stamp = read_stamp_tick(st, /*alive_now=*/false, now);
    // Aguas abajo slot_is_fresh hace (now - stamp); que dé EXACTO el offset enorme.
    TEST_ASSERT_EQUAL_UINT32(READ_STAMP_STALE_OFFSET_MS, (uint32_t)(now - stamp));
    // Nunca vivo: el estado no se ensucia.
    TEST_ASSERT_FALSE(st.ever_alive);
    TEST_ASSERT_EQUAL_UINT32(0u, st.last_alive_ms);
}

// ============================================================================
// (3) MUERTO en frío con UPTIME CHICO (now=50): el stamp 0 ingenuo daría falso-
// FRESCO (50-0=50 < umbral 1000). El forzado-vencido lo evita. WRAP-SAFE.
// ============================================================================
void test_dead_cold_start_small_uptime_is_forced_stale(void) {
    ReadStampState st{};
    const uint32_t now = 50u;
    const uint32_t stamp = read_stamp_tick(st, false, now);
    TEST_ASSERT_EQUAL_UINT32(READ_STAMP_STALE_OFFSET_MS, (uint32_t)(now - stamp));
}

// ============================================================================
// (4) MUERTO TRAS HABER VIVIDO → congela el último stamp con vida (no avanza).
// ============================================================================
void test_dead_after_alive_holds_last_alive_stamp(void) {
    ReadStampState st{};
    read_stamp_tick(st, /*alive=*/true, /*now=*/1000u);          // vivo en 1000
    const uint32_t held = read_stamp_tick(st, /*alive=*/false, /*now=*/1050u);
    TEST_ASSERT_EQUAL_UINT32(1000u, held);                       // congelado en 1000
    TEST_ASSERT_EQUAL_UINT32(1000u, st.last_alive_ms);           // estado intacto
    TEST_ASSERT_TRUE(st.ever_alive);
}

// ============================================================================
// (5) RE-VIVO → re-avanza el ratchet al nuevo `now`.
// ============================================================================
void test_realive_readvances_stamp(void) {
    ReadStampState st{};
    read_stamp_tick(st, true,  1000u);     // vivo
    read_stamp_tick(st, false, 1050u);     // muerto (congela 1000)
    const uint32_t s2 = read_stamp_tick(st, true, 2000u);   // re-vivo
    TEST_ASSERT_EQUAL_UINT32(2000u, s2);
    TEST_ASSERT_EQUAL_UINT32(2000u, st.last_alive_ms);
}

// ============================================================================
// (6) WRAP: vivo justo en el borde del wrap (now=UINT32_MAX) devuelve now igual.
// ============================================================================
void test_alive_at_wrap_returns_now(void) {
    ReadStampState st{};
    const uint32_t stamp = read_stamp_tick(st, true, UINT32_MAX);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, stamp);
}

// ============================================================================
// (7) INTEGRACIÓN: un sensor que MUERE con el loop vivo hace ENVEJECER su slot
// al ritmo del SENSOR (no del loop). Es justo lo que el `now` plano NO logra.
// ============================================================================
void test_ratchet_ages_slot_out_per_sensor(void) {
    struct V { uint16_t x; };
    SensorSlot<V> slot{};
    ReadStampState st{};
    const uint32_t TH = 250u;

    // t=1000: sensor vivo → publica fresco.
    uint32_t s = read_stamp_tick(st, /*alive=*/true, 1000u);
    slot_publish(slot, V{42}, s);
    TEST_ASSERT_TRUE(slot_is_fresh(slot, 1000u, TH));

    // t=1100: sensor MUERTO, loop SIGUE publicando (republica el último valor).
    // El stamp se congela en 1000 → 1100-1000=100 <= 250 → todavía fresco.
    s = read_stamp_tick(st, /*alive=*/false, 1100u);
    slot_publish(slot, V{42}, s);
    TEST_ASSERT_TRUE(slot_is_fresh(slot, 1100u, TH));

    // t=1300: sigue muerto, loop sigue publicando. Stamp congelado en 1000 →
    // 1300-1000=300 > 250 → STALE. El slot envejeció al ritmo del SENSOR.
    s = read_stamp_tick(st, /*alive=*/false, 1300u);
    slot_publish(slot, V{42}, s);
    TEST_ASSERT_FALSE(slot_is_fresh(slot, 1300u, TH));

    // CONTRASTE (status quo): si hubiéramos estampado con `now` plano cada loop,
    // last_publish=1300 → 1300-1300=0 <= 250 → fresco PARA SIEMPRE (el bug que
    // Fase 2 cierra). Lo verificamos sobre un slot gemelo.
    SensorSlot<V> naive{};
    slot_publish(naive, V{42}, 1300u);   // `now` plano
    TEST_ASSERT_TRUE(slot_is_fresh(naive, 1300u, TH));   // nunca envejece
}

// ============================================================================
// (8) INTEGRACIÓN cold-start: un slot cuyo sensor NUNCA vivió queda STALE aunque
// el uptime sea menor al umbral (donde un stamp 0 daría falso-FRESCO).
// ============================================================================
void test_cold_start_slot_is_stale_even_below_threshold(void) {
    struct V { uint16_t x; };
    SensorSlot<V> slot{};
    ReadStampState st{};
    const uint32_t now = 50u;          // uptime 50 ms
    const uint32_t TH  = 1000u;        // umbral cámara

    const uint32_t s = read_stamp_tick(st, /*alive=*/false, now);
    slot_publish(slot, V{7}, s);
    // now - stamp == OFFSET (≫ TH) → STALE pese a uptime 50 < TH 1000.
    TEST_ASSERT_FALSE(slot_is_fresh(slot, now, TH));

    // CONTRASTE: con stamp 0 (last_alive zero-init ingenuo) sería falso-fresco.
    SensorSlot<V> naive{};
    slot_publish(naive, V{7}, 0u);
    TEST_ASSERT_TRUE(slot_is_fresh(naive, now, TH));   // 50-0=50 <= 1000 → fresco (mal)
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_alive_returns_now_and_marks_ever_alive);
    RUN_TEST(test_dead_cold_start_is_forced_stale);
    RUN_TEST(test_dead_cold_start_small_uptime_is_forced_stale);
    RUN_TEST(test_dead_after_alive_holds_last_alive_stamp);
    RUN_TEST(test_realive_readvances_stamp);
    RUN_TEST(test_alive_at_wrap_returns_now);
    RUN_TEST(test_ratchet_ages_slot_out_per_sensor);
    RUN_TEST(test_cold_start_slot_is_stale_even_below_threshold);
    return UNITY_END();
}
