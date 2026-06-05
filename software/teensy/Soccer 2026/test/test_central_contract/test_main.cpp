// test_central_contract — pio test -e test_native -f test_central_contract
#include <unity.h>
#include <cstring>   // memcpy
#include <cstddef>   // offsetof
#include "types.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

void test_worldsnapshot_has_ball_velocity_fields(void){
    WorldSnapshot w{};
    w.ball_vx_mm_s = -1234;
    w.ball_vy_mm_s = 567;
    TEST_ASSERT_EQUAL_INT16(-1234, w.ball_vx_mm_s);
    TEST_ASSERT_EQUAL_INT16(567, w.ball_vy_mm_s);
}
void test_worldsnapshot_size_is_31(void){
    TEST_ASSERT_EQUAL_UINT32(31, sizeof(WorldSnapshot));
}
void test_worldsnapshot_has_goal_own_fields(void){
    WorldSnapshot w{};
    w.goal_own_visible = 1;
    w.goal_own_angle_centideg = -4321;
    w.goal_own_distance_mm = 1234;
    TEST_ASSERT_EQUAL_UINT8(1, w.goal_own_visible);
    TEST_ASSERT_EQUAL_INT16(-4321, w.goal_own_angle_centideg);
    TEST_ASSERT_EQUAL_INT16(1234, w.goal_own_distance_mm);
}
void test_worldsnapshot_heading_valid_flag_bit4(void){
    WorldSnapshot w{};
    w.flags = 0x10;  // bit 4 = heading_valid
    TEST_ASSERT_TRUE((w.flags & 0x10) != 0);
    TEST_ASSERT_EQUAL_UINT8(0, (w.flags & 0xE0));  // bits 5-7 reservados = 0
}

// ── Golden round-trip del WorldSnapshot v3 ──────────────────────────────────
// El snapshot cruza TOP→CENTRAL por memcpy crudo del struct packed (31 B). El
// static_assert(sizeof==31) atrapa cambios de TAMAÑO pero NO un reordenamiento
// de campos del mismo tamaño (swap ball_x/ball_y, goal_opp/goal_own, etc.).
// Estos tests siembran un valor único por campo, hacen memcpy ida-y-vuelta y
// pinean offsets concretos en el buffer, de modo que cualquier reorder dispare.

