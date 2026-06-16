// test_snapshot_from_slots — TDD de LA COSTURA pizarra→ensamblador.
//
// Cubre src/shared/snapshot_from_slots.h: inputs_from_slots() lee cada
// SensorSlot<T> del SensorBlackboard, decide su frescura con slot_is_fresh()
// contra un único `now`, y arma el SnapshotInputs que consume el ensamblador.
//
// Verifica el CONTRATO (el dato fresco pasa) y el FAIL-SAFE (slot viejo / nunca
// publicado ⇒ *_fresh=false ⇒ assemble_snapshot colapsa a sentinela), el wrap de
// millis(), la consistencia de un solo `now`, el paso-tal-cual de los campos COMM
// y la INTEGRACIÓN end-to-end (inputs_from_slots → assemble_snapshot).
//
// Corre host (sin Arduino) con:
//   bash scripts/run-host-tests.sh test_snapshot_from_slots
//
// run-host-tests.sh compila con -I src/shared; el módulo es puro (sólo <stdint.h>
// + sensor_slot.h + snapshot_assembler.h + types.h) → no linkea Arduino.

#include <unity.h>
#include "snapshot_from_slots.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ----------------------------------------------------------------------------
// Helper: publica un blackboard COMPLETO con datos distinguibles, todo "ahora".
// Cada productor publica en t = base_ms (mismo instante). Los tests degradan un
// solo slot (lo publican viejo, o no lo publican) y verifican la independencia.
// ----------------------------------------------------------------------------
static void publish_all(SensorBlackboard& bb, uint32_t at) {
    BallObs b{};   b.x = 111; b.y = 222; b.conf = 95; b.vx = 33; b.vy = -44; b.visible = true;
    slot_publish(bb.ball, b, at);

    GoalObs go{};  go.ang = 1500;  go.dist = 1800; go.visible = true;
    slot_publish(bb.goal_opp, go, at);

    GoalObs gp{};  gp.ang = -3000; gp.dist = 900;  gp.visible = true;
    slot_publish(bb.goal_own, gp, at);

    HeadingObs h{}; h.centideg = 4500; h.valid_src = true;
    slot_publish(bb.heading, h, at);

    PoseObs p{};   p.x = 500; p.y = -700; p.conf = 70; p.valid_src = true;
    slot_publish(bb.pose, p, at);

    ObstObs o{};   o.mm = 350;
    slot_publish(bb.obstacle, o, at);

    bb.referee_cmd         = 1;     // START
    bb.match_running       = true;
    bb.partner_alive       = true;
    bb.partner_sees_ball   = true;
    bb.in_own_penalty_area = true;
}

// Umbrales default (los del firmware vivo).
static SlotThresholds defaults(void) { return SlotThresholds{}; }

// ============================================================================
// (1) Slot publicado RECIÉN (now - 10ms, umbral 1000ms) → fresh, dato pasa.
// ============================================================================
void test_fresh_slot_passes_data(void) {
    SensorBlackboard bb{};
    publish_all(bb, 1000);
    const uint32_t now = 1010;   // 10 ms después → fresco para todos los umbrales

    const SnapshotInputs in = inputs_from_slots(bb, now, defaults());

    // Frescura de cada slot → true.
    TEST_ASSERT_TRUE(in.ball_fresh);
    TEST_ASSERT_TRUE(in.goal_opp_fresh);
    TEST_ASSERT_TRUE(in.goal_own_fresh);
    TEST_ASSERT_TRUE(in.heading_fresh);
    TEST_ASSERT_TRUE(in.pose_fresh);
    TEST_ASSERT_TRUE(in.obstacle_fresh);

    // El dato pasa tal cual a los campos del input.
    TEST_ASSERT_EQUAL_INT16(111, in.ball_x_mm);
    TEST_ASSERT_EQUAL_INT16(222, in.ball_y_mm);
    TEST_ASSERT_EQUAL_UINT8(95,  in.ball_confidence);
    TEST_ASSERT_TRUE(in.ball_visible);
    TEST_ASSERT_EQUAL_INT16(1500, in.goal_opp_angle_centideg);
    TEST_ASSERT_EQUAL_INT16(4500, in.heading_centideg);
    TEST_ASSERT_TRUE(in.heading_valid_src);
    TEST_ASSERT_EQUAL_INT16(500,  in.pose_x_mm);
    TEST_ASSERT_EQUAL_UINT16(350, in.min_obstacle_mm);
}

