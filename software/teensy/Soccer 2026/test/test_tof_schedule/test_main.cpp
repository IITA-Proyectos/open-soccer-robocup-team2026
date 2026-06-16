// test_tof_schedule — TDD del TURNERO round-robin de los 4 ToF (PURO).
//
// Cubre src/shared/tof_schedule.h: tof_sched_next() reparte turnos entre los ToF
// vivos en round-robin, SALTA al caído (ready=false) y devuelve TOF_SCHED_NONE
// (0xFF) sin colgarse cuando los 4 están caídos. Verifica además la
// byte-equivalencia con el round-robin de hoy (s_rr = (s_rr+1)%NUM_TOF) y la
// re-incorporación de un ToF que vuelve a ready.
//
// Corre host (sin Arduino) con:
//   bash scripts/run-host-tests.sh test_tof_schedule

#include <unity.h>
#include "tof_schedule.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Helper: estado con los 4 ToF listos (rr=0).
static TofSchedState all_ready_state(void) {
    TofSchedState st{};
    for (uint8_t i = 0; i < TOF_SCHED_N; ++i) tof_sched_set_ready(st, i, true);
    return st;
}

// ============================================================================
// (0) ZERO-INIT == fail-safe: nadie ready → NONE, sin colgar.
// ============================================================================

void test_zero_init_all_down_returns_none(void) {
    TofSchedState st{};  // .bss: ready[]=false, rr=0
    // Varias veces seguidas: siempre NONE, nunca cuelga, rr no se ensucia.
    for (int n = 0; n < 10; ++n) {
        TEST_ASSERT_EQUAL_UINT8(TOF_SCHED_NONE, tof_sched_next(st, (uint32_t)n));
    }
    TEST_ASSERT_EQUAL_UINT8(0, st.rr);  // sin ready, rr no avanza
}

// ============================================================================
// (1) ROUND-ROBIN PURO: 4 ready → 0,1,2,3,0,1,2,3,...
// ============================================================================

void test_round_robin_all_ready_cycles(void) {
    TofSchedState st = all_ready_state();
    const uint8_t expected[] = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1};
    for (int i = 0; i < 10; ++i) {
        TEST_ASSERT_EQUAL_UINT8(expected[i], tof_sched_next(st, (uint32_t)i));
    }
}

// ============================================================================
// (2) BYTE-EQUIVALENCIA con el round-robin de hoy:
//     next() con 4 ready == (s_rr + 1) % 4 / "i = s_rr; s_rr = (s_rr+1)%N".
//     El driver hace: i = s_rr (devuelve el actual); s_rr avanza. Replicamos el
//     modelo de referencia y exigimos que coincida turno a turno por 40 pasos.
// ============================================================================

void test_byte_equivalence_with_legacy_rr(void) {
    TofSchedState st = all_ready_state();
    uint8_t s_rr = 0;  // modelo de referencia (sensors_tof.cpp)
    for (int step = 0; step < 40; ++step) {
        const uint8_t ref = s_rr;
        s_rr = (uint8_t)((s_rr + 1) % TOF_SCHED_N);
        const uint8_t got = tof_sched_next(st, (uint32_t)step);
        TEST_ASSERT_EQUAL_UINT8(ref, got);
    }
}

// ============================================================================
// (3) SKIP DEL CAÍDO: ready[2]=false → 0,1,3,0,1,3,...
//     (el índice 2 nunca sale; los otros 3 rotan parejo).
// ============================================================================

void test_skip_dead_tof_rotates_alive(void) {
    TofSchedState st = all_ready_state();
    tof_sched_set_ready(st, 2, false);  // ToF 2 caído

    const uint8_t expected[] = {0, 1, 3, 0, 1, 3, 0, 1, 3};
    for (int i = 0; i < 9; ++i) {
        const uint8_t got = tof_sched_next(st, (uint32_t)i);
        TEST_ASSERT_EQUAL_UINT8(expected[i], got);
        TEST_ASSERT_NOT_EQUAL(2, got);  // el caído NUNCA sale
    }
}

// --- Caído al inicio (ready[0]=false) → arranca en 1: 1,2,3,1,2,3,...
void test_skip_dead_at_zero(void) {
    TofSchedState st = all_ready_state();
    tof_sched_set_ready(st, 0, false);
    const uint8_t expected[] = {1, 2, 3, 1, 2, 3};
    for (int i = 0; i < 6; ++i) {
        TEST_ASSERT_EQUAL_UINT8(expected[i], tof_sched_next(st, (uint32_t)i));
    }
}

