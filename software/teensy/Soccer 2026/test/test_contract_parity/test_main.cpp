// test_contract_parity - PARIDAD DE CONTRATO: el camino de la PIZARRA no cambia el CABLE.
//
// Que prueba
// ----------
// El rediseno RT mueve el armado del WorldSnapshot del loop (build_snapshot, HW-bound) a
// la ISR del emisor, que arma con inputs_from_slots(blackboard) -> assemble_snapshot(...).
// Este test garantiza que ESE camino produce, para un estado dado, un WorldSnapshot
// BYTE-IDENTICO al que dicta el CONTRATO del struct (types.h) construido A MANO. Como el
// snapshot viaja por wire con un memcpy crudo del struct (comm_central.cpp:
// `memcpy(f.payload, &snap, sizeof(WorldSnapshot))`), byte-identidad del struct ==
// byte-identidad en el cable. Comparamos los 31 bytes con memcmp.
//
// build_snapshot() es HW-bound (lee sensors_*, cameras_*, comm_*), no se puede linkear
// host. Por eso el "golden" es un WorldSnapshot rellenado a mano con la convencion del
// contrato (mismos campos/sentinelas que escribe build_snapshot para ese estado). Si un
// dia este golden y assemble_snapshot divergen, o cambia sizeof, este test FALLA => alguien
// rompio el contrato al mover a la pizarra. Fuente de verdad: types.h + assemble_snapshot.
//
// Corre host (sin Arduino):
//   bash scripts/run-host-tests.sh test_contract_parity

#include <unity.h>
#include <string.h>   // memcmp
#include "snapshot_assembler.h"   // SnapshotInputs, assemble_snapshot, WS_* (PURO)
#include "types.h"                // WorldSnapshot, sizeof==31

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// (0) El contrato de tamano es la base de todo: sin 31 bytes, el cable cambio.
// ---------------------------------------------------------------------------
void test_sizeof_is_31(void) {
    TEST_ASSERT_EQUAL_UINT32(31u, (uint32_t)sizeof(WorldSnapshot));
}

// GOLDEN construido a mano segun el contrato (types.h), estado "todo presente y valido".
static WorldSnapshot golden_all_present(void) {
    WorldSnapshot g{};   // value-init -> sentinelas base en 0
    g.my_x_mm             = 500;
    g.my_y_mm             = -700;
    g.my_heading_centideg = 4500;
    g.my_pose_confidence  = 70;
    g.ball_x_mm       = 111;
    g.ball_y_mm       = 222;
    g.ball_visible    = 1;
    g.ball_confidence = 95;
    g.ball_vx_mm_s    = 33;
    g.ball_vy_mm_s    = -44;
    g.goal_opp_angle_centideg = 1500;
    g.goal_opp_distance_mm    = 1800;
    g.goal_opp_visible        = 1;
    g.goal_own_visible        = 1;
    g.goal_own_angle_centideg = -3000;
    g.goal_own_distance_mm    = 900;
    g.min_obstacle_mm = 350;
    g.referee_cmd = 1;
    g.flags = WS_FLAG_IN_OWN_PENALTY_AREA | WS_FLAG_PARTNER_ALIVE
            | WS_FLAG_MATCH_RUNNING | WS_FLAG_HEADING_VALID;
    return g;
}

// Inputs de la pizarra que, ensamblados, deben dar EXACTO el golden de arriba.
static SnapshotInputs inputs_all_present(void) {
    SnapshotInputs in{};
    in.ball_fresh = true;  in.ball_visible = true;
    in.ball_x_mm = 111; in.ball_y_mm = 222; in.ball_confidence = 95;
    in.ball_vx_mm_s = 33; in.ball_vy_mm_s = -44;
    in.goal_opp_fresh = true; in.goal_opp_visible = true;
    in.goal_opp_angle_centideg = 1500; in.goal_opp_distance_mm = 1800;
    in.goal_own_fresh = true; in.goal_own_visible = true;
    in.goal_own_angle_centideg = -3000; in.goal_own_distance_mm = 900;
    in.heading_fresh = true; in.heading_valid_src = true; in.heading_centideg = 4500;
    in.pose_fresh = true; in.pose_valid_src = true;
    in.pose_x_mm = 500; in.pose_y_mm = -700; in.pose_confidence = 70;
    in.obstacle_fresh = true; in.min_obstacle_mm = 350;
    in.referee_cmd = 1;
    in.match_running = true; in.partner_alive = true; in.partner_sees_ball = false;
    in.in_own_penalty_area = true;
    return in;
}

// ---------------------------------------------------------------------------
// (1) PARIDAD: assemble_snapshot(inputs_all_present) == golden, byte a byte (31).
//     Es el corazon: el camino de la pizarra NO muda el cable.
// ---------------------------------------------------------------------------
void test_assemble_matches_golden_byte_for_byte(void) {
    const WorldSnapshot got    = assemble_snapshot(inputs_all_present());
    const WorldSnapshot golden = golden_all_present();
    TEST_ASSERT_EQUAL_INT(0, memcmp(&got, &golden, sizeof(WorldSnapshot)));
}