// ============================================================================
// (2) Slot viejo (now - 500ms, umbral 250ms para ToF) → *_fresh=false →
//     assemble colapsa ESE campo a sentinela; el resto sobrevive.
// ============================================================================
void test_stale_obstacle_marks_not_fresh_and_collapses(void) {
    SensorBlackboard bb{};
    publish_all(bb, 1000);
    const uint32_t now = 1500;   // 500 ms después → ToF (250) vencido; cámara (1000) viva

    const SnapshotInputs in = inputs_from_slots(bb, now, defaults());

    // Obstáculo (umbral 250) venció; cámaras (umbral 1000) siguen frescas.
    TEST_ASSERT_FALSE(in.obstacle_fresh);
    TEST_ASSERT_FALSE(in.pose_fresh);     // pose umbral 250 también venció
    TEST_ASSERT_FALSE(in.heading_fresh);  // heading umbral 250 también venció
    TEST_ASSERT_TRUE(in.ball_fresh);
    TEST_ASSERT_TRUE(in.goal_opp_fresh);
    TEST_ASSERT_TRUE(in.goal_own_fresh);

    // El ensamblador colapsa lo no-fresco y deja vivo lo fresco.
    const WorldSnapshot s = assemble_snapshot(in);
    TEST_ASSERT_EQUAL_UINT16(WS_OBSTACLE_NONE, s.min_obstacle_mm);  // ToF viejo → NONE
    TEST_ASSERT_EQUAL_UINT8(0, s.my_pose_confidence);               // pose vieja → conf 0
    TEST_ASSERT_FALSE(s.flags & WS_FLAG_HEADING_VALID);             // heading viejo → bit off
    TEST_ASSERT_EQUAL_UINT8(1, s.ball_visible);                     // pelota viva
    TEST_ASSERT_EQUAL_UINT8(1, s.goal_opp_visible);                 // arco vivo
}

// Caso aislado clásico del contrato: solo la pelota queda vieja (cámara umbral
// 1000), el resto fresco. Verifica que un slot viejo NO arrastra a los demás.
void test_stale_ball_only_collapses_ball(void) {
    SensorBlackboard bb{};
    // Publico todo en t=0, después re-publico TODO menos la pelota en t=2000.
    publish_all(bb, 0);
    const uint32_t now = 2000;
    GoalObs go{};  go.ang = 1500;  go.dist = 1800; go.visible = true; slot_publish(bb.goal_opp, go, now);
    GoalObs gp{};  gp.ang = -3000; gp.dist = 900;  gp.visible = true; slot_publish(bb.goal_own, gp, now);
    HeadingObs h{}; h.centideg = 4500; h.valid_src = true;            slot_publish(bb.heading, h, now);
    PoseObs p{};   p.x = 500; p.y = -700; p.conf = 70; p.valid_src = true; slot_publish(bb.pose, p, now);
    ObstObs o{};   o.mm = 350;                                        slot_publish(bb.obstacle, o, now);
    // pelota quedó en t=0 → 2000 ms vieja, umbral 1000 → no fresca.

    const SnapshotInputs in = inputs_from_slots(bb, now, defaults());
    TEST_ASSERT_FALSE(in.ball_fresh);
    TEST_ASSERT_TRUE(in.goal_opp_fresh);
    TEST_ASSERT_TRUE(in.heading_fresh);
    TEST_ASSERT_TRUE(in.pose_fresh);
    TEST_ASSERT_TRUE(in.obstacle_fresh);

    const WorldSnapshot s = assemble_snapshot(in);
    TEST_ASSERT_EQUAL_UINT8(0, s.ball_visible);   // pelota vieja → invisible
    TEST_ASSERT_EQUAL_INT16(0, s.ball_x_mm);
    // El resto vivo:
    TEST_ASSERT_EQUAL_INT16(4500, s.my_heading_centideg);
    TEST_ASSERT_TRUE(s.flags & WS_FLAG_HEADING_VALID);
    TEST_ASSERT_EQUAL_UINT8(70, s.my_pose_confidence);
    TEST_ASSERT_EQUAL_UINT16(350, s.min_obstacle_mm);
}

