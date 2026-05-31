// test_tof_zone_orient — verifica la corrección de orientación de zonas ToF.
// El izquierdo (TOF3) está rotado 180° respecto a los otros 3 (banco 2026-05-31).

#include <unity.h>
#include "tof_zone_orient.h"

using namespace iitasoccer;

// Los sensores frontal/atrás/derecha NO se tocan (identidad).
void test_non_left_identity() {
    for (int s = 0; s <= 2; ++s) {
        for (int z = 0; z < 64; ++z) {
            TEST_ASSERT_EQUAL_INT(z, tof_raw_zone_for_canonical(s, z, 8));
        }
    }
}

// Solo el izquierdo necesita corrección.
void test_only_left_needs_180() {
    TEST_ASSERT_FALSE(tof_zone_needs_180(TOF_IDX_FRONT));
    TEST_ASSERT_FALSE(tof_zone_needs_180(TOF_IDX_BACK));
    TEST_ASSERT_FALSE(tof_zone_needs_180(TOF_IDX_RIGHT));
    TEST_ASSERT_TRUE (tof_zone_needs_180(TOF_IDX_LEFT));
}

// 180° en grilla 8x8: (fila,col) -> (7-fila, 7-col).
void test_left_180_known_points() {
    // esquina sup-izq (0,0)=0  -> esquina inf-der (7,7)=63
    TEST_ASSERT_EQUAL_INT(63, tof_raw_zone_for_canonical(TOF_IDX_LEFT, 0, 8));
    // esquina inf-der (7,7)=63 -> (0,0)=0
    TEST_ASSERT_EQUAL_INT(0,  tof_raw_zone_for_canonical(TOF_IDX_LEFT, 63, 8));
    // (0,6)=6 -> (7,1)=57
    TEST_ASSERT_EQUAL_INT(57, tof_raw_zone_for_canonical(TOF_IDX_LEFT, 6, 8));
    // centro-ish (3,3)=27 -> (4,4)=36
    TEST_ASSERT_EQUAL_INT(36, tof_raw_zone_for_canonical(TOF_IDX_LEFT, 27, 8));
}

// La rotación de 180° es su propia inversa: aplicarla dos veces = identidad.
void test_left_180_is_involution() {
    for (int z = 0; z < 64; ++z) {
        int once = tof_raw_zone_for_canonical(TOF_IDX_LEFT, z, 8);
        int twice = tof_raw_zone_for_canonical(TOF_IDX_LEFT, once, 8);
        TEST_ASSERT_EQUAL_INT(z, twice);
    }
}

// Es una biyección: las 64 zonas mapean a 64 zonas distintas.
void test_left_180_is_bijection() {
    bool seen[64] = {false};
    for (int z = 0; z < 64; ++z) {
        int m = tof_raw_zone_for_canonical(TOF_IDX_LEFT, z, 8);
        TEST_ASSERT_TRUE(m >= 0 && m < 64);
        TEST_ASSERT_FALSE(seen[m]);  // sin colisiones
        seen[m] = true;
    }
}

// Funciona también en 4x4 (por si el firmware usa esa resolución).
void test_left_180_grid4() {
    // (0,0)=0 -> (3,3)=15 ; (0,3)=3 -> (3,0)=12
    TEST_ASSERT_EQUAL_INT(15, tof_raw_zone_for_canonical(TOF_IDX_LEFT, 0, 4));
    TEST_ASSERT_EQUAL_INT(12, tof_raw_zone_for_canonical(TOF_IDX_LEFT, 3, 4));
}

// Defensivo: índices/anchos inválidos no rompen.
void test_defensive() {
    TEST_ASSERT_EQUAL_INT(5, tof_raw_zone_for_canonical(TOF_IDX_LEFT, 5, 0));   // grid_w<=0
    TEST_ASSERT_EQUAL_INT(-1, tof_raw_zone_for_canonical(TOF_IDX_LEFT, -1, 8)); // fuera de rango
    TEST_ASSERT_EQUAL_INT(99, tof_raw_zone_for_canonical(TOF_IDX_LEFT, 99, 8)); // fuera de rango
}

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_non_left_identity);
    RUN_TEST(test_only_left_needs_180);
    RUN_TEST(test_left_180_known_points);
    RUN_TEST(test_left_180_is_involution);
    RUN_TEST(test_left_180_is_bijection);
    RUN_TEST(test_left_180_grid4);
    RUN_TEST(test_defensive);
    return UNITY_END();
}