// --- Dos caídos (1 y 3) → sólo rotan 0 y 2: 0,2,0,2,...
void test_two_dead_rotates_remaining_pair(void) {
    TofSchedState st = all_ready_state();
    tof_sched_set_ready(st, 1, false);
    tof_sched_set_ready(st, 3, false);
    const uint8_t expected[] = {0, 2, 0, 2, 0, 2};
    for (int i = 0; i < 6; ++i) {
        TEST_ASSERT_EQUAL_UINT8(expected[i], tof_sched_next(st, (uint32_t)i));
    }
}

// --- Un solo vivo (sólo el 2) → siempre devuelve 2, nunca cuelga.
void test_single_alive_always_same(void) {
    TofSchedState st{};
    tof_sched_set_ready(st, 2, true);
    for (int i = 0; i < 8; ++i) {
        TEST_ASSERT_EQUAL_UINT8(2, tof_sched_next(st, (uint32_t)i));
    }
}

// ============================================================================
// (4) TODOS CAÍDOS (tras estar vivos) → NONE, sin loop infinito.
//     Si llega aquí es porque NO se colgó (la cota TOF_SCHED_N garantiza salida).
// ============================================================================

void test_all_dead_after_alive_returns_none(void) {
    TofSchedState st = all_ready_state();
    // Rotó un poco (rr quedó en cualquier lado)...
    tof_sched_next(st, 0);
    tof_sched_next(st, 1);
    // ...y ahora se caen los 4.
    for (uint8_t i = 0; i < TOF_SCHED_N; ++i) tof_sched_set_ready(st, i, false);
    for (int n = 0; n < 12; ++n) {
        TEST_ASSERT_EQUAL_UINT8(TOF_SCHED_NONE, tof_sched_next(st, (uint32_t)n));
    }
}

// ============================================================================
// (5) RE-INCORPORACIÓN: un ToF que vuelve a ready entra de nuevo a la rotación.
// ============================================================================

void test_recovered_tof_rejoins_rotation(void) {
    TofSchedState st = all_ready_state();
    tof_sched_set_ready(st, 1, false);  // ToF 1 cae

    // Sin el 1: 0,2,3,0,2,3...
    TEST_ASSERT_EQUAL_UINT8(0, tof_sched_next(st, 0));
    TEST_ASSERT_EQUAL_UINT8(2, tof_sched_next(st, 1));
    TEST_ASSERT_EQUAL_UINT8(3, tof_sched_next(st, 2));
    TEST_ASSERT_EQUAL_UINT8(0, tof_sched_next(st, 3));

    // El 1 se recupera.
    tof_sched_set_ready(st, 1, true);

    // Ahora el 1 vuelve a salir en la rotación. rr quedó en 1 tras servir el 0,
    // así que el próximo es el 1 (ya recuperado).
    TEST_ASSERT_EQUAL_UINT8(1, tof_sched_next(st, 4));
    TEST_ASSERT_EQUAL_UINT8(2, tof_sched_next(st, 5));
    TEST_ASSERT_EQUAL_UINT8(3, tof_sched_next(st, 6));
    TEST_ASSERT_EQUAL_UINT8(0, tof_sched_next(st, 7));
    TEST_ASSERT_EQUAL_UINT8(1, tof_sched_next(st, 8));  // el 1 sigue en juego
}

// ============================================================================
// (6) set_ready / note_attempt: índice fuera de rango = no-op (no corrompe).
// ============================================================================

void test_out_of_range_index_is_noop(void) {
    TofSchedState st = all_ready_state();
    // Fuera de rango: no debe escribir fuera del array ni cambiar el comportamiento.
    tof_sched_set_ready(st, TOF_SCHED_N, false);      // i == N (inválido)
    tof_sched_set_ready(st, 200, false);              // muy fuera
    tof_sched_note_attempt(st, TOF_SCHED_N, 123);     // inválido
    tof_sched_note_attempt(st, 99, 456);              // inválido
    // Los 4 siguen ready → round-robin intacto.
    const uint8_t expected[] = {0, 1, 2, 3, 0};
    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_UINT8(expected[i], tof_sched_next(st, (uint32_t)i));
    }
}