// ============================================================================
// (3) Slot NUNCA publicado → slot_is_fresh=false → *_fresh=false → sentinela.
//     Blackboard zero-init (.bss) = arranque en frío, sin un solo publish.
// ============================================================================
void test_never_published_all_sentinel(void) {
    SensorBlackboard bb{};        // nada publicado
    const uint32_t now = 12345;

    const SnapshotInputs in = inputs_from_slots(bb, now, defaults());

    // Ningún slot fresco.
    TEST_ASSERT_FALSE(in.ball_fresh);
    TEST_ASSERT_FALSE(in.goal_opp_fresh);
    TEST_ASSERT_FALSE(in.goal_own_fresh);
    TEST_ASSERT_FALSE(in.heading_fresh);
    TEST_ASSERT_FALSE(in.pose_fresh);
    TEST_ASSERT_FALSE(in.obstacle_fresh);

    // El ensamblador → snapshot 100% sentinela.
    const WorldSnapshot s = assemble_snapshot(in);
    TEST_ASSERT_EQUAL_UINT8(0, s.ball_visible);
    TEST_ASSERT_EQUAL_UINT8(0, s.goal_opp_visible);
    TEST_ASSERT_EQUAL_UINT8(0, s.goal_own_visible);
    TEST_ASSERT_EQUAL_UINT8(0, s.my_pose_confidence);
    TEST_ASSERT_EQUAL_INT16(0, s.my_heading_centideg);
    TEST_ASSERT_FALSE(s.flags & WS_FLAG_HEADING_VALID);
    TEST_ASSERT_EQUAL_UINT16(WS_OBSTACLE_NONE, s.min_obstacle_mm);
}

// ============================================================================
// (4) WRAP de millis(): publicado justo ANTES del wrap, `now` justo DESPUÉS.
//     La resta UNSIGNED da el elapsed real (chico) → sigue fresco, NO sentinela.
// ============================================================================
void test_wrap_millis_stays_fresh(void) {
    SensorBlackboard bb{};
    const uint32_t publish_at = 0xFFFFFF00u;     // 256 ms antes del wrap
    publish_all(bb, publish_at);
    const uint32_t now = 0x0000005Au;            // 90 ms después del wrap (0x100 + 0x5A... )
    // elapsed = now - publish_at = 0x5A - (-0x100) = 0x100 + 0x5A = 346 ms.
    // Con umbral 1000 (cámara) sigue fresco; con 250 (ToF/heading/pose) ya venció.

    const SnapshotInputs in = inputs_from_slots(bb, now, defaults());

    // Cámara (umbral 1000) sobrevive al wrap: la resta unsigned NO marcó "viejo".
    TEST_ASSERT_TRUE(in.ball_fresh);
    TEST_ASSERT_TRUE(in.goal_opp_fresh);
    TEST_ASSERT_TRUE(in.goal_own_fresh);
    // ToF/heading/pose (umbral 250) sí vencieron (346 > 250), pero por el TIEMPO
    // real, no por un wrap espurio.
    TEST_ASSERT_FALSE(in.obstacle_fresh);
    TEST_ASSERT_FALSE(in.heading_fresh);
    TEST_ASSERT_FALSE(in.pose_fresh);

    // El dato de la pelota cruzó intacto el wrap.
    TEST_ASSERT_EQUAL_INT16(111, in.ball_x_mm);
}

// Wrap con umbral grande: publicado antes del wrap, now poco después, umbral lo
// cubre → TODOS frescos (verifica que NINGÚN slot se marca viejo por el wrap).
void test_wrap_millis_all_fresh_with_big_threshold(void) {
    SensorBlackboard bb{};
    publish_all(bb, 0xFFFFFFF0u);   // 16 ms antes del wrap
    const uint32_t now = 0x00000005u;  // 6 ms después del wrap → elapsed = 21 ms
    SlotThresholds big;             // todos defaults; 21 ms < 250 < 1000 → frescos

    const SnapshotInputs in = inputs_from_slots(bb, now, big);
    TEST_ASSERT_TRUE(in.ball_fresh);
    TEST_ASSERT_TRUE(in.goal_opp_fresh);
    TEST_ASSERT_TRUE(in.goal_own_fresh);
    TEST_ASSERT_TRUE(in.heading_fresh);
    TEST_ASSERT_TRUE(in.pose_fresh);
    TEST_ASSERT_TRUE(in.obstacle_fresh);
}

