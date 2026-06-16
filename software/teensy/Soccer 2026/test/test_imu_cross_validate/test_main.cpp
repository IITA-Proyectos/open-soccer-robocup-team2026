// test_imu_cross_validate — salud del BNO por cross-validación independiente (TASK-212 Fase 1).
// Corre host: bash scripts/run-host-tests.sh test_imu_cross_validate
//
// El invariante #1 (review adversarial): NUNCA declarar MALO a un BNO con < 2 refs
// INDEPENDIENTES (vetar un sano es peor que el bug original). + verdictos SANO/SOSPECHA/
// MALO, anti-aliasing del centinela, saturación OTOS (B.3), strafe de cámara, scheduler.

#include <unity.h>
#include "imu_cross_validate.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

static int vi(XvalVerdict v) { return (int)v; }
static int ai(XvalAction a) { return (int)a; }

// SANO: primario coincide con OTOS y cámara → SANO, NINGUNA, score alto.
void test_healthy_agrees(void) {
    XvalState s{}; xval_reset(s);
    XvalParams p = xval_default_params();
    uint32_t now = 1000;
    for (int i = 0; i < 6; ++i) {
        xval_feed_primary(s, 20.0f, false, now);
        xval_feed_otos(s, 20.0f, true, now);
        xval_feed_camera(s, 19.0f, true, now);
        xval_update(s, p, now);
        now += 10;
    }
    TEST_ASSERT_EQUAL(vi(XvalVerdict::SANO), vi(xval_primary_verdict(s)));
    TEST_ASSERT_EQUAL(ai(XvalAction::NINGUNA), ai(xval_recommended_action(s)));
    TEST_ASSERT_TRUE(xval_primary_score(s) > 80.0f);
}

// CONGELADO vía INC-1: MALO inmediato (prueba interna), action FLAG_DRIFT (INC-1 ya
// degradó a heading_valid=0; no hay settle activo para un congelado).
void test_inc1_frozen_is_malo(void) {
    XvalState s{}; xval_reset(s);
    XvalParams p = xval_default_params();
    xval_feed_primary(s, 0.0f, /*frozen*/true, 1000);
    xval_feed_camera(s, 30.0f, true, 1000);
    xval_update(s, p, 1000);
    TEST_ASSERT_EQUAL(vi(XvalVerdict::MALO), vi(xval_primary_verdict(s)));
    TEST_ASSERT_EQUAL(ai(XvalAction::FLAG_DRIFT), ai(xval_recommended_action(s)));
}

// DERIVA con 2 refs INDEPENDIENTES que coinciden → MALO tras K ventanas → RECOMMEND_SETTLE.
void test_drift_two_refs_malo_after_k(void) {
    XvalState s{}; xval_reset(s);
    XvalParams p = xval_default_params();
    uint32_t now = 1000;
    for (int i = 0; i < (int)p.consensus_k; ++i) {
        xval_feed_primary(s, 0.0f, false, now);   // primario "quieto"
        xval_feed_otos(s, 25.0f, true, now);      // OTOS: girando 25
        xval_feed_camera(s, 24.0f, true, now);    // cámara: 24 (coincide con OTOS)
        xval_update(s, p, now);
        now += 10;
    }
    TEST_ASSERT_EQUAL(vi(XvalVerdict::MALO), vi(xval_primary_verdict(s)));
    TEST_ASSERT_EQUAL(ai(XvalAction::RECOMMEND_SETTLE), ai(xval_recommended_action(s)));
}

// DERIVA EN REPOSO: refs dicen ~0, el primario inventa rotación → MALO (con 2 refs).
void test_drift_at_rest(void) {
    XvalState s{}; xval_reset(s);
    XvalParams p = xval_default_params();
    uint32_t now = 1000;
    for (int i = 0; i < (int)p.consensus_k; ++i) {
        xval_feed_primary(s, 15.0f, false, now);  // inventa 15 dps
        xval_feed_otos(s, 0.0f, true, now);
        xval_feed_camera(s, 0.0f, true, now);
        xval_update(s, p, now);
        now += 10;
    }
    TEST_ASSERT_EQUAL(vi(XvalVerdict::MALO), vi(xval_primary_verdict(s)));
}

