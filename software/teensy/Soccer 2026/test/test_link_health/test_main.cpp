// test_link_health — lógica PURA de los enlaces inter-placa del TOP (Track B).
// Corre host con: bash scripts/run-host-tests.sh test_link_health
//
// Cubre 2 piezas puras extraídas a comm_arbiter.h (sin Arduino):
//
//   1) RefereeDebounce / referee_debounce_update (P1-ARB-DEBOUNCE)
//      Debounce por ESTABILIDAD del nivel GPIO del árbitro: un glitch de EMI de
//      motores NO debe cambiar el estado oficial; un cambio sostenido N ms sí.
//      Fail-safe: arranca y vuelve a STOP. (Mueve en STOP = penalizable.)
//
//   2) LinkSeqTracker / link_seq_track (P1-SEQ-LINK-HEALTH)
//      Consumo del SEQ del protocolo en RX + conteo de gap por enlace:
//      consecutivo→0, salto→+gap, wrap 255→0, primer frame siembra sin contar.
//
// El header solo incluye <stdint.h> + types.h (Arduino-free) → compila host. Se
// incluye por ruta relativa porque run-host-tests.sh no agrega -I src/top.

#include <unity.h>
#include "../../src/top/comm_arbiter.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ============================ DEBOUNCE ÁRBITRO ================================

// Estado inicial (.bss zero-init): oficial = STOP (fail-safe), sin sembrar.
void test_debounce_boot_is_stop(void) {
    RefereeDebounce d{};
    TEST_ASSERT_FALSE(d.official);
    TEST_ASSERT_FALSE(d.seeded);
}

// Primer update SIEMBRA la base temporal sin forzar cambio: aunque el crudo llegue
// HIGH en el primer tick, el oficial sigue STOP (no hay historia de estabilidad).
void test_debounce_seed_does_not_jump(void) {
    RefereeDebounce d{};
    TEST_ASSERT_FALSE(referee_debounce_update(d, /*raw=*/true, /*now=*/0));
    TEST_ASSERT_TRUE(d.seeded);
    TEST_ASSERT_FALSE(d.official);   // sigue STOP en el seed
}

// Glitch de 1 tick (HIGH aislado) NO cambia el estado: vuelve a LOW antes de N ms.
void test_debounce_single_tick_glitch_ignored(void) {
    RefereeDebounce d{};
    referee_debounce_update(d, false, 0);          // seed en STOP
    // Glitch espurio a HIGH en t=100, pero a t=105 ya volvió a LOW (5 ms << 40 ms).
    TEST_ASSERT_FALSE(referee_debounce_update(d, true,  100));
    TEST_ASSERT_FALSE(referee_debounce_update(d, false, 105));
    TEST_ASSERT_FALSE(d.official);                 // nunca arrancó: glitch ignorado
}

// Una ráfaga de glitches cortos (cada uno < ventana) tampoco arranca el match.
void test_debounce_burst_of_glitches_ignored(void) {
    RefereeDebounce d{};
    referee_debounce_update(d, false, 0);
    uint32_t t = 10;
    for (int i = 0; i < 20; ++i) {
        referee_debounce_update(d, true,  t);      // HIGH espurio
        t += 5;
        referee_debounce_update(d, false, t);      // vuelve a LOW antes de 40 ms
        t += 5;
    }
    TEST_ASSERT_FALSE(d.official);                  // 0 transiciones espurias
}

// Transición ESTABLE (HIGH sostenido >= ventana) sí cambia el estado oficial.
void test_debounce_stable_high_starts(void) {
    RefereeDebounce d{};
    referee_debounce_update(d, false, 0);          // seed STOP
    TEST_ASSERT_FALSE(referee_debounce_update(d, true, 100));   // candidato HIGH @100
    TEST_ASSERT_FALSE(referee_debounce_update(d, true, 130));   // 30 ms < 40 → aún STOP
    TEST_ASSERT_TRUE (referee_debounce_update(d, true, 140));   // 40 ms >= 40 → START
    TEST_ASSERT_TRUE(d.official);
}