// ============================================================================
// (5) Borde exacto del umbral: elapsed == umbral → todavía FRESCO (corte es
//     > umbral, mismo criterio que slot_is_fresh / tof_fresh_or_no_reading).
// ============================================================================
void test_threshold_boundary_exact_is_fresh(void) {
    SensorBlackboard bb{};
    ObstObs o{}; o.mm = 350;
    slot_publish(bb.obstacle, o, 1000);
    const uint32_t now = 1250;   // elapsed = 250 == umbral ToF → fresco

    SnapshotInputs in = inputs_from_slots(bb, now, defaults());
    TEST_ASSERT_TRUE(in.obstacle_fresh);

    // Un ms más → vencido.
    in = inputs_from_slots(bb, 1251, defaults());
    TEST_ASSERT_FALSE(in.obstacle_fresh);
}

// ============================================================================
// (6) Consistencia de un solo `now`: dos slots publicados en instantes distintos
//     se juzgan AMBOS contra el mismo `now`. Verifica que el módulo no relee el
//     reloj por slot (lo cual partiría la coherencia temporal del snapshot).
// ============================================================================
void test_single_now_judges_all_slots(void) {
    SensorBlackboard bb{};
    // pelota fresca (publicada tarde), obstáculo viejo (publicado temprano).
    ObstObs o{}; o.mm = 200; slot_publish(bb.obstacle, o, 100);    // viejo
    BallObs b{}; b.x = 9; b.visible = true; slot_publish(bb.ball, b, 900);  // reciente
    const uint32_t now = 1000;
    // Contra ESTE now: obstáculo elapsed=900 (>250 → viejo), pelota elapsed=100 (<1000 → fresca).

    const SnapshotInputs in = inputs_from_slots(bb, now, defaults());
    TEST_ASSERT_FALSE(in.obstacle_fresh);
    TEST_ASSERT_TRUE(in.ball_fresh);
    TEST_ASSERT_EQUAL_INT16(9, in.ball_x_mm);
}

// ============================================================================
// (7) Validez intrínseca vs frescura: slot FRESCO pero el dato dice "no visible"
//     / "source inválido" → la frescura es true pero el visible/valid_src viaja
//     tal cual (el ensamblador respeta el visible intrínseco).
// ============================================================================
void test_fresh_but_not_visible_ball(void) {
    SensorBlackboard bb{};
    BallObs b{}; b.x = 50; b.visible = false;   // cámara fresca, sin pelota a la vista
    slot_publish(bb.ball, b, 1000);
    const uint32_t now = 1010;

    const SnapshotInputs in = inputs_from_slots(bb, now, defaults());
    TEST_ASSERT_TRUE(in.ball_fresh);     // el transporte está fresco
    TEST_ASSERT_FALSE(in.ball_visible);  // pero no hay pelota

    const WorldSnapshot s = assemble_snapshot(in);
    TEST_ASSERT_EQUAL_UINT8(0, s.ball_visible);
}

void test_fresh_heading_but_source_invalid(void) {
    SensorBlackboard bb{};
    HeadingObs h{}; h.centideg = 4500; h.valid_src = false;  // BNO congelado pero slot "fresco"
    slot_publish(bb.heading, h, 1000);
    const uint32_t now = 1100;   // dentro de 250 ms → fresco

    const SnapshotInputs in = inputs_from_slots(bb, now, defaults());
    TEST_ASSERT_TRUE(in.heading_fresh);
    TEST_ASSERT_FALSE(in.heading_valid_src);

    const WorldSnapshot s = assemble_snapshot(in);
    TEST_ASSERT_FALSE(s.flags & WS_FLAG_HEADING_VALID);   // bit off por valid_src
    TEST_ASSERT_EQUAL_INT16(4500, s.my_heading_centideg); // el valor igual viaja
}