// note_attempt sella el tiempo en el slot correcto y NO toca ready/rr.
void test_note_attempt_records_time_only(void) {
    TofSchedState st = all_ready_state();
    tof_sched_note_attempt(st, 2, 7777u);
    TEST_ASSERT_EQUAL_UINT32(7777u, st.last_attempt_ms[2]);
    TEST_ASSERT_EQUAL_UINT32(0u,    st.last_attempt_ms[0]);  // los otros sin tocar
    TEST_ASSERT_EQUAL_UINT8(0, st.rr);                        // rr no se movió
    TEST_ASSERT_TRUE(st.ready[2]);                            // ready intacto
}

// ============================================================================
// (7) MODO PRIORIZADO (opcional): le da el turno al de last_attempt MÁS VIEJO.
// ============================================================================

void test_prioritized_picks_oldest_attempt(void) {
    TofSchedState st = all_ready_state();
    // Sellamos intentos recientes para 0,1,3 y dejamos el 2 sin tocar (más viejo).
    tof_sched_note_attempt(st, 0, 1000u);
    tof_sched_note_attempt(st, 1, 1000u);
    tof_sched_note_attempt(st, 3, 1000u);
    // 2 quedó en 0 (nunca intentado) → es el más viejo a now=1000.
    TEST_ASSERT_EQUAL_UINT8(2, tof_sched_next_prioritized(st, 1000u));
}

// Priorizado salta al caído aunque sea el más viejo.
void test_prioritized_skips_dead(void) {
    TofSchedState st = all_ready_state();
    tof_sched_note_attempt(st, 0, 500u);
    tof_sched_note_attempt(st, 1, 500u);
    tof_sched_note_attempt(st, 3, 500u);
    // 2 sería el más viejo (sin intento) pero está caído → debe elegir otro.
    tof_sched_set_ready(st, 2, false);
    const uint8_t got = tof_sched_next_prioritized(st, 1000u);
    TEST_ASSERT_NOT_EQUAL(2, got);
    TEST_ASSERT_NOT_EQUAL(TOF_SCHED_NONE, got);
}

// Priorizado con todos caídos → NONE.
void test_prioritized_all_dead_returns_none(void) {
    TofSchedState st{};  // nadie ready
    TEST_ASSERT_EQUAL_UINT8(TOF_SCHED_NONE, tof_sched_next_prioritized(st, 1234u));
}

// Priorizado wrap-safe: el "más viejo" se mide con resta unsigned (sin falso joven
// por el wrap de millis()). last_attempt cerca del wrap, now justo después.
void test_prioritized_wrap_safe(void) {
    TofSchedState st = all_ready_state();
    const uint32_t near_wrap = 0xFFFFFFF0u;
    // 0 intentado ANTES del wrap (hace 0x20 ms respecto a now) → el más viejo.
    tof_sched_note_attempt(st, 0, near_wrap);
    // 1,2,3 intentados DESPUÉS del wrap (recién, edad ~0).
    const uint32_t now = near_wrap + 0x20u;  // envuelve a 0x10
    tof_sched_note_attempt(st, 1, now);
    tof_sched_note_attempt(st, 2, now);
    tof_sched_note_attempt(st, 3, now);
    // edad(0) = now - near_wrap = 0x20 (grande), edad(1,2,3)=0 → gana el 0.
    TEST_ASSERT_EQUAL_UINT8(0, tof_sched_next_prioritized(st, now));
}

int main(int, char**) {
    UNITY_BEGIN();
    // (0) fail-safe por defecto
    RUN_TEST(test_zero_init_all_down_returns_none);
    // (1) round-robin puro
    RUN_TEST(test_round_robin_all_ready_cycles);
    // (2) byte-equivalencia con el legacy
    RUN_TEST(test_byte_equivalence_with_legacy_rr);
    // (3) skip del caído
    RUN_TEST(test_skip_dead_tof_rotates_alive);
    RUN_TEST(test_skip_dead_at_zero);
    RUN_TEST(test_two_dead_rotates_remaining_pair);
    RUN_TEST(test_single_alive_always_same);
    // (4) todos caídos → NONE sin colgar
    RUN_TEST(test_all_dead_after_alive_returns_none);
    // (5) re-incorporación
    RUN_TEST(test_recovered_tof_rejoins_rotation);
    // (6) set_ready / note_attempt
    RUN_TEST(test_out_of_range_index_is_noop);
    RUN_TEST(test_note_attempt_records_time_only);
    // (7) modo priorizado opcional
    RUN_TEST(test_prioritized_picks_oldest_attempt);
    RUN_TEST(test_prioritized_skips_dead);
    RUN_TEST(test_prioritized_all_dead_returns_none);
    RUN_TEST(test_prioritized_wrap_safe);
    return UNITY_END();
}
