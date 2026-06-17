// test_reflex — Capa 1 del lazo RT CENTRAL: maquina de PRIORIDADES.
// Corre host con: bash scripts/run-host-tests.sh test_reflex
//
// Cubre el modulo PURO reflex.h. Verifica:
//   • Prioridad estricta: STOP gana sobre todo (incluso BRAKE_EDGE).
//   • STOP resetea el estado del borde (al volver a RUN, reflejo arranca limpio).
//   • Anti-latch: borde reciente -> BRAKE_EDGE; borde >=350 ms -> RELEASE_TO_FSM.
//   • RELEASE sostenido: tras RELEASE, NO re-frenar mientras edge_now siga alto.
//   • Re-arme: edge_now baja -> NONE + estado reseteado; nuevo edge -> BRAKE otra vez.
//   • Wrap-safe: el anti-latch funciona aunque now_ms cruce el overflow de uint32.
//   • REGRESION CSV Maria 2026-06-14: el deadlock de 5+ s NO debe volver.

#include <unity.h>
#include "reflex.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Helper: estado fresco (zero-init -> sin armar).
static ReflexState fresh() {
    ReflexState s{};
    return s;
}

// ── PRIORIDAD: STOP gana sobre TODO ──────────────────────────────────────────

// Sin partido corriendo y sin borde: STOP_MOTORS.
void test_stop_sin_borde(void) {
    ReflexState s = fresh();
    ReflexInputs in{ /*match*/false, /*edge*/false };
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::STOP_MOTORS),
                      static_cast<int>(reflex_arbitrate(s, in, 1000u)));
}

// Sin partido pero CON borde inminente: STOP_MOTORS (cambio de conducta vs hoy:
// hoy el STOP no preempta el borde; con esto si — riesgo R5 a validar en banco).
void test_stop_preempta_borde(void) {
    ReflexState s = fresh();
    ReflexInputs in{ /*match*/false, /*edge*/true };
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::STOP_MOTORS),
                      static_cast<int>(reflex_arbitrate(s, in, 1000u)));
}

// STOP resetea el estado del borde: si veniamos con anti-latch armado y se
// para el partido, al volver a RUN el reflejo arranca limpio (sin "memoria"
// del borde que estaba viendo cuando se paro).
void test_stop_resetea_estado_borde(void) {
    ReflexState s = fresh();
    // Armamos el borde a t=1000 con anti-latch andando.
    ReflexInputs run_edge{ true, true };
    reflex_arbitrate(s, run_edge, 1000u);   // BRAKE_EDGE (since=1000)
    TEST_ASSERT_NOT_EQUAL(0u, s.edge_since_ms);

    // Arbitro para a t=1200: STOP gana y resetea el estado del borde.
    ReflexInputs stop_edge{ false, true };
    reflex_arbitrate(s, stop_edge, 1200u);
    TEST_ASSERT_EQUAL_UINT32(0u, s.edge_since_ms);
    TEST_ASSERT_FALSE(s.edge_released);

    // Volvemos a RUN a t=1300 con el mismo borde: arranca un anti-latch NUEVO,
    // no continua el viejo (si fuera al reves, el RELEASE llegaria en 50 ms en
    // vez de en 350 — bug grave que congelaria al arquero al re-arrancar).
    ReflexInputs run_edge2{ true, true };
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::BRAKE_EDGE),
                      static_cast<int>(reflex_arbitrate(s, run_edge2, 1300u)));
    TEST_ASSERT_EQUAL_UINT32(1300u, s.edge_since_ms);
}

// ── ANTI-LATCH: BRAKE -> RELEASE a los 350 ms ────────────────────────────────

// Borde recien detectado: BRAKE_EDGE y arma el reloj.
void test_borde_reciente_brake(void) {
    ReflexState s = fresh();
    ReflexInputs in{ true, true };
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::BRAKE_EDGE),
                      static_cast<int>(reflex_arbitrate(s, in, 5000u)));
    TEST_ASSERT_EQUAL_UINT32(5000u, s.edge_since_ms);
    TEST_ASSERT_FALSE(s.edge_released);
}

// Borde mantenido <350 ms: sigue BRAKE_EDGE.
void test_borde_dentro_antilatch_brake(void) {
    ReflexState s = fresh();
    ReflexInputs in{ true, true };
    reflex_arbitrate(s, in, 5000u);    // arma a t=5000
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::BRAKE_EDGE),
                      static_cast<int>(reflex_arbitrate(s, in, 5100u)));   // +100 ms
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::BRAKE_EDGE),
                      static_cast<int>(reflex_arbitrate(s, in, 5349u)));   // +349 ms (<350)
}