// ============================================================================
// (8) Campos COMM pasan TAL CUAL (frescura resuelta aguas arriba; sin slot).
// ============================================================================
void test_comm_fields_pass_through(void) {
    SensorBlackboard bb{};        // ningún slot publicado…
    bb.referee_cmd         = 3;   // RESET
    bb.match_running       = true;
    bb.partner_alive       = true;
    bb.partner_sees_ball   = false;
    bb.in_own_penalty_area = true;
    const uint32_t now = 5000;

    const SnapshotInputs in = inputs_from_slots(bb, now, defaults());
    TEST_ASSERT_EQUAL_UINT8(3, in.referee_cmd);
    TEST_ASSERT_TRUE(in.match_running);
    TEST_ASSERT_TRUE(in.partner_alive);
    TEST_ASSERT_FALSE(in.partner_sees_ball);
    TEST_ASSERT_TRUE(in.in_own_penalty_area);

    // …y llegan al snapshot aunque los slots sensoriales estén en sentinela.
    const WorldSnapshot s = assemble_snapshot(in);
    TEST_ASSERT_EQUAL_UINT8(3, s.referee_cmd);
    TEST_ASSERT_TRUE(s.flags & WS_FLAG_MATCH_RUNNING);
    TEST_ASSERT_TRUE(s.flags & WS_FLAG_PARTNER_ALIVE);
    TEST_ASSERT_FALSE(s.flags & WS_FLAG_PARTNER_SEES_BALL);
    TEST_ASSERT_TRUE(s.flags & WS_FLAG_IN_OWN_PENALTY_AREA);
    TEST_ASSERT_EQUAL_UINT8(0, s.ball_visible);   // sensores en sentinela
}

// ============================================================================
// (9) INTEGRACIÓN end-to-end (el caso del contrato): blackboard con POSE vieja +
//     resto fresco → assemble_snapshot(inputs_from_slots(...)) deja la pose en
//     sentinela y TODO el resto vivo.
// ============================================================================
void test_integration_stale_pose_rest_alive(void) {
    SensorBlackboard bb{};
    // Todo fresco en t=2000…
    BallObs b{};   b.x = 111; b.y = 222; b.conf = 95; b.vx = 33; b.vy = -44; b.visible = true;
    slot_publish(bb.ball, b, 2000);
    GoalObs go{};  go.ang = 1500;  go.dist = 1800; go.visible = true; slot_publish(bb.goal_opp, go, 2000);
    GoalObs gp{};  gp.ang = -3000; gp.dist = 900;  gp.visible = true; slot_publish(bb.goal_own, gp, 2000);
    HeadingObs h{}; h.centideg = 4500; h.valid_src = true;            slot_publish(bb.heading, h, 2000);
    ObstObs o{};   o.mm = 350;                                        slot_publish(bb.obstacle, o, 2000);
    // …pero la POSE se publicó hace rato (t=1000), 1000 ms vieja, umbral 250 → muerta.
    PoseObs p{};   p.x = 500; p.y = -700; p.conf = 70; p.valid_src = true; slot_publish(bb.pose, p, 1000);

    bb.referee_cmd = 1; bb.match_running = true; bb.partner_alive = true;

    const uint32_t now = 2010;   // ball/goal/heading/obst elapsed=10ms; pose elapsed=1010ms

    const WorldSnapshot s = assemble_snapshot(inputs_from_slots(bb, now, defaults()));

    // POSE colapsada (sentinela): confidence 0, x/y 0.
    TEST_ASSERT_EQUAL_UINT8(0, s.my_pose_confidence);
    TEST_ASSERT_EQUAL_INT16(0, s.my_x_mm);
    TEST_ASSERT_EQUAL_INT16(0, s.my_y_mm);

    // TODO el resto VIVO con su dato real:
    TEST_ASSERT_EQUAL_UINT8(1,   s.ball_visible);
    TEST_ASSERT_EQUAL_INT16(111, s.ball_x_mm);
    TEST_ASSERT_EQUAL_INT16(222, s.ball_y_mm);
    TEST_ASSERT_EQUAL_UINT8(95,  s.ball_confidence);
    TEST_ASSERT_EQUAL_UINT8(1,    s.goal_opp_visible);
    TEST_ASSERT_EQUAL_INT16(1500, s.goal_opp_angle_centideg);
    TEST_ASSERT_EQUAL_UINT8(1,     s.goal_own_visible);
    TEST_ASSERT_EQUAL_INT16(-3000, s.goal_own_angle_centideg);
    TEST_ASSERT_EQUAL_INT16(4500, s.my_heading_centideg);
    TEST_ASSERT_TRUE(s.flags & WS_FLAG_HEADING_VALID);
    TEST_ASSERT_EQUAL_UINT16(350, s.min_obstacle_mm);
    TEST_ASSERT_EQUAL_UINT8(1, s.referee_cmd);
    TEST_ASSERT_TRUE(s.flags & WS_FLAG_MATCH_RUNNING);
}