// INVARIANTE #1 — ANTI-FALSO-VETO: con UNA sola ref independiente (cámara), una
// divergencia GRANDE y SOSTENIDA jamás declara MALO (la cámara podría mentir). Máx SOSPECHA.
void test_single_ref_never_malo(void) {
    XvalState s{}; xval_reset(s);
    XvalParams p = xval_default_params();
    uint32_t now = 1000;
    for (int i = 0; i < 30; ++i) {
        xval_feed_primary(s, 0.0f, false, now);
        xval_feed_camera(s, 40.0f, true, now);    // SOLO cámara, divergencia enorme
        xval_update(s, p, now);
        now += 10;
    }
    TEST_ASSERT_NOT_EQUAL(vi(XvalVerdict::MALO), vi(xval_primary_verdict(s)));
    TEST_ASSERT_EQUAL(1, (int)xval_n_indep_refs(s));   // 1 ref independiente
}

// INVARIANTE #1 (extremo) — CERO refs independientes (solo el centinela, 2º BNO):
// jamás MALO (falla de modo común; un BNO no veta a otro BNO).
void test_zero_indep_refs_never_malo(void) {
    XvalState s{}; xval_reset(s);
    XvalParams p = xval_default_params();
    uint32_t now = 1000;
    for (int i = 0; i < 30; ++i) {
        xval_feed_primary(s, 0.0f, false, now);
        // centinela presente, evaluable (net_rotation alto), divergente:
        xval_feed_sentinel(s, 40.0f, /*present*/true, /*net_rotation*/40.0f, now);
        xval_update(s, p, now);
        now += 10;
    }
    TEST_ASSERT_NOT_EQUAL(vi(XvalVerdict::MALO), vi(xval_primary_verdict(s)));
    TEST_ASSERT_EQUAL(0, (int)xval_n_indep_refs(s));   // el centinela NO cuenta como independiente
}

// CONSENSO REFUTA SOSPECHA: el primario coincide con 2 refs que coinciden entre sí → SANO
// (aunque haya una tercera fuente — el centinela — un poco distinta).
void test_consensus_confirms_healthy(void) {
    XvalState s{}; xval_reset(s);
    XvalParams p = xval_default_params();
    uint32_t now = 1000;
    for (int i = 0; i < 6; ++i) {
        xval_feed_primary(s, 30.0f, false, now);
        xval_feed_otos(s, 30.0f, true, now);
        xval_feed_camera(s, 31.0f, true, now);
        xval_feed_sentinel(s, 28.0f, true, 30.0f, now);  // centinela un poco distinto, no cambia el veredicto
        xval_update(s, p, now);
        now += 10;
    }
    TEST_ASSERT_EQUAL(vi(XvalVerdict::SANO), vi(xval_primary_verdict(s)));
}

// SATURACIÓN OTOS (B.3): w_otos clavado en 327 → descartado de las refs → no veta un
// primario sano en giro rápido. (Código durmiente en R2; prueba el módulo puro.)
void test_otos_saturation_ignored(void) {
    XvalState s{}; xval_reset(s);
    XvalParams p = xval_default_params();
    uint32_t now = 1000;
    for (int i = 0; i < 10; ++i) {
        xval_feed_primary(s, 400.0f, false, now);   // giro rápido real
        xval_feed_otos(s, 327.0f, true, now);       // OTOS SATURADO → no confiable
        xval_update(s, p, now);
        now += 10;
    }
    TEST_ASSERT_EQUAL(0, (int)xval_n_indep_refs(s));   // OTOS saturado NO cuenta
    TEST_ASSERT_NOT_EQUAL(vi(XvalVerdict::MALO), vi(xval_primary_verdict(s)));
}

// CÁMARA SESGADA POR STRAFE: el glue pasa cam valid=false (|vel_lateral| no ~0) → la
// cámara no cuenta → no se sospecha del primario sano por un d(bearing)/dt de traslación.
void test_camera_strafe_bias_no_suspect(void) {
    XvalState s{}; xval_reset(s);
    XvalParams p = xval_default_params();
    uint32_t now = 1000;
    for (int i = 0; i < 10; ++i) {
        xval_feed_primary(s, 0.0f, false, now);
        xval_feed_camera(s, 40.0f, /*valid*/false, now);  // strafe → inválida
        xval_update(s, p, now);
        now += 10;
    }
    TEST_ASSERT_EQUAL(0, (int)xval_n_indep_refs(s));
    TEST_ASSERT_NOT_EQUAL(vi(XvalVerdict::MALO), vi(xval_primary_verdict(s)));
}

