// test_publish_decision - TDD de la decision PURA alive_now por slot del emisor
// (src/shared/publish_decision.h, FASE 2 del ratchet de vida read_stamp.h).
//
// Que cubre: que cada publish_alive_* devuelva el alive_now CORRECTO a partir de las
// senales que cada fuente expone, con la regla "no marcar vivo algo invalido". Estas
// son las funciones de POLITICA que el glue gateado (snapshot_emitter_publish) usa
// para alimentar read_stamp_tick; testearlas host garantiza que el sentinela
// por-sensor se dispara cuando debe, sin tocar hardware.
//
// Corre host (sin Arduino) con:
//   bash scripts/run-host-tests.sh test_publish_decision

#include <unity.h>
#include "publish_decision.h"
#include "read_stamp.h"     // integracion: alive_now -> read_stamp_tick -> stamp
#include "sensor_slot.h"    // integracion end-to-end: stamp -> slot_is_fresh

using namespace iitasoccer;

// struct local minimo trivialmente-copiable para el slot de integracion (no
// incluimos snapshot_from_slots.h para mantener el test enfocado en la decision +
// el ratchet; sensor_slot.h solo exige un POD copiable). DECLARADO ARRIBA de su uso.
struct HeadingObsLike { int16_t centideg; bool valid_src; };

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// HEADING: vivo == heading_valid_src. Un BNO no-valido NO esta vivo.
// ---------------------------------------------------------------------------
void test_heading_alive_iff_valid_src(void) {
    TEST_ASSERT_TRUE (publish_alive_heading(true));
    TEST_ASSERT_FALSE(publish_alive_heading(false));
}

// ---------------------------------------------------------------------------
// PELOTA: necesita camara viva Y pelota visible AHORA (AND de las dos).
// ---------------------------------------------------------------------------
void test_ball_alive_requires_cam_alive_and_visible(void) {
    TEST_ASSERT_TRUE (publish_alive_ball(/*cam*/true,  /*vis*/true));   // viva + ve
    TEST_ASSERT_FALSE(publish_alive_ball(/*cam*/true,  /*vis*/false));  // viva, no ve
    TEST_ASSERT_FALSE(publish_alive_ball(/*cam*/false, /*vis*/true));   // muda (dato rancio)
    TEST_ASSERT_FALSE(publish_alive_ball(/*cam*/false, /*vis*/false));  // muda + no ve
}

// ---------------------------------------------------------------------------
// ARCO: mismo criterio que la pelota (camara viva Y arco visible AHORA).
// ---------------------------------------------------------------------------
void test_goal_alive_requires_cam_alive_and_visible(void) {
    TEST_ASSERT_TRUE (publish_alive_goal(true,  true));
    TEST_ASSERT_FALSE(publish_alive_goal(true,  false));
    TEST_ASSERT_FALSE(publish_alive_goal(false, true));
}

// ---------------------------------------------------------------------------
// POSE: vivo == pose_valid_src. <2 ToFs (valid=false) NO esta vivo.
// ---------------------------------------------------------------------------
void test_pose_alive_iff_valid_src(void) {
    TEST_ASSERT_TRUE (publish_alive_pose(true));
    TEST_ASSERT_FALSE(publish_alive_pose(false));
}

// ---------------------------------------------------------------------------
// OBSTACULO: always-alive en el ratchet (liveness vive en el VALOR). El
// parametro reservado no cambia el resultado mientras no exista el bool real.
// ---------------------------------------------------------------------------
void test_obstacle_always_alive_in_ratchet(void) {
    TEST_ASSERT_TRUE(publish_alive_obstacle(false));
    TEST_ASSERT_TRUE(publish_alive_obstacle(true));
}