// ---------------------------------------------------------------------------
// (1b) Mismo caso campo por campo: si (1) falla, esto localiza el byte roto.
// ---------------------------------------------------------------------------
void test_assemble_matches_golden_fieldwise(void) {
    const WorldSnapshot s = assemble_snapshot(inputs_all_present());
    TEST_ASSERT_EQUAL_INT16(500,   s.my_x_mm);
    TEST_ASSERT_EQUAL_INT16(-700,  s.my_y_mm);
    TEST_ASSERT_EQUAL_INT16(4500,  s.my_heading_centideg);
    TEST_ASSERT_EQUAL_UINT8(70,    s.my_pose_confidence);
    TEST_ASSERT_EQUAL_INT16(111,   s.ball_x_mm);
    TEST_ASSERT_EQUAL_INT16(222,   s.ball_y_mm);
    TEST_ASSERT_EQUAL_UINT8(1,     s.ball_visible);
    TEST_ASSERT_EQUAL_UINT8(95,    s.ball_confidence);
    TEST_ASSERT_EQUAL_INT16(33,    s.ball_vx_mm_s);
    TEST_ASSERT_EQUAL_INT16(-44,   s.ball_vy_mm_s);
    TEST_ASSERT_EQUAL_INT16(1500,  s.goal_opp_angle_centideg);
    TEST_ASSERT_EQUAL_INT16(1800,  s.goal_opp_distance_mm);
    TEST_ASSERT_EQUAL_UINT8(1,     s.goal_opp_visible);
    TEST_ASSERT_EQUAL_UINT8(1,     s.goal_own_visible);
    TEST_ASSERT_EQUAL_INT16(-3000, s.goal_own_angle_centideg);
    TEST_ASSERT_EQUAL_INT16(900,   s.goal_own_distance_mm);
    TEST_ASSERT_EQUAL_UINT16(350,  s.min_obstacle_mm);
    TEST_ASSERT_EQUAL_UINT8(1,     s.referee_cmd);
    TEST_ASSERT_EQUAL_HEX8(
        WS_FLAG_IN_OWN_PENALTY_AREA | WS_FLAG_PARTNER_ALIVE
        | WS_FLAG_MATCH_RUNNING | WS_FLAG_HEADING_VALID, s.flags);
}

// ---------------------------------------------------------------------------
// (2) GOLDEN de SENTINELA: estado "nada fresco" => el contrato dice TODO en
//     sentinela (value-init salvo min_obstacle_mm = 0xFFFF). Caso LOOP-MUERTO /
//     arranque en frio que CENTRAL debe leer como "ciego".
// ---------------------------------------------------------------------------
void test_all_sentinel_matches_golden(void) {
    SnapshotInputs in{};   // todo false/0 -> nada fresco, nada visible
    const WorldSnapshot got = assemble_snapshot(in);

    WorldSnapshot golden{};                       // value-init = todos 0
    golden.min_obstacle_mm = WS_OBSTACLE_NONE;    // unico sentinela != 0 (0xFFFF = libre)

    TEST_ASSERT_EQUAL_INT(0, memcmp(&got, &golden, sizeof(WorldSnapshot)));
    TEST_ASSERT_EQUAL_UINT8(0,  got.ball_visible);
    TEST_ASSERT_EQUAL_UINT8(0,  got.goal_opp_visible);
    TEST_ASSERT_EQUAL_UINT8(0,  got.goal_own_visible);
    TEST_ASSERT_EQUAL_UINT8(0,  got.my_pose_confidence);
    TEST_ASSERT_EQUAL_INT16(0,  got.my_heading_centideg);
    TEST_ASSERT_EQUAL_UINT16(WS_OBSTACLE_NONE, got.min_obstacle_mm);
    TEST_ASSERT_FALSE(got.flags & WS_FLAG_HEADING_VALID);
}

// ---------------------------------------------------------------------------
// (3) memcmp realmente cubre los 31 bytes (packed sin huecos): un byte distinto
//     DEBE diferir.
// ---------------------------------------------------------------------------
void test_memcmp_detects_single_byte_diff(void) {
    const WorldSnapshot a = assemble_snapshot(inputs_all_present());
    WorldSnapshot b = a;
    b.flags ^= 0x01;
    TEST_ASSERT_NOT_EQUAL(0, memcmp(&a, &b, sizeof(WorldSnapshot)));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sizeof_is_31);
    RUN_TEST(test_assemble_matches_golden_byte_for_byte);
    RUN_TEST(test_assemble_matches_golden_fieldwise);
    RUN_TEST(test_all_sentinel_matches_golden);
    RUN_TEST(test_memcmp_detects_single_byte_diff);
    return UNITY_END();
}
