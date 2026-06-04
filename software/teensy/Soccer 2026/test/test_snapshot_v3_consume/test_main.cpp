// test_snapshot_v3_consume — decisiones PURAS del CONSUMO del WorldSnapshot v3
// en la placa CENTRAL. Corre host con:
//   bash scripts/run-host-tests.sh test_snapshot_v3_consume
//
// Cubre las 2 decisiones puras que el schema v3 agrega al consumo (strategy.cpp),
// extraídas a world_model.h (Arduino-free) para poder caracterizarlas host-side:
//
//   1) central_gate_heading_omega(heading_valid, omega_if_valid)
//      Compuerta del heading_valid (flags bit4). Si el BNO no convergió (heading=0
//      falso al boot) → ω=0 (no orientar con rumbo inventado). Si heading_valid →
//      ω idéntica a la computada por el HeadingPID (conducta IDÉNTICA a hoy).
//      Se usa en POSITION/APPROACH/CLEAR del delantero y CLEAR/PATROL/INTERCEPT
//      del arquero.
//
//   2) gk_own_goal_orient(goal_own_visible, heading_valid, my_heading, goal_own_angle)
//      Mejora del arquero: si el arco propio es visible (y heading válido) orienta
//      el frente a la cancha (de espaldas al arco propio). FALLBACK EXACTO: si NO
//      es visible o heading inválido → set_heading=false → el caller no setea
//      rumbo → ω=0 (idéntico a la conducta previa al schema v3).
//
// world_model.h sólo incluye <stdint.h> + types.h → compila host. Se incluye por
// ruta relativa porque run-host-tests.sh sólo agrega -I src/shared + el dir del
// test (NO -I src/central), igual que test_link_health con comm_arbiter.h. El test
// SÓLO llama a las funciones `inline` puras del header; NO llama a los accessors
// world_model_* (esos viven en world_model.cpp, que depende de Arduino y no se
// linkea acá).

#include <unity.h>
#include <cmath>
#include "../../src/central/world_model.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ===================== (1) heading_valid → gate de ω =========================

// heading_valid=1 (caso normal) → ω pasa SIN cambios (conducta idéntica a hoy).
void test_gate_heading_valid_passes_omega(void) {
    TEST_ASSERT_EQUAL_FLOAT(123.4f, central_gate_heading_omega(true, 123.4f));
    TEST_ASSERT_EQUAL_FLOAT(-57.0f, central_gate_heading_omega(true, -57.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f,   central_gate_heading_omega(true, 0.0f));
}

// heading_valid=0 (BNO sin converger / boot) → ω forzada a 0: NO orientar con
// heading=0 falso, sin importar qué ω hubiera pedido el PID.
void test_gate_heading_invalid_zeroes_omega(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, central_gate_heading_omega(false, 123.4f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, central_gate_heading_omega(false, -300.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, central_gate_heading_omega(false, 0.0f));
}

// El gate NO depende del signo ni de la magnitud de la ω entrante: sólo del flag.
void test_gate_is_pure_passthrough_or_zero(void) {
    for (float w = -500.0f; w <= 500.0f; w += 50.0f) {
        TEST_ASSERT_EQUAL_FLOAT(w,    central_gate_heading_omega(true,  w));
        TEST_ASSERT_EQUAL_FLOAT(0.0f, central_gate_heading_omega(false, w));
    }
}

// ============== (2) goal_own → orientación del arquero (GK) ===================

// Arco propio NO visible → FALLBACK EXACTO: set_heading=false (el caller no setea
// rumbo este tick → ω=0, idéntico a antes del schema v3).
void test_gk_orient_not_visible_is_fallback(void) {
    const GkOwnGoalOrient o = gk_own_goal_orient(/*visible=*/false, /*hv=*/true,
                                                 /*my_heading=*/30.0f, /*own_angle=*/45.0f);
    TEST_ASSERT_FALSE(o.set_heading);
}

// Arco propio visible PERO heading inválido → también fallback exacto (no orientar
// con heading=0 falso al boot, aunque se vea el arco).
void test_gk_orient_visible_but_heading_invalid_is_fallback(void) {
    const GkOwnGoalOrient o = gk_own_goal_orient(/*visible=*/true, /*hv=*/false,
                                                 /*my_heading=*/30.0f, /*own_angle=*/45.0f);
    TEST_ASSERT_FALSE(o.set_heading);
}

// Visible + heading válido → set_heading=true y el rumbo objetivo mira a la CANCHA
// (de espaldas al arco propio): target = my_heading + own_angle + 180, normalizado.
void test_gk_orient_visible_faces_field_away_from_own_goal(void) {
    // Arco propio justo detrás (own_angle=180 rel) y robot a heading 0 →
    // frente a la cancha = 0 + 180 + 180 = 360 → normaliza a 0.
    const GkOwnGoalOrient a = gk_own_goal_orient(true, true, 0.0f, 180.0f);
    TEST_ASSERT_TRUE(a.set_heading);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, a.heading_target_deg);

    // Arco propio al frente (own_angle=0) con robot a heading 0 → mirar al campo =
    // 0 + 0 + 180 = 180 (límite superior se mantiene en +180, no envuelve).
    const GkOwnGoalOrient b = gk_own_goal_orient(true, true, 0.0f, 0.0f);
    TEST_ASSERT_TRUE(b.set_heading);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 180.0f, b.heading_target_deg);
}

// El rumbo objetivo SIEMPRE queda normalizado en (-180, 180].
void test_gk_orient_target_is_normalized(void) {
    for (float h = -180.0f; h <= 180.0f; h += 45.0f) {
        for (float a = -180.0f; a <= 180.0f; a += 45.0f) {
            const GkOwnGoalOrient o = gk_own_goal_orient(true, true, h, a);
            TEST_ASSERT_TRUE(o.set_heading);
            TEST_ASSERT_TRUE(o.heading_target_deg >  -180.0f);
            TEST_ASSERT_TRUE(o.heading_target_deg <=  180.0f);
        }
    }
}

// Sentinela del contrato (mismo criterio que goal_opp): goal_own_visible=0 es el
// ÚNICO disparador del fallback en la rama "no visible". Aquí lo fijamos sobre la
// struct del snapshot para documentar el criterio que la compuerta consume.
void test_goal_own_visible_zero_means_fallback(void) {
    WorldSnapshot w{};
    w.goal_own_visible = 0;                 // no visible → angle/distance N/A
    w.goal_own_angle_centideg = 4500;       // dato presente pero NO confiable
    const bool visible = (w.goal_own_visible != 0);
    const GkOwnGoalOrient o = gk_own_goal_orient(visible, true, 0.0f,
                                                 w.goal_own_angle_centideg / 100.0f);
    TEST_ASSERT_FALSE(o.set_heading);       // sentinela honrado → fallback exacto
}

int main(int, char**) {
    UNITY_BEGIN();
    // heading_valid gate
    RUN_TEST(test_gate_heading_valid_passes_omega);
    RUN_TEST(test_gate_heading_invalid_zeroes_omega);
    RUN_TEST(test_gate_is_pure_passthrough_or_zero);
    // goal_own GK orient
    RUN_TEST(test_gk_orient_not_visible_is_fallback);
    RUN_TEST(test_gk_orient_visible_but_heading_invalid_is_fallback);
    RUN_TEST(test_gk_orient_visible_faces_field_away_from_own_goal);
    RUN_TEST(test_gk_orient_target_is_normalized);
    RUN_TEST(test_goal_own_visible_zero_means_fallback);
    return UNITY_END();
}