// Reconstruye un int16 little-endian desde dos bytes del buffer. TOP y CENTRAL
// son ambos Teensy (ARM little-endian) y el host de test es x86 (little-endian),
// así que el orden de bytes del wire coincide con el del struct en memoria.
static int16_t le_i16(const uint8_t* p){
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint16_t le_u16(const uint8_t* p){
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

// Siembra TODOS los campos con valores únicos y reconocibles.
static WorldSnapshot make_seeded(void){
    WorldSnapshot w{};
    w.my_x_mm                 = 1111;
    w.my_y_mm                 = 2222;
    w.my_heading_centideg     = -3333;
    w.my_pose_confidence      = 77;
    w.ball_x_mm               = 4444;
    w.ball_y_mm               = -5555;
    w.ball_visible            = 1;
    w.ball_confidence         = 88;
    w.ball_vx_mm_s            = -1234;
    w.ball_vy_mm_s            = 1357;
    w.goal_opp_angle_centideg = 6000;
    w.goal_opp_distance_mm    = 7000;
    w.goal_opp_visible        = 1;
    w.goal_own_visible        = 1;
    w.goal_own_angle_centideg = -4321;
    w.goal_own_distance_mm    = 8000;
    w.min_obstacle_mm         = 9000;
    w.referee_cmd             = 3;
    w.flags                   = 0x10;
    return w;
}

void test_worldsnapshot_v3_byte_roundtrip(void){
    WorldSnapshot w = make_seeded();
    uint8_t buf[31];
    TEST_ASSERT_EQUAL_UINT32(sizeof(buf), sizeof(w));  // 31 = 31
    memcpy(buf, &w, sizeof(w));
    WorldSnapshot w2{};
    memcpy(&w2, buf, sizeof(w2));
    // Campo por campo: el round-trip debe preservar TODO.
    TEST_ASSERT_EQUAL_INT16(1111,  w2.my_x_mm);
    TEST_ASSERT_EQUAL_INT16(2222,  w2.my_y_mm);
    TEST_ASSERT_EQUAL_INT16(-3333, w2.my_heading_centideg);
    TEST_ASSERT_EQUAL_UINT8(77,    w2.my_pose_confidence);
    TEST_ASSERT_EQUAL_INT16(4444,  w2.ball_x_mm);
    TEST_ASSERT_EQUAL_INT16(-5555, w2.ball_y_mm);
    TEST_ASSERT_EQUAL_UINT8(1,     w2.ball_visible);
    TEST_ASSERT_EQUAL_UINT8(88,    w2.ball_confidence);
    TEST_ASSERT_EQUAL_INT16(-1234, w2.ball_vx_mm_s);
    TEST_ASSERT_EQUAL_INT16(1357,  w2.ball_vy_mm_s);
    TEST_ASSERT_EQUAL_INT16(6000,  w2.goal_opp_angle_centideg);
    TEST_ASSERT_EQUAL_INT16(7000,  w2.goal_opp_distance_mm);
    TEST_ASSERT_EQUAL_UINT8(1,     w2.goal_opp_visible);
    TEST_ASSERT_EQUAL_UINT8(1,     w2.goal_own_visible);
    TEST_ASSERT_EQUAL_INT16(-4321, w2.goal_own_angle_centideg);
    TEST_ASSERT_EQUAL_INT16(8000,  w2.goal_own_distance_mm);
    TEST_ASSERT_EQUAL_UINT16(9000, w2.min_obstacle_mm);
    TEST_ASSERT_EQUAL_UINT8(3,     w2.referee_cmd);
    TEST_ASSERT_EQUAL_UINT8(0x10,  w2.flags);
}

// GOLDEN de offsets: fija el byte/offset de cada campo. Un swap de campos del
// mismo tamaño (p.ej. ball_x↔ball_y, o goal_opp_angle↔goal_own_angle) NO cambia
// sizeof pero SÍ rompe estos offsets → el test falla y delata el reorder.
void test_worldsnapshot_v3_golden_offsets(void){
    // 1) Offsets esperados del contrato v3 (packed, sin padding).
    TEST_ASSERT_EQUAL_UINT32(0,  offsetof(WorldSnapshot, my_x_mm));
    TEST_ASSERT_EQUAL_UINT32(2,  offsetof(WorldSnapshot, my_y_mm));
    TEST_ASSERT_EQUAL_UINT32(4,  offsetof(WorldSnapshot, my_heading_centideg));
    TEST_ASSERT_EQUAL_UINT32(6,  offsetof(WorldSnapshot, my_pose_confidence));
    TEST_ASSERT_EQUAL_UINT32(7,  offsetof(WorldSnapshot, ball_x_mm));
    TEST_ASSERT_EQUAL_UINT32(9,  offsetof(WorldSnapshot, ball_y_mm));
    TEST_ASSERT_EQUAL_UINT32(11, offsetof(WorldSnapshot, ball_visible));
    TEST_ASSERT_EQUAL_UINT32(12, offsetof(WorldSnapshot, ball_confidence));
    TEST_ASSERT_EQUAL_UINT32(13, offsetof(WorldSnapshot, ball_vx_mm_s));
    TEST_ASSERT_EQUAL_UINT32(15, offsetof(WorldSnapshot, ball_vy_mm_s));
    TEST_ASSERT_EQUAL_UINT32(17, offsetof(WorldSnapshot, goal_opp_angle_centideg));
    TEST_ASSERT_EQUAL_UINT32(19, offsetof(WorldSnapshot, goal_opp_distance_mm));
    TEST_ASSERT_EQUAL_UINT32(21, offsetof(WorldSnapshot, goal_opp_visible));
    TEST_ASSERT_EQUAL_UINT32(22, offsetof(WorldSnapshot, goal_own_visible));
    TEST_ASSERT_EQUAL_UINT32(23, offsetof(WorldSnapshot, goal_own_angle_centideg));
    TEST_ASSERT_EQUAL_UINT32(25, offsetof(WorldSnapshot, goal_own_distance_mm));
    TEST_ASSERT_EQUAL_UINT32(27, offsetof(WorldSnapshot, min_obstacle_mm));
    TEST_ASSERT_EQUAL_UINT32(29, offsetof(WorldSnapshot, referee_cmd));
    TEST_ASSERT_EQUAL_UINT32(30, offsetof(WorldSnapshot, flags));

    // 2) Leer el buffer crudo en los offsets conocidos y reconstruir → debe
    //    coincidir con el valor sembrado. Pinea ≥4 campos por byte exacto.
    WorldSnapshot w = make_seeded();
    uint8_t buf[31];
    memcpy(buf, &w, sizeof(w));
    TEST_ASSERT_EQUAL_INT16(1111,  le_i16(buf + 0));   // my_x_mm
    TEST_ASSERT_EQUAL_INT16(2222,  le_i16(buf + 2));   // my_y_mm
    TEST_ASSERT_EQUAL_INT16(4444,  le_i16(buf + 7));   // ball_x_mm (no ball_y)
    TEST_ASSERT_EQUAL_INT16(-5555, le_i16(buf + 9));   // ball_y_mm
    TEST_ASSERT_EQUAL_INT16(-1234, le_i16(buf + 13));  // ball_vx_mm_s
    TEST_ASSERT_EQUAL_INT16(-4321, le_i16(buf + 23));  // goal_own_angle (no goal_opp)
    TEST_ASSERT_EQUAL_UINT16(9000, le_u16(buf + 27));  // min_obstacle_mm
    TEST_ASSERT_EQUAL_UINT8(3,     buf[29]);           // referee_cmd
    TEST_ASSERT_EQUAL_UINT8(0x10,  buf[30]);           // flags
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_worldsnapshot_has_ball_velocity_fields);
    RUN_TEST(test_worldsnapshot_size_is_31);
    RUN_TEST(test_worldsnapshot_has_goal_own_fields);
    RUN_TEST(test_worldsnapshot_heading_valid_flag_bit4);
    RUN_TEST(test_worldsnapshot_v3_byte_roundtrip);
    RUN_TEST(test_worldsnapshot_v3_golden_offsets);
    return UNITY_END();
}