// CÁMARA STALE: vista al alimentar pero vieja al evaluar (> fresh_timeout) → no cuenta.
void test_camera_stale_not_counted(void) {
    XvalState s{}; xval_reset(s);
    XvalParams p = xval_default_params();
    xval_feed_camera(s, 30.0f, true, 1000);
    xval_feed_primary(s, 0.0f, false, 1000 + 1000);   // 1 s después
    xval_update(s, p, 1000 + 1000);                   // cámara ya vieja
    TEST_ASSERT_EQUAL(0, (int)xval_n_indep_refs(s));
}

// CENTINELA NO-EVALUABLE (anti-aliasing 1 Hz): net_rotation ~0 (giró y volvió) → el
// centinela NO entra a las refs aunque esté presente y fresco.
void test_sentinel_non_evaluable_window(void) {
    XvalState s{}; xval_reset(s);
    XvalParams p = xval_default_params();
    uint32_t now = 1000;
    xval_feed_sentinel(s, 50.0f, /*present*/true, /*net_rotation*/0.0f, now);  // neta ~0
    xval_feed_primary(s, 0.0f, false, now);
    xval_update(s, p, now);
    TEST_ASSERT_NOT_EQUAL(vi(XvalVerdict::MALO), vi(xval_primary_verdict(s)));
}

// SCHEDULER del centinela: arranca due, arma, completa → re-programa a +period; y el
// TIMEOUT defensivo re-arma sin colgar.
void test_sentinel_scheduler_and_timeout(void) {
    XvalState s{}; xval_reset(s);
    XvalParams p = xval_default_params();
    // due al boot (sentinel_due_ms=0).
    TEST_ASSERT_TRUE(xval_sentinel_due(s, 0));
    xval_sentinel_arm(s, p, 0);
    TEST_ASSERT_FALSE(xval_sentinel_due(s, 50));      // ya en REQUESTED, no due
    // completa antes del timeout → re-programa a +period.
    xval_sentinel_done(s, p, 50);
    TEST_ASSERT_FALSE(xval_sentinel_due(s, 100));     // todavía no (due=50+1000)
    TEST_ASSERT_TRUE(xval_sentinel_due(s, 50 + (int)p.sentinel_period_ms));
    // ahora probamos el TIMEOUT: armar y no completar.
    uint32_t now = 50 + p.sentinel_period_ms;
    xval_sentinel_arm(s, p, now);
    TEST_ASSERT_FALSE(xval_sentinel_timed_out(s, p, now + 10));     // dentro del timeout
    TEST_ASSERT_TRUE(xval_sentinel_timed_out(s, p, now + p.sentinel_timeout_ms));  // vencido → re-arma
    TEST_ASSERT_TRUE(xval_sentinel_due(s, now + p.sentinel_timeout_ms + p.sentinel_period_ms));
}

// JERARQUÍA DE DEGRADACIÓN: sin centinela presente, xval_sentinel_healthy=false (gate
// duro para cualquier failover futuro → preferir heading_valid=0).
void test_sentinel_healthy_gate(void) {
    XvalState s{}; xval_reset(s);
    TEST_ASSERT_FALSE(xval_sentinel_healthy(s));       // nunca alimentado
    xval_feed_sentinel(s, 10.0f, /*present*/true, 10.0f, 1000);
    TEST_ASSERT_TRUE(xval_sentinel_healthy(s));
    xval_feed_sentinel(s, 10.0f, /*present*/false, 10.0f, 2000);  // secundario ausente
    TEST_ASSERT_FALSE(xval_sentinel_healthy(s));
}

