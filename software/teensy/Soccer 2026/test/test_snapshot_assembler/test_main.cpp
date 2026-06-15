// test_snapshot_assembler — TDD del ENSAMBLADOR PURO del WorldSnapshot.
//
// Cubre src/shared/snapshot_assembler.h: dado un SnapshotInputs (valores ya
// leídos de cada slot + su flag de frescura), assemble_snapshot() arma el
// WorldSnapshot aplicando el FAIL-SAFE por frescura — un slot viejo colapsa a su
// SENTINELA (no propaga dato muerto) y el resto sobrevive.
//
// Corre host (sin Arduino) con:
//   bash scripts/run-host-tests.sh test_snapshot_assembler
//
// run-host-tests.sh compila con -I src/shared, así que el header se incluye por
// nombre. El módulo es puro (solo <stdint.h> + types.h) → no linkea Arduino.

#include <unity.h>
#include "snapshot_assembler.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ----------------------------------------------------------------------------
// Helper: un SnapshotInputs con TODOS los slots frescos, válidos y con datos
// distinguibles (valores "marcadores" para verificar que pasan tal cual).
// Cada test parte de esto y degrada un solo slot.
// ----------------------------------------------------------------------------
static SnapshotInputs all_fresh_inputs(void) {
    SnapshotInputs in{};

    in.ball_fresh        = true;
    in.ball_visible      = true;
    in.ball_x_mm         = 111;
    in.ball_y_mm         = 222;
    in.ball_confidence   = 95;
    in.ball_vx_mm_s      = 33;
    in.ball_vy_mm_s      = -44;

    in.goal_opp_fresh        = true;
    in.goal_opp_visible      = true;
    in.goal_opp_angle_centideg = 1500;   // 15°
    in.goal_opp_distance_mm    = 1800;

    in.goal_own_fresh        = true;
    in.goal_own_visible      = true;
    in.goal_own_angle_centideg = -3000;  // -30°
    in.goal_own_distance_mm    = 900;

    in.heading_fresh     = true;
    in.heading_valid_src = true;
    in.heading_centideg  = 4500;          // 45°

    in.pose_fresh        = true;
    in.pose_valid_src    = true;
    in.pose_x_mm         = 500;
    in.pose_y_mm         = -700;
    in.pose_confidence   = 70;

    in.obstacle_fresh    = true;
    in.min_obstacle_mm   = 350;

    in.referee_cmd       = 1;             // START
    in.match_running     = true;
    in.partner_alive     = true;
    in.partner_sees_ball = true;
    in.in_own_penalty_area = true;

    return in;
}

// ============================================================================
// (1) TODOS FRESCOS → snapshot completo (todos los datos pasan tal cual)
// ============================================================================

void test_all_fresh_full_snapshot(void) {
    const WorldSnapshot s = assemble_snapshot(all_fresh_inputs());

    // Pose
    TEST_ASSERT_EQUAL_INT16(500,  s.my_x_mm);
    TEST_ASSERT_EQUAL_INT16(-700, s.my_y_mm);
    TEST_ASSERT_EQUAL_UINT8(70,   s.my_pose_confidence);

    // Heading
    TEST_ASSERT_EQUAL_INT16(4500, s.my_heading_centideg);

    // Pelota
    TEST_ASSERT_EQUAL_UINT8(1,   s.ball_visible);
    TEST_ASSERT_EQUAL_INT16(111, s.ball_x_mm);
    TEST_ASSERT_EQUAL_INT16(222, s.ball_y_mm);
    TEST_ASSERT_EQUAL_UINT8(95,  s.ball_confidence);
    TEST_ASSERT_EQUAL_INT16(33,  s.ball_vx_mm_s);
    TEST_ASSERT_EQUAL_INT16(-44, s.ball_vy_mm_s);

    // Arco rival
    TEST_ASSERT_EQUAL_UINT8(1,    s.goal_opp_visible);
    TEST_ASSERT_EQUAL_INT16(1500, s.goal_opp_angle_centideg);
    TEST_ASSERT_EQUAL_INT16(1800, s.goal_opp_distance_mm);

    // Arco propio
    TEST_ASSERT_EQUAL_UINT8(1,     s.goal_own_visible);
    TEST_ASSERT_EQUAL_INT16(-3000, s.goal_own_angle_centideg);
    TEST_ASSERT_EQUAL_INT16(900,   s.goal_own_distance_mm);

    // Obstáculo
    TEST_ASSERT_EQUAL_UINT16(350, s.min_obstacle_mm);

    // Árbitro + flags
    TEST_ASSERT_EQUAL_UINT8(1, s.referee_cmd);
    TEST_ASSERT_TRUE(s.flags & WS_FLAG_IN_OWN_PENALTY_AREA);
    TEST_ASSERT_TRUE(s.flags & WS_FLAG_PARTNER_ALIVE);
    TEST_ASSERT_TRUE(s.flags & WS_FLAG_PARTNER_SEES_BALL);
    TEST_ASSERT_TRUE(s.flags & WS_FLAG_MATCH_RUNNING);
    TEST_ASSERT_TRUE(s.flags & WS_FLAG_HEADING_VALID);
}