// Justo en el borde de la ventana: < debounce_ms no cambia, == sí.
void test_debounce_window_boundary(void) {
    RefereeDebounce d{};
    referee_debounce_update(d, false, 1000);
    referee_debounce_update(d, true, 1000);                    // candidato @1000
    TEST_ASSERT_FALSE(referee_debounce_update(d, true, 1039)); // 39 ms < 40 → STOP
    TEST_ASSERT_TRUE (referee_debounce_update(d, true, 1040)); // exacto 40 ms → START
}

// Vuelta a STOP: una vez en START, un LOW sostenido >= ventana baja a STOP.
void test_debounce_stable_low_returns_to_stop(void) {
    RefereeDebounce d{};
    referee_debounce_update(d, false, 0);
    referee_debounce_update(d, true, 100);
    referee_debounce_update(d, true, 200);                     // ya en START
    TEST_ASSERT_TRUE(d.official);
    // STOP sostenido:
    TEST_ASSERT_TRUE (referee_debounce_update(d, false, 300)); // candidato LOW @300
    TEST_ASSERT_TRUE (referee_debounce_update(d, false, 320)); // 20 ms < 40 → sigue START
    TEST_ASSERT_FALSE(referee_debounce_update(d, false, 340)); // 40 ms >= 40 → STOP
    TEST_ASSERT_FALSE(d.official);
}

// Glitch a LOW durante el juego (START) NO para el match si es corto.
void test_debounce_glitch_low_during_play_ignored(void) {
    RefereeDebounce d{};
    referee_debounce_update(d, false, 0);
    referee_debounce_update(d, true, 100);
    referee_debounce_update(d, true, 200);                     // START
    // Glitch LOW de 5 ms a mitad de jugada:
    TEST_ASSERT_TRUE(referee_debounce_update(d, false, 500));
    TEST_ASSERT_TRUE(referee_debounce_update(d, true,  505));
    TEST_ASSERT_TRUE(d.official);                              // siguió jugando
}

// El cronómetro se REINICIA si el candidato cambia: HIGH-glitch-HIGH no acumula.
void test_debounce_candidate_change_resets_timer(void) {
    RefereeDebounce d{};
    referee_debounce_update(d, false, 0);
    referee_debounce_update(d, true,  100);   // candidato HIGH @100
    referee_debounce_update(d, false, 120);   // crudo vuelve a LOW (==oficial) → resync
    referee_debounce_update(d, true,  130);   // candidato HIGH otra vez @130 (cronómetro reinicia)
    TEST_ASSERT_FALSE(referee_debounce_update(d, true, 160)); // 30 ms desde 130 → aún STOP
    TEST_ASSERT_TRUE (referee_debounce_update(d, true, 170)); // 40 ms desde 130 → START
}

// Cable desconectado: INPUT_PULLDOWN da raw=false estable → oficial = STOP (fail-safe).
void test_debounce_disconnected_cable_stays_stop(void) {
    RefereeDebounce d{};
    for (uint32_t t = 0; t <= 1000; t += 10) {
        referee_debounce_update(d, false, t);   // crudo siempre LOW
    }
    TEST_ASSERT_FALSE(d.official);
}

// =========================== SEQ LINK HEALTH =================================

// Primer frame siembra y NO cuenta pérdida.
void test_seq_first_frame_no_loss(void) {
    LinkSeqTracker t{};
    TEST_ASSERT_EQUAL_UINT32(0, link_seq_track(t, 7));
    TEST_ASSERT_TRUE(t.have_last);
    TEST_ASSERT_EQUAL_UINT8(7, t.last_seq);
    TEST_ASSERT_EQUAL_UINT32(0, t.lost);
}

// Secuencia consecutiva → 0 perdidos.
void test_seq_consecutive_no_loss(void) {
    LinkSeqTracker t{};
    for (uint8_t s = 10; s < 60; ++s) link_seq_track(t, s);
    TEST_ASSERT_EQUAL_UINT32(0, t.lost);
}