// ===========================================================================
// INTEGRACION end-to-end (la logica pura del cambio): un HEADING que se congela
// (valid_src deja de ser true con el loop vivo) hace ENVEJECER su slot y lo lleva
// a STALE al ritmo del SENSOR. Replica el flujo real del publish:
//   alive_now = publish_alive_heading(valid_src) -> read_stamp_tick -> slot_publish.
// Es el bug exacto que FASE 2 cierra: el "heading congelado-pero-valido" deja de
// re-estamparse fresco.
// ===========================================================================
void test_frozen_heading_ages_out_and_collapses(void) {
    SensorSlot<HeadingObsLike> slot{};
    ReadStampState st{};
    const uint32_t TH = 250u;            // SlotThresholds.heading_ms

    // t=1000: heading VALIDO -> alive -> stamp avanza -> FRESCO.
    {
        const bool alive = publish_alive_heading(/*valid_src=*/true);
        const uint32_t s = read_stamp_tick(st, alive, 1000u);
        slot_publish(slot, HeadingObsLike{1234, true}, s);
        TEST_ASSERT_TRUE(slot_is_fresh(slot, 1000u, TH));
    }
    // t=1100: BNO CONGELADO -> valid_src=false -> NO vivo -> stamp congelado en
    // 1000 -> 1100-1000=100 <= 250 -> todavia fresco (margen de 1 muestra perdida).
    {
        const bool alive = publish_alive_heading(/*valid_src=*/false);
        const uint32_t s = read_stamp_tick(st, alive, 1100u);
        slot_publish(slot, HeadingObsLike{1234, false}, s);   // valor rancio republicado
        TEST_ASSERT_TRUE(slot_is_fresh(slot, 1100u, TH));
    }
    // t=1300: sigue congelado. stamp congelado en 1000 -> 1300-1000=300 > 250 ->
    // STALE. El slot envejecio al ritmo del SENSOR aunque el loop siga publicando.
    {
        const bool alive = publish_alive_heading(/*valid_src=*/false);
        const uint32_t s = read_stamp_tick(st, alive, 1300u);
        slot_publish(slot, HeadingObsLike{1234, false}, s);
        TEST_ASSERT_FALSE(slot_is_fresh(slot, 1300u, TH));   // -> assemble limpia bit4
    }
    // CONTRASTE (status quo FASE 1, `now` plano): nunca habria envejecido.
    SensorSlot<HeadingObsLike> naive{};
    slot_publish(naive, HeadingObsLike{1234, false}, 1300u);
    TEST_ASSERT_TRUE(slot_is_fresh(naive, 1300u, TH));        // fresco PARA SIEMPRE (el bug)
}

// ===========================================================================
// INTEGRACION: una PELOTA que sale del FOV (visible->false con camara viva) hace
// envejecer su slot al ritmo de ball_ms (1000). Confirma el AND cam_alive&&visible
// dentro del flujo real.
// ===========================================================================
void test_ball_lost_from_fov_ages_out(void) {
    struct BallLike { int16_t x; bool visible; };
    SensorSlot<BallLike> slot{};
    ReadStampState st{};
    const uint32_t TH = 1000u;           // SlotThresholds.ball_ms

    // t=1000: camara viva + ve la pelota -> alive -> fresco.
    {
        const bool alive = publish_alive_ball(/*cam*/true, /*vis*/true);
        slot_publish(slot, BallLike{50, true}, read_stamp_tick(st, alive, 1000u));
        TEST_ASSERT_TRUE(slot_is_fresh(slot, 1000u, TH));
    }
    // t=2500: camara viva pero ya NO ve la pelota -> NO vivo -> stamp congelado en
    // 1000 -> 2500-1000=1500 > 1000 -> STALE (el slot colapsa al sentinela).
    {
        const bool alive = publish_alive_ball(/*cam*/true, /*vis*/false);
        slot_publish(slot, BallLike{50, false}, read_stamp_tick(st, alive, 2500u));
        TEST_ASSERT_FALSE(slot_is_fresh(slot, 2500u, TH));
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_heading_alive_iff_valid_src);
    RUN_TEST(test_ball_alive_requires_cam_alive_and_visible);
    RUN_TEST(test_goal_alive_requires_cam_alive_and_visible);
    RUN_TEST(test_pose_alive_iff_valid_src);
    RUN_TEST(test_obstacle_always_alive_in_ratchet);
    RUN_TEST(test_frozen_heading_ages_out_and_collapses);
    RUN_TEST(test_ball_lost_from_fov_ages_out);
    return UNITY_END();
}