// ============================================================================
// (2) UN SLOT VIEJO → ese campo cae a su sentinela; el RESTO sobrevive
// ============================================================================

// --- Pelota vieja → ball_visible=0 + todo el bloque pelota a 0; el resto vive.
void test_stale_ball_collapses_only_ball(void) {
    SnapshotInputs in = all_fresh_inputs();
    in.ball_fresh = false;
    const WorldSnapshot s = assemble_snapshot(in);

    // Pelota colapsada
    TEST_ASSERT_EQUAL_UINT8(0, s.ball_visible);
    TEST_ASSERT_EQUAL_INT16(0, s.ball_x_mm);
    TEST_ASSERT_EQUAL_INT16(0, s.ball_y_mm);
    TEST_ASSERT_EQUAL_UINT8(0, s.ball_confidence);
    TEST_ASSERT_EQUAL_INT16(0, s.ball_vx_mm_s);
    TEST_ASSERT_EQUAL_INT16(0, s.ball_vy_mm_s);

    // El resto SOBREVIVE
    TEST_ASSERT_EQUAL_INT16(4500, s.my_heading_centideg);
    TEST_ASSERT_EQUAL_UINT8(70,   s.my_pose_confidence);
    TEST_ASSERT_EQUAL_UINT8(1,    s.goal_opp_visible);
    TEST_ASSERT_EQUAL_UINT8(1,    s.goal_own_visible);
    TEST_ASSERT_EQUAL_UINT16(350, s.min_obstacle_mm);
    TEST_ASSERT_TRUE(s.flags & WS_FLAG_HEADING_VALID);
}

// --- Arco rival viejo → goal_opp_visible=0 + angle/distance N/A (0); resto vive.
void test_stale_goal_opp_collapses_only_goal_opp(void) {
    SnapshotInputs in = all_fresh_inputs();
    in.goal_opp_fresh = false;
    const WorldSnapshot s = assemble_snapshot(in);

    TEST_ASSERT_EQUAL_UINT8(0, s.goal_opp_visible);
    TEST_ASSERT_EQUAL_INT16(0, s.goal_opp_angle_centideg);
    TEST_ASSERT_EQUAL_INT16(0, s.goal_opp_distance_mm);

    // El arco PROPIO y la pelota siguen vivos.
    TEST_ASSERT_EQUAL_UINT8(1,     s.goal_own_visible);
    TEST_ASSERT_EQUAL_INT16(-3000, s.goal_own_angle_centideg);
    TEST_ASSERT_EQUAL_UINT8(1,     s.ball_visible);
}

// --- Arco propio viejo → goal_own_visible=0; el rival sobrevive (independencia).
void test_stale_goal_own_collapses_only_goal_own(void) {
    SnapshotInputs in = all_fresh_inputs();
    in.goal_own_fresh = false;
    const WorldSnapshot s = assemble_snapshot(in);

    TEST_ASSERT_EQUAL_UINT8(0, s.goal_own_visible);
    TEST_ASSERT_EQUAL_INT16(0, s.goal_own_angle_centideg);
    TEST_ASSERT_EQUAL_INT16(0, s.goal_own_distance_mm);

    TEST_ASSERT_EQUAL_UINT8(1,    s.goal_opp_visible);
    TEST_ASSERT_EQUAL_INT16(1500, s.goal_opp_angle_centideg);
}