// Salto de SEQ → suma exactamente el gap (cuántos faltaron).
void test_seq_gap_counts_missing(void) {
    LinkSeqTracker t{};
    link_seq_track(t, 5);
    TEST_ASSERT_EQUAL_UINT32(3, link_seq_track(t, 9));  // faltaron 6,7,8 = 3
    TEST_ASSERT_EQUAL_UINT32(3, t.lost);
}

// Gaps acumulan a lo largo del tiempo.
void test_seq_gaps_accumulate(void) {
    LinkSeqTracker t{};
    link_seq_track(t, 0);
    link_seq_track(t, 2);   // +1 (faltó 1)
    link_seq_track(t, 5);   // +2 (faltaron 3,4)
    link_seq_track(t, 6);   // +0
    TEST_ASSERT_EQUAL_UINT32(3, t.lost);
}

// Wrap 255→0 sin pérdida: gap = 0 (el cast uint8_t lo maneja).
void test_seq_wrap_no_loss(void) {
    LinkSeqTracker t{};
    link_seq_track(t, 254);
    TEST_ASSERT_EQUAL_UINT32(0, link_seq_track(t, 255));
    TEST_ASSERT_EQUAL_UINT32(0, link_seq_track(t, 0));   // wrap consecutivo
    TEST_ASSERT_EQUAL_UINT32(0, link_seq_track(t, 1));
    TEST_ASSERT_EQUAL_UINT32(0, t.lost);
}

// Wrap CON pérdida: 253 → 1 saltándose 254,255,0 = 3 perdidos.
void test_seq_wrap_with_loss(void) {
    LinkSeqTracker t{};
    link_seq_track(t, 253);
    TEST_ASSERT_EQUAL_UINT32(3, link_seq_track(t, 1));   // faltaron 254,255,0
    TEST_ASSERT_EQUAL_UINT32(3, t.lost);
}

// Frame intercalado de otro tipo con el mismo SEQ monótono por enlace: el gap
// sigue siendo correcto (la limitación documentada es que no distingue QUÉ tipo,
// no que cuente mal). Mientras el emisor use un único SEQ por enlace, esto vale.
void test_seq_interleaved_types_single_counter(void) {
    LinkSeqTracker t{};
    // Emisor manda tipo A (seq 0), tipo B (seq 1), tipo A (seq 2)... un solo SEQ.
    link_seq_track(t, 0);
    link_seq_track(t, 1);
    link_seq_track(t, 2);
    link_seq_track(t, 3);
    TEST_ASSERT_EQUAL_UINT32(0, t.lost);   // intercalado pero consecutivo → sin pérdida
}

int main(int, char**) {
    UNITY_BEGIN();
    // Debounce árbitro
    RUN_TEST(test_debounce_boot_is_stop);
    RUN_TEST(test_debounce_seed_does_not_jump);
    RUN_TEST(test_debounce_single_tick_glitch_ignored);
    RUN_TEST(test_debounce_burst_of_glitches_ignored);
    RUN_TEST(test_debounce_stable_high_starts);
    RUN_TEST(test_debounce_window_boundary);
    RUN_TEST(test_debounce_stable_low_returns_to_stop);
    RUN_TEST(test_debounce_glitch_low_during_play_ignored);
    RUN_TEST(test_debounce_candidate_change_resets_timer);
    RUN_TEST(test_debounce_disconnected_cable_stays_stop);
    // Seq link health
    RUN_TEST(test_seq_first_frame_no_loss);
    RUN_TEST(test_seq_consecutive_no_loss);
    RUN_TEST(test_seq_gap_counts_missing);
    RUN_TEST(test_seq_gaps_accumulate);
    RUN_TEST(test_seq_wrap_no_loss);
    RUN_TEST(test_seq_wrap_with_loss);
    RUN_TEST(test_seq_interleaved_types_single_counter);
    return UNITY_END();
}