// H5 (path R1, que SÍ tiene OTOS): con el OTOS como ÚNICA ref independiente, una
// divergencia grande NUNCA declara MALO (1 ref podría mentir por patinazo). Solo SOSPECHA.
void test_otos_single_ref_never_malo(void) {
    XvalState s{}; xval_reset(s);
    XvalParams p = xval_default_params();
    uint32_t now = 1000;
    for (int i = 0; i < 30; ++i) {
        xval_feed_primary(s, 0.0f, false, now);
        xval_feed_otos(s, 30.0f, /*valid*/true, now);   // SOLO OTOS (R1)
        xval_update(s, p, now);
        now += 10;
    }
    TEST_ASSERT_NOT_EQUAL(vi(XvalVerdict::MALO), vi(xval_primary_verdict(s)));
    TEST_ASSERT_EQUAL(1, (int)xval_n_indep_refs(s));
}

// CONSENSO DÉBIL (gap cerrado): 2 refs independientes que DISCREPAN entre sí (una miente)
// NO escalan a MALO aunque el primario difiera de la mediana. Solo SOSPECHA.
void test_two_indep_refs_disagree_never_malo(void) {
    XvalState s{}; xval_reset(s);
    XvalParams p = xval_default_params();
    uint32_t now = 1000;
    for (int i = 0; i < 30; ++i) {
        xval_feed_primary(s, 30.0f, false, now);   // el primario coincide con el OTOS
        xval_feed_otos(s, 30.0f, true, now);       // OTOS dice 30
        xval_feed_camera(s, -30.0f, true, now);    // cámara dice -30 (discrepan fuerte entre sí)
        xval_update(s, p, now);
        now += 10;
    }
    // n_indep=2 pero SIN consenso fuerte (|30-(-30)|>tol) → nunca MALO.
    TEST_ASSERT_NOT_EQUAL(vi(XvalVerdict::MALO), vi(xval_primary_verdict(s)));
    TEST_ASSERT_EQUAL(2, (int)xval_n_indep_refs(s));
}

// El centinela se lee a 1 Hz → su frescura usa sentinel_fresh_timeout_ms (1300), NO el
// fresh_timeout_ms de 300. Sin esto, entre lecturas el centinela quedaría stale y su rama
// sería CÓDIGO MUERTO. A 1200 ms sigue contando; a 1400 ms (>1300) queda stale.
void test_sentinel_fresco_a_cadencia_1hz(void) {
    XvalParams p = xval_default_params();
    XvalState s{}; xval_reset(s);
    xval_feed_sentinel(s, 40.0f, /*present*/true, /*net_rotation*/40.0f, 1000);
    xval_feed_primary(s, 0.0f, false, 2200);   // 1200 ms después
    xval_update(s, p, 2200);
    TEST_ASSERT_TRUE(xval_window_evaluable(s));   // centinela aún fresco → hay ref → ventana evaluable
    TEST_ASSERT_NOT_EQUAL(vi(XvalVerdict::MALO), vi(xval_primary_verdict(s)));  // 2º BNO no veta
    XvalState s2{}; xval_reset(s2);
    xval_feed_sentinel(s2, 40.0f, true, 40.0f, 1000);
    xval_feed_primary(s2, 0.0f, false, 2400);   // 1400 ms (> 1300) → stale
    xval_update(s2, p, 2400);
    TEST_ASSERT_FALSE(xval_window_evaluable(s2));  // sin refs → ventana no evaluable
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_healthy_agrees);
    RUN_TEST(test_sentinel_fresco_a_cadencia_1hz);
    RUN_TEST(test_otos_single_ref_never_malo);
    RUN_TEST(test_two_indep_refs_disagree_never_malo);
    RUN_TEST(test_inc1_frozen_is_malo);
    RUN_TEST(test_drift_two_refs_malo_after_k);
    RUN_TEST(test_drift_at_rest);
    RUN_TEST(test_single_ref_never_malo);
    RUN_TEST(test_zero_indep_refs_never_malo);
    RUN_TEST(test_consensus_confirms_healthy);
    RUN_TEST(test_otos_saturation_ignored);
    RUN_TEST(test_camera_strafe_bias_no_suspect);
    RUN_TEST(test_camera_stale_not_counted);
    RUN_TEST(test_sentinel_non_evaluable_window);
    RUN_TEST(test_sentinel_scheduler_and_timeout);
    RUN_TEST(test_sentinel_healthy_gate);
    return UNITY_END();
}
