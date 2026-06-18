// test_tof_zone_mask_orient — helper PURO tof_zone_mask_orient.h (rotacion/flip de
// la mascara de zonas del ToF, para que el FIRMWARE sea el dueno de la rotacion).
// Corre con: bash scripts/run-host-tests.sh test_tof_zone_mask_orient

#include <unity.h>
#include "tof_zone_mask_orient.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

void test_identity_rot0_flip0(void) {
    // Sin rotacion ni flip => la mascara queda igual (camino no-op, byte-neutro).
    TEST_ASSERT_EQUAL_HEX16((uint16_t)0xABCD, tof_zone_mask_orient(0xABCD, 0, 0));
    TEST_ASSERT_EQUAL_HEX16((uint16_t)0xFFFF, tof_zone_mask_orient(0xFFFF, 0, 0));
    TEST_ASSERT_EQUAL_HEX16((uint16_t)0x0000, tof_zone_mask_orient(0x0000, 90, 0));
}

void test_single_bit_rotations(void) {
    // Veto canonico en la zona de DISPLAY 0 (arriba-izq). Devuelve la zona CRUDA que se MUESTRA
    // ahi tras la rotacion horaria (convencion de la app): rot90 -> cruda 12, rot270 -> cruda 3,
    // rot180 -> cruda 15 (involucion). (Antes el helper daba 90->3/270->12: ESE era el bug.)
    TEST_ASSERT_EQUAL_HEX16((uint16_t)(1u << 12), tof_zone_mask_orient(1u << 0, 90,  0));
    TEST_ASSERT_EQUAL_HEX16((uint16_t)(1u << 15), tof_zone_mask_orient(1u << 0, 180, 0));
    TEST_ASSERT_EQUAL_HEX16((uint16_t)(1u << 3),  tof_zone_mask_orient(1u << 0, 270, 0));
}

void test_veto_top_row_rot90_is_a_column(void) {
    // Caso REAL: vetar la FILA SUPERIOR del display (zonas 0,1,2,3 = 0x000F) con rot90 debe
    // vetar una COLUMNA cruda (la izquierda: 0,4,8,12 = 0x1111). Es lo que pasa fisicamente al
    // rotar 90 la grilla. Verifica que el veto "de arriba" cae donde el operador espera.
    TEST_ASSERT_EQUAL_HEX16((uint16_t)0x1111, tof_zone_mask_orient(0x000F, 90, 0));
    // Sin rotar, vetar la fila superior veta esa misma fila cruda.
    TEST_ASSERT_EQUAL_HEX16((uint16_t)0x000F, tof_zone_mask_orient(0x000F, 0, 0));
}

void test_flips(void) {
    // flip X (bit0): col0 -> col3 => idx 0 -> idx 3.  flip Y (bit1): fila0 -> fila3 => idx 0 -> 12.
    TEST_ASSERT_EQUAL_HEX16((uint16_t)(1u << 3),  tof_zone_mask_orient(1u << 0, 0, 0x1));
    TEST_ASSERT_EQUAL_HEX16((uint16_t)(1u << 12), tof_zone_mask_orient(1u << 0, 0, 0x2));
}

void test_180_matches_existing_left_correction(void) {
    // 180 = invertir fila Y columna = idx i -> 15-i (mismo resultado que tof_zone_orient.h
    // para el sensor izquierdo). Asi las dos vias de la correccion del izquierdo coinciden.
    for (int i = 0; i < 16; ++i) {
        const uint16_t got = tof_zone_mask_orient((uint16_t)(1u << i), 180, 0);
        TEST_ASSERT_EQUAL_HEX16((uint16_t)(1u << (15 - i)), got);
    }
}

void test_is_bijection(void) {
    // Toda rotacion/flip valida es una permutacion de las 16 zonas => 0xFFFF -> 0xFFFF.
    const int rots[4] = {0, 90, 180, 270};
    for (int r = 0; r < 4; ++r) {
        for (uint8_t f = 0; f < 4; ++f) {
            TEST_ASSERT_EQUAL_HEX16((uint16_t)0xFFFF, tof_zone_mask_orient(0xFFFF, rots[r], f));
        }
    }
}

void test_invalid_rot_is_identity(void) {
    // rot no multiplo de 90 -> se trata como 0 (defensivo).
    TEST_ASSERT_EQUAL_HEX16((uint16_t)0x1234, tof_zone_mask_orient(0x1234, 45, 0));
}

void test_index_defensive(void) {
    TEST_ASSERT_EQUAL_INT(-1, tof_zone_orient_index(-1, 90, 0, 4));
    TEST_ASSERT_EQUAL_INT(99, tof_zone_orient_index(99, 90, 0, 4));
    TEST_ASSERT_EQUAL_INT(5,  tof_zone_orient_index(5,  90, 0, 0));  // grid invalido -> idx
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_identity_rot0_flip0);
    RUN_TEST(test_single_bit_rotations);
    RUN_TEST(test_flips);
    RUN_TEST(test_180_matches_existing_left_correction);
    RUN_TEST(test_is_bijection);
    RUN_TEST(test_invalid_rot_is_identity);
    RUN_TEST(test_index_defensive);
    return UNITY_END();
}