// Borde mantenido al borde exacto del anti-latch: ya RELEASE (corte >=).
void test_borde_en_el_borde_release(void) {
    ReflexState s = fresh();
    ReflexInputs in{ true, true };
    reflex_arbitrate(s, in, 5000u);
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::RELEASE_TO_FSM),
                      static_cast<int>(reflex_arbitrate(s, in, 5350u)));  // exact >=350
    TEST_ASSERT_TRUE(s.edge_released);
}

// Borde mantenido bien pasado los 350 ms: sigue RELEASE (sostenido, NO re-frena).
void test_borde_pasado_antilatch_release_sostenido(void) {
    ReflexState s = fresh();
    ReflexInputs in{ true, true };
    reflex_arbitrate(s, in, 5000u);
    reflex_arbitrate(s, in, 5350u);    // RELEASE
    // 1, 2 y 5 segundos despues: SIGUE RELEASE (mientras edge_now siga true).
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::RELEASE_TO_FSM),
                      static_cast<int>(reflex_arbitrate(s, in, 6000u)));
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::RELEASE_TO_FSM),
                      static_cast<int>(reflex_arbitrate(s, in, 7000u)));
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::RELEASE_TO_FSM),
                      static_cast<int>(reflex_arbitrate(s, in, 10000u)));
}

// ── RE-ARME: edge baja -> NONE + estado reseteado; nuevo edge -> BRAKE otra vez ─

// Sin borde y con partido: NONE (la FSM manda).
void test_sin_borde_none(void) {
    ReflexState s = fresh();
    ReflexInputs in{ true, false };
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::NONE),
                      static_cast<int>(reflex_arbitrate(s, in, 1000u)));
    TEST_ASSERT_EQUAL_UINT32(0u, s.edge_since_ms);
}

// Borde -> bajo -> nuevo borde: re-arma el anti-latch desde cero.
void test_borde_baja_y_vuelve_rearma(void) {
    ReflexState s = fresh();
    // Borde de 5000 a 5500 (RELEASE a los 5350).
    reflex_arbitrate(s, {true, true},  5000u);   // BRAKE
    reflex_arbitrate(s, {true, true},  5350u);   // RELEASE
    TEST_ASSERT_TRUE(s.edge_released);

    // Edge baja en 5500: NONE y reset del estado.
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::NONE),
                      static_cast<int>(reflex_arbitrate(s, {true, false}, 5500u)));
    TEST_ASSERT_EQUAL_UINT32(0u, s.edge_since_ms);
    TEST_ASSERT_FALSE(s.edge_released);

    // Nuevo borde en 6000: BRAKE_EDGE (anti-latch arranca de cero, no de 5000).
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::BRAKE_EDGE),
                      static_cast<int>(reflex_arbitrate(s, {true, true}, 6000u)));
    TEST_ASSERT_EQUAL_UINT32(6000u, s.edge_since_ms);
    // Y el anti-latch nuevo vence 350 ms despues del nuevo borde (no del viejo).
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::BRAKE_EDGE),
                      static_cast<int>(reflex_arbitrate(s, {true, true}, 6300u)));
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::RELEASE_TO_FSM),
                      static_cast<int>(reflex_arbitrate(s, {true, true}, 6350u)));
}

// ── WRAP-SAFE: el anti-latch funciona aunque now_ms cruce el overflow ─────────

void test_wrap_safe_antilatch(void) {
    // Borde detectado 100 ms ANTES del wrap; evaluamos 250 ms DESPUES del wrap.
    // Intervalo real = 350 ms (justo en el borde del anti-latch).
    const uint32_t casi_max = 0xFFFFFFFFu - 100u + 1u;   // wrap en exactamente 100 ms
    ReflexState s = fresh();
    ReflexInputs in{ true, true };

    reflex_arbitrate(s, in, casi_max);   // BRAKE (arma a casi_max)
    TEST_ASSERT_EQUAL_UINT32(casi_max, s.edge_since_ms);

    // 100 ms hasta el wrap + 250 ms despues = 350 ms reales. Debe RELEASE.
    const uint32_t now = 250u;
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::RELEASE_TO_FSM),
                      static_cast<int>(reflex_arbitrate(s, in, now)));
}

// ── OVERRIDE de edge_max_ms (testing / titracion) ────────────────────────────

