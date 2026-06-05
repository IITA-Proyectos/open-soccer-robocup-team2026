// test_snapshot_roundtrip — GOLDEN round-trip del WorldSnapshot v3 (31 B).
// Corre host con:
//   bash scripts/run-host-tests.sh test_snapshot_roundtrip
//
// Por qué existe (TEST-GAP de blindaje, audit 2026-06-05)
// ------------------------------------------------------
// El WorldSnapshot v3 viaja por el cable TOP→CENTRAL como BYTES CRUDOS del struct
// packed: el TOP hace `memcpy(frame.payload, &snap, sizeof(WorldSnapshot))`
// (comm_central.cpp) y el CENTRAL hace `memcpy(&snap, frame.payload, ...)`
// (comm_top.cpp). No hay (de)serializador campo-a-campo: el contrato de wire ES
// el layout en memoria del struct. Por eso un cambio silencioso de OFFSET (campo
// reordenado, tipo cambiado, padding inyectado) rompería el enlace SIN cambiar el
// sizeof (que test_central_contract ya cubre). Este test FIJA el layout byte-a-byte:
//
//   1) sizeof == 31 (contrato v3).
//   2) Round-trip memcpy→bytes→memcpy de un snapshot con valor ÚNICO en CADA
//      campo (incluye ball_vx/vy, goal_own_*, flags bit4) → igualdad campo-a-campo.
//   3) El OFFSET de cada campo está PINNEADO (offsetof) → reordenar/insertar un
//      campo mueve un offset y rompe acá.
//   4) Bytes little-endian en offsets clave PINNEADOS a mano → un cambio de
//      endianness o de empaquetado se detecta aunque los offsets "cuadren".
//
// PURO: solo types.h (+ <string.h>/<stddef.h>). types.h incluye solo <stdint.h> +
// <climits> → compila host. -I src/shared ya está en el harness.

#include <unity.h>
#include <string.h>
#include <stddef.h>
#include "types.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// (1) El contrato de tamaño v3: 31 bytes exactos. (Redundante con
// test_central_contract a propósito: este test debe poder correr solo.)
// ---------------------------------------------------------------------------
void test_size_is_31(void) {
    TEST_ASSERT_EQUAL_UINT32(31u, (uint32_t)sizeof(WorldSnapshot));
}

// ---------------------------------------------------------------------------
// (3) Offsets PINNEADOS. Si alguien reordena un campo, inserta uno nuevo o el
// compilador inyecta padding (no debería: __attribute__((packed))), uno de estos
// offsets cambia y el test falla apuntando al campo movido. El layout v3:
//
//   off  campo                      tipo     bytes
//   ---  -------------------------  -------  -----
//    0   my_x_mm                    int16     2
//    2   my_y_mm                    int16     2
//    4   my_heading_centideg        int16     2
//    6   my_pose_confidence         uint8     1
//    7   ball_x_mm                  int16     2
//    9   ball_y_mm                  int16     2
//   11   ball_visible               uint8     1
//   12   ball_confidence            uint8     1
//   13   ball_vx_mm_s               int16     2
//   15   ball_vy_mm_s               int16     2
//   17   goal_opp_angle_centideg    int16     2
//   19   goal_opp_distance_mm       int16     2
//   21   goal_opp_visible           uint8     1
//   22   goal_own_visible           uint8     1
//   23   goal_own_angle_centideg    int16     2
//   25   goal_own_distance_mm       int16     2
//   27   min_obstacle_mm            uint16    2
//   29   referee_cmd                uint8     1
//   30   flags                      uint8     1
//                                            ----
//                                             31
// ---------------------------------------------------------------------------
void test_field_offsets_pinned(void) {
    TEST_ASSERT_EQUAL_UINT32( 0u, (uint32_t)offsetof(WorldSnapshot, my_x_mm));
    TEST_ASSERT_EQUAL_UINT32( 2u, (uint32_t)offsetof(WorldSnapshot, my_y_mm));
    TEST_ASSERT_EQUAL_UINT32( 4u, (uint32_t)offsetof(WorldSnapshot, my_heading_centideg));
    TEST_ASSERT_EQUAL_UINT32( 6u, (uint32_t)offsetof(WorldSnapshot, my_pose_confidence));
    TEST_ASSERT_EQUAL_UINT32( 7u, (uint32_t)offsetof(WorldSnapshot, ball_x_mm));
    TEST_ASSERT_EQUAL_UINT32( 9u, (uint32_t)offsetof(WorldSnapshot, ball_y_mm));
    TEST_ASSERT_EQUAL_UINT32(11u, (uint32_t)offsetof(WorldSnapshot, ball_visible));
    TEST_ASSERT_EQUAL_UINT32(12u, (uint32_t)offsetof(WorldSnapshot, ball_confidence));
    TEST_ASSERT_EQUAL_UINT32(13u, (uint32_t)offsetof(WorldSnapshot, ball_vx_mm_s));
    TEST_ASSERT_EQUAL_UINT32(15u, (uint32_t)offsetof(WorldSnapshot, ball_vy_mm_s));
    TEST_ASSERT_EQUAL_UINT32(17u, (uint32_t)offsetof(WorldSnapshot, goal_opp_angle_centideg));
    TEST_ASSERT_EQUAL_UINT32(19u, (uint32_t)offsetof(WorldSnapshot, goal_opp_distance_mm));
    TEST_ASSERT_EQUAL_UINT32(21u, (uint32_t)offsetof(WorldSnapshot, goal_opp_visible));
    TEST_ASSERT_EQUAL_UINT32(22u, (uint32_t)offsetof(WorldSnapshot, goal_own_visible));
    TEST_ASSERT_EQUAL_UINT32(23u, (uint32_t)offsetof(WorldSnapshot, goal_own_angle_centideg));
    TEST_ASSERT_EQUAL_UINT32(25u, (uint32_t)offsetof(WorldSnapshot, goal_own_distance_mm));
    TEST_ASSERT_EQUAL_UINT32(27u, (uint32_t)offsetof(WorldSnapshot, min_obstacle_mm));
    TEST_ASSERT_EQUAL_UINT32(29u, (uint32_t)offsetof(WorldSnapshot, referee_cmd));
    TEST_ASSERT_EQUAL_UINT32(30u, (uint32_t)offsetof(WorldSnapshot, flags));
}