// --- Pose vieja → my_pose_confidence=0 + x/y=0; heading (otro slot) sobrevive.
void test_stale_pose_collapses_only_pose(void) {
    SnapshotInputs in = all_fresh_inputs();
    in.pose_fresh = false;
    const WorldSnapshot s = assemble_snapshot(in);

    TEST_ASSERT_EQUAL_UINT8(0, s.my_pose_confidence);
    TEST_ASSERT_EQUAL_INT16(0, s.my_x_mm);
    TEST_ASSERT_EQUAL_INT16(0, s.my_y_mm);

    // El heading NO depende de la pose: sigue vivo y válido.
    TEST_ASSERT_EQUAL_INT16(4500, s.my_heading_centideg);
    TEST_ASSERT_TRUE(s.flags & WS_FLAG_HEADING_VALID);
}

// --- Obstáculo viejo → min_obstacle_mm = NONE (0xFFFF), NO pared fantasma.
void test_stale_obstacle_collapses_to_none(void) {
    SnapshotInputs in = all_fresh_inputs();
    in.obstacle_fresh = false;
    const WorldSnapshot s = assemble_snapshot(in);

    TEST_ASSERT_EQUAL_UINT16(WS_OBSTACLE_NONE, s.min_obstacle_mm);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF,           s.min_obstacle_mm);

    // El resto sigue.
    TEST_ASSERT_EQUAL_UINT8(1, s.ball_visible);
    TEST_ASSERT_EQUAL_INT16(4500, s.my_heading_centideg);
}

// ============================================================================
// (3) heading_valid refleja la FRESCURA del rumbo
// ============================================================================

// Rumbo FRESCO + válido → bit heading_valid en 1; el valor del BNO pasa.
void test_heading_fresh_valid_sets_flag_and_value(void) {
    SnapshotInputs in = all_fresh_inputs();
    const WorldSnapshot s = assemble_snapshot(in);
    TEST_ASSERT_TRUE(s.flags & WS_FLAG_HEADING_VALID);
    TEST_ASSERT_EQUAL_INT16(4500, s.my_heading_centideg);
}

// Rumbo NO fresco → bit heading_valid en 0, y el valor cae a 0 (rumbo neutro):
// NO se propaga el yaw viejo, y CENTRAL sabe (por el bit) que no debe confiar.
void test_heading_stale_clears_flag_and_zeroes_value(void) {
    SnapshotInputs in = all_fresh_inputs();
    in.heading_fresh = false;
    const WorldSnapshot s = assemble_snapshot(in);
    TEST_ASSERT_FALSE(s.flags & WS_FLAG_HEADING_VALID);
    TEST_ASSERT_EQUAL_INT16(0, s.my_heading_centideg);

    // Sólo el heading se afecta: la pelota y los arcos siguen vivos.
    TEST_ASSERT_EQUAL_UINT8(1, s.ball_visible);
    TEST_ASSERT_EQUAL_UINT8(1, s.goal_opp_visible);
}

// Rumbo FRESCO pero el BNO se reporta inválido en vivo (fused_valid=false:
// el único BNO sano se congeló/cayó) → el valor llega pero el bit queda en 0.
void test_heading_fresh_but_source_invalid_clears_flag(void) {
    SnapshotInputs in = all_fresh_inputs();
    in.heading_valid_src = false;
    const WorldSnapshot s = assemble_snapshot(in);
    TEST_ASSERT_FALSE(s.flags & WS_FLAG_HEADING_VALID);
    // El campo igual viaja (CENTRAL lo consume sin gatear por confidence; el bit
    // es lo que le dice que no confíe).
    TEST_ASSERT_EQUAL_INT16(4500, s.my_heading_centideg);
}

// ============================================================================
// EXTRA: un slot fresco pero NO visible (cámara fresca, sin pelota) → visible=0.
// La frescura del transporte ≠ "hay dato"; respetamos el visible intrínseco.
// ============================================================================