// ============================================================================
// (10) Republicar refresca: un slot viejo vuelve a estar fresco tras un publish
//      nuevo (la frescura sigue el ÚLTIMO publish, no el primero).
// ============================================================================
void test_republish_refreshes(void) {
    SensorBlackboard bb{};
    ObstObs o1{}; o1.mm = 100; slot_publish(bb.obstacle, o1, 1000);
    // En t=2000, el slot (umbral 250) estaría muerto…
    SnapshotInputs in = inputs_from_slots(bb, 2000, defaults());
    TEST_ASSERT_FALSE(in.obstacle_fresh);
    // …pero re-publico en t=2000 con dato nuevo → vuelve fresco y trae el dato nuevo.
    ObstObs o2{}; o2.mm = 80; slot_publish(bb.obstacle, o2, 2000);
    in = inputs_from_slots(bb, 2010, defaults());
    TEST_ASSERT_TRUE(in.obstacle_fresh);
    TEST_ASSERT_EQUAL_UINT16(80, in.min_obstacle_mm);
}

// ============================================================================
// (11) Umbrales custom (banco): el caller puede apretar/aflojar por slot.
// ============================================================================
void test_custom_thresholds(void) {
    SensorBlackboard bb{};
    BallObs b{}; b.x = 1; b.visible = true; slot_publish(bb.ball, b, 1000);
    const uint32_t now = 1100;   // elapsed = 100 ms

    SlotThresholds tight;
    tight.ball_ms = 50;          // apretado: 100 > 50 → no fresco
    SnapshotInputs in = inputs_from_slots(bb, now, tight);
    TEST_ASSERT_FALSE(in.ball_fresh);

    SlotThresholds loose;
    loose.ball_ms = 5000;        // aflojado: 100 < 5000 → fresco
    in = inputs_from_slots(bb, now, loose);
    TEST_ASSERT_TRUE(in.ball_fresh);
}

int main(int, char**) {
    UNITY_BEGIN();
    // (1) fresco → dato pasa
    RUN_TEST(test_fresh_slot_passes_data);
    // (2) viejo → sentinela, resto vive
    RUN_TEST(test_stale_obstacle_marks_not_fresh_and_collapses);
    RUN_TEST(test_stale_ball_only_collapses_ball);
    // (3) nunca publicado → sentinela total
    RUN_TEST(test_never_published_all_sentinel);
    // (4) wrap de millis
    RUN_TEST(test_wrap_millis_stays_fresh);
    RUN_TEST(test_wrap_millis_all_fresh_with_big_threshold);
    // (5) borde exacto del umbral
    RUN_TEST(test_threshold_boundary_exact_is_fresh);
    // (6) un solo `now` para todos los slots
    RUN_TEST(test_single_now_judges_all_slots);
    // (7) validez intrínseca vs frescura
    RUN_TEST(test_fresh_but_not_visible_ball);
    RUN_TEST(test_fresh_heading_but_source_invalid);
    // (8) COMM tal cual
    RUN_TEST(test_comm_fields_pass_through);
    // (9) integración end-to-end
    RUN_TEST(test_integration_stale_pose_rest_alive);
    // (10) republicar refresca
    RUN_TEST(test_republish_refreshes);
    // (11) umbrales custom
    RUN_TEST(test_custom_thresholds);
    return UNITY_END();
}