void test_override_edge_max_ms(void) {
    ReflexState s = fresh();
    ReflexInputs in{ true, true };
    // Con override de 100 ms (en vez de 350): RELEASE llega a los 100 ms.
    reflex_arbitrate(s, in, 1000u, /*edge_max_ms*/100u);   // BRAKE
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::BRAKE_EDGE),
                      static_cast<int>(reflex_arbitrate(s, in, 1099u, 100u)));
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::RELEASE_TO_FSM),
                      static_cast<int>(reflex_arbitrate(s, in, 1100u, 100u)));
}

// ── REGRESION: el deadlock del arquero de Maria (CSV 2026-06-14) ─────────────
//
// Sintoma vivo (caja negra v1.2): arquero pegado a su linea izquierda; cada
// vuelta del loop con imminent_exit=1 frenaba y hacia `return` -> nunca corria
// strategy_tick -> nunca disparaba ESCAPE -> seguia frenando -> DEADLOCK 5+ s
// (emerg=1 todo el tramo). El fix: tras 350 ms soltar el control para que la
// FSM ESCAPE despegue al robot. Este test prueba que el modulo PURO no re-cae
// en el bug y mantiene RELEASE mientras la linea siga inminente — la FSM puede
// hacer su escape sin que el reflejo le robe el control de vuelta.
void test_regresion_deadlock_maria(void) {
    ReflexState s = fresh();
    ReflexInputs in{ true, true };   // partido corriendo + borde inminente

    // Simulamos 5 segundos de loop a 100 Hz (cada 10 ms) con edge_now sostenido.
    int brake_count = 0;
    int release_count = 0;
    for (uint32_t t = 0; t <= 5000u; t += 10u) {
        ReflexAction a = reflex_arbitrate(s, in, t);
        if (a == ReflexAction::BRAKE_EDGE)    ++brake_count;
        if (a == ReflexAction::RELEASE_TO_FSM) ++release_count;
        // STOP/NONE NO deben aparecer (partido corre, borde alto todo el tramo).
        TEST_ASSERT_TRUE(a == ReflexAction::BRAKE_EDGE ||
                         a == ReflexAction::RELEASE_TO_FSM);
    }

    // El BRAKE solo cubre los primeros 350 ms = ~36 ticks @100 Hz (incluye el
    // tick exacto t=0 que arma). El resto (~465 ticks) debe ser RELEASE
    // SOSTENIDO. Si el modulo re-cayera en el deadlock, brake_count seria los
    // 501 ticks y release_count seria 0.
    TEST_ASSERT_TRUE_MESSAGE(brake_count > 0 && brake_count <= 40,
                             "BRAKE solo debe cubrir el anti-latch (~35 ticks)");
    TEST_ASSERT_TRUE_MESSAGE(release_count > 400,
                             "RELEASE debe sostenerse el resto del tramo");

    // Sanity: edge_released debe seguir true (no se cayo en el medio).
    TEST_ASSERT_TRUE(s.edge_released);
}

// REGRESION: tras RELEASE, si la linea SE VA y vuelve, el ciclo BRAKE->RELEASE
// arranca limpio (no quedan en RELEASE forever).
void test_regresion_release_no_es_pegamento(void) {
    ReflexState s = fresh();
    // Tramo 1: borde sostenido cae en RELEASE.
    reflex_arbitrate(s, {true, true}, 1000u);     // BRAKE
    reflex_arbitrate(s, {true, true}, 1350u);     // RELEASE
    TEST_ASSERT_TRUE(s.edge_released);

    // Linea se va.
    reflex_arbitrate(s, {true, false}, 1500u);    // NONE + reset
    TEST_ASSERT_FALSE(s.edge_released);
    TEST_ASSERT_EQUAL_UINT32(0u, s.edge_since_ms);

    // Tramo 2: nuevo borde -> BRAKE (no salta directo a RELEASE).
    TEST_ASSERT_EQUAL(static_cast<int>(ReflexAction::BRAKE_EDGE),
                      static_cast<int>(reflex_arbitrate(s, {true, true}, 2000u)));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_stop_sin_borde);
    RUN_TEST(test_stop_preempta_borde);
    RUN_TEST(test_stop_resetea_estado_borde);
    RUN_TEST(test_borde_reciente_brake);
    RUN_TEST(test_borde_dentro_antilatch_brake);
    RUN_TEST(test_borde_en_el_borde_release);
    RUN_TEST(test_borde_pasado_antilatch_release_sostenido);
    RUN_TEST(test_sin_borde_none);
    RUN_TEST(test_borde_baja_y_vuelve_rearma);
    RUN_TEST(test_wrap_safe_antilatch);
    RUN_TEST(test_override_edge_max_ms);
    RUN_TEST(test_regresion_deadlock_maria);
    RUN_TEST(test_regresion_release_no_es_pegamento);
    return UNITY_END();
}