// Llena un snapshot con un valor ÚNICO y reconocible en CADA campo. Valores con
// signo distinto / bits altos para detectar cruces de campos o truncamientos.
static WorldSnapshot make_known(void) {
    WorldSnapshot s{};
    s.my_x_mm                 = -12345;   // negativo + bits altos
    s.my_y_mm                 =  23456;
    s.my_heading_centideg     = -17999;   // ~ -179.99°
    s.my_pose_confidence      =     77;
    s.ball_x_mm               =   -321;
    s.ball_y_mm               =    654;
    s.ball_visible            =      1;
    s.ball_confidence         =     88;
    s.ball_vx_mm_s            =  -1500;   // velocidad pelota (v3)
    s.ball_vy_mm_s            =   2000;
    s.goal_opp_angle_centideg =   4500;   // +45.00°
    s.goal_opp_distance_mm    =   1820;
    s.goal_opp_visible        =      1;
    s.goal_own_visible        =      1;   // v3
    s.goal_own_angle_centideg =  -9000;   // -90.00° (v3)
    s.goal_own_distance_mm    =   2430;   // (v3)
    s.min_obstacle_mm         =  0xBEEF;  // 48879
    s.referee_cmd             =      3;   // reset
    s.flags                   =   0x10;   // bit4 = heading_valid (v3)
    return s;
}