void test_fresh_but_not_visible_ball_stays_invisible(void) {
    SnapshotInputs in = all_fresh_inputs();
    in.ball_visible = false;   // cámara fresca pero no ve la pelota
    const WorldSnapshot s = assemble_snapshot(in);
    TEST_ASSERT_EQUAL_UINT8(0, s.ball_visible);
    TEST_ASSERT_EQUAL_INT16(0, s.ball_x_mm);
}

// ============================================================================
// EXTRA: pose fresca de transporte pero inválida intrínsecamente (p.ej. <2 TOFs)
// → confidence=0 (CENTRAL ignora la posición).
// ============================================================================

void test_pose_fresh_but_source_invalid_zeroes_confidence(void) {
    SnapshotInputs in = all_fresh_inputs();
    in.pose_valid_src = false;
    const WorldSnapshot s = assemble_snapshot(in);
    TEST_ASSERT_EQUAL_UINT8(0, s.my_pose_confidence);
    TEST_ASSERT_EQUAL_INT16(0, s.my_x_mm);
    TEST_ASSERT_EQUAL_INT16(0, s.my_y_mm);
}

// ============================================================================
// EXTRA: TODOS los slots viejos → snapshot 100% en sentinelas (fail-safe total).
// Ningún dato muerto sobrevive; flags de sensores en 0.
// ============================================================================

void test_all_stale_full_fail_safe(void) {
    SnapshotInputs in = all_fresh_inputs();
    in.ball_fresh = in.goal_opp_fresh = in.goal_own_fresh = false;
    in.heading_fresh = in.pose_fresh = in.obstacle_fresh = false;
    const WorldSnapshot s = assemble_snapshot(in);

    TEST_ASSERT_EQUAL_UINT8(0, s.ball_visible);
    TEST_ASSERT_EQUAL_UINT8(0, s.goal_opp_visible);
    TEST_ASSERT_EQUAL_UINT8(0, s.goal_own_visible);
    TEST_ASSERT_EQUAL_UINT8(0, s.my_pose_confidence);
    TEST_ASSERT_EQUAL_INT16(0, s.my_heading_centideg);
    TEST_ASSERT_FALSE(s.flags & WS_FLAG_HEADING_VALID);
    TEST_ASSERT_EQUAL_UINT16(WS_OBSTACLE_NONE, s.min_obstacle_mm);

    // Los flags de COMM (no son slots sensoriales) sí siguen reflejando su input.
    TEST_ASSERT_TRUE(s.flags & WS_FLAG_MATCH_RUNNING);
    TEST_ASSERT_TRUE(s.flags & WS_FLAG_PARTNER_ALIVE);
}

// ============================================================================
// EXTRA: el contrato de tamaño del WorldSnapshot no se rompió (sanity v3).
// ============================================================================

void test_snapshot_size_contract(void) {
    TEST_ASSERT_EQUAL_UINT32(31u, (uint32_t)sizeof(WorldSnapshot));
}

int main(int, char**) {
    UNITY_BEGIN();
    // (1) todos frescos
    RUN_TEST(test_all_fresh_full_snapshot);
    // (2) un slot viejo → cae a sentinela, el resto sobrevive
    RUN_TEST(test_stale_ball_collapses_only_ball);
    RUN_TEST(test_stale_goal_opp_collapses_only_goal_opp);
    RUN_TEST(test_stale_goal_own_collapses_only_goal_own);
    RUN_TEST(test_stale_pose_collapses_only_pose);
    RUN_TEST(test_stale_obstacle_collapses_to_none);
    // (3) heading_valid refleja la frescura del rumbo
    RUN_TEST(test_heading_fresh_valid_sets_flag_and_value);
    RUN_TEST(test_heading_stale_clears_flag_and_zeroes_value);
    RUN_TEST(test_heading_fresh_but_source_invalid_clears_flag);
    // extras
    RUN_TEST(test_fresh_but_not_visible_ball_stays_invisible);
    RUN_TEST(test_pose_fresh_but_source_invalid_zeroes_confidence);
    RUN_TEST(test_all_stale_full_fail_safe);
    RUN_TEST(test_snapshot_size_contract);
    return UNITY_END();
}