// ---------------------------------------------------------------------------
// (2) Round-trip: struct → 31 bytes (memcpy, IGUAL que el wire) → struct →
// igualdad campo-a-campo. Esto es exactamente lo que hacen comm_central.cpp
// (TX en TOP) y comm_top.cpp (RX en CENTRAL).
// ---------------------------------------------------------------------------
void test_memcpy_roundtrip_all_fields(void) {
    const WorldSnapshot src = make_known();

    uint8_t wire[31];
    memcpy(wire, &src, sizeof(WorldSnapshot));   // serializar (TOP)

    WorldSnapshot dst{};
    memcpy(&dst, wire, sizeof(WorldSnapshot));   // des-serializar (CENTRAL)

    TEST_ASSERT_EQUAL_INT16 (src.my_x_mm,                 dst.my_x_mm);
    TEST_ASSERT_EQUAL_INT16 (src.my_y_mm,                 dst.my_y_mm);
    TEST_ASSERT_EQUAL_INT16 (src.my_heading_centideg,     dst.my_heading_centideg);
    TEST_ASSERT_EQUAL_UINT8 (src.my_pose_confidence,      dst.my_pose_confidence);
    TEST_ASSERT_EQUAL_INT16 (src.ball_x_mm,               dst.ball_x_mm);
    TEST_ASSERT_EQUAL_INT16 (src.ball_y_mm,               dst.ball_y_mm);
    TEST_ASSERT_EQUAL_UINT8 (src.ball_visible,            dst.ball_visible);
    TEST_ASSERT_EQUAL_UINT8 (src.ball_confidence,         dst.ball_confidence);
    TEST_ASSERT_EQUAL_INT16 (src.ball_vx_mm_s,            dst.ball_vx_mm_s);
    TEST_ASSERT_EQUAL_INT16 (src.ball_vy_mm_s,            dst.ball_vy_mm_s);
    TEST_ASSERT_EQUAL_INT16 (src.goal_opp_angle_centideg, dst.goal_opp_angle_centideg);
    TEST_ASSERT_EQUAL_INT16 (src.goal_opp_distance_mm,    dst.goal_opp_distance_mm);
    TEST_ASSERT_EQUAL_UINT8 (src.goal_opp_visible,        dst.goal_opp_visible);
    TEST_ASSERT_EQUAL_UINT8 (src.goal_own_visible,        dst.goal_own_visible);
    TEST_ASSERT_EQUAL_INT16 (src.goal_own_angle_centideg, dst.goal_own_angle_centideg);
    TEST_ASSERT_EQUAL_INT16 (src.goal_own_distance_mm,    dst.goal_own_distance_mm);
    TEST_ASSERT_EQUAL_UINT16(src.min_obstacle_mm,         dst.min_obstacle_mm);
    TEST_ASSERT_EQUAL_UINT8 (src.referee_cmd,             dst.referee_cmd);
    TEST_ASSERT_EQUAL_UINT8 (src.flags,                   dst.flags);

    // Y el blob completo debe ser idéntico (sin bytes "sueltos" por padding).
    TEST_ASSERT_EQUAL_MEMORY(&src, &dst, sizeof(WorldSnapshot));
}

// Helper: lee un int16 little-endian desde el blob (orden del Teensy ARM = LE).
static int16_t le16(const uint8_t* p) {
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint16_t leu16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

// ---------------------------------------------------------------------------
// (4) Golden byte-level: el blob serializado tiene los valores en los offsets y
// el endianness esperados. Esto ata el contrato al byte, no solo al campo: un
// cambio de endianness o de empaquetado se ve aunque offsetof "cuadre".
// ---------------------------------------------------------------------------
void test_golden_bytes_little_endian(void) {
    const WorldSnapshot src = make_known();
    uint8_t b[31];
    memcpy(b, &src, sizeof(WorldSnapshot));

    // int16 LE en offsets clave.
    TEST_ASSERT_EQUAL_INT16(-12345, le16(&b[0]));    // my_x_mm
    TEST_ASSERT_EQUAL_INT16(-17999, le16(&b[4]));    // my_heading_centideg
    TEST_ASSERT_EQUAL_INT16(-1500,  le16(&b[13]));   // ball_vx_mm_s (v3)
    TEST_ASSERT_EQUAL_INT16( 2000,  le16(&b[15]));   // ball_vy_mm_s (v3)
    TEST_ASSERT_EQUAL_INT16(-9000,  le16(&b[23]));   // goal_own_angle_centideg (v3)
    TEST_ASSERT_EQUAL_INT16( 2430,  le16(&b[25]));   // goal_own_distance_mm (v3)

    // uint16 LE: 0xBEEF -> EF (low) en off 27, BE (high) en off 28.
    TEST_ASSERT_EQUAL_UINT16(0xBEEF, leu16(&b[27])); // min_obstacle_mm
    TEST_ASSERT_EQUAL_UINT8 (0xEF,   b[27]);
    TEST_ASSERT_EQUAL_UINT8 (0xBE,   b[28]);

    // uint8 escalares.
    TEST_ASSERT_EQUAL_UINT8(77,   b[6]);   // my_pose_confidence
    TEST_ASSERT_EQUAL_UINT8(1,    b[11]);  // ball_visible
    TEST_ASSERT_EQUAL_UINT8(1,    b[22]);  // goal_own_visible (v3)
    TEST_ASSERT_EQUAL_UINT8(3,    b[29]);  // referee_cmd
    TEST_ASSERT_EQUAL_UINT8(0x10, b[30]);  // flags: bit4 heading_valid (v3)

    // El bit4 de flags está realmente prendido en el byte serializado.
    TEST_ASSERT_TRUE((b[30] & 0x10) != 0);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_size_is_31);
    RUN_TEST(test_field_offsets_pinned);
    RUN_TEST(test_memcpy_roundtrip_all_fields);
    RUN_TEST(test_golden_bytes_little_endian);
    return UNITY_END();
}
