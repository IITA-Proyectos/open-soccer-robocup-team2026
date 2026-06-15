// test_rot_lut — sin/cos Q12 círculo completo + rotación de vectores en enteros (H1).
// Corre host: bash scripts/run-host-tests.sh test_rot_lut
//
// Cubre lo que el review adversarial pidió: precisión Q12 en los 4 cuadrantes, signos,
// identidad pitagórica, rotación de vectores por ángulos conocidos, round-trip y deriva
// acumulada del redondeo /4096.

#include <unity.h>
#include "rot_lut.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// cos en los ángulos cardinales y a 45° (los 4 cuadrantes).
void test_cos_cardinales(void) {
    TEST_ASSERT_EQUAL_INT32(4096,  rot_cos_q12(0));
    TEST_ASSERT_EQUAL_INT32(0,     rot_cos_q12(90));
    TEST_ASSERT_EQUAL_INT32(-4096, rot_cos_q12(180));
    TEST_ASSERT_EQUAL_INT32(0,     rot_cos_q12(270));
    TEST_ASSERT_EQUAL_INT32(4096,  rot_cos_q12(360));   // envuelve a 0
    TEST_ASSERT_INT32_WITHIN(2, 2896, rot_cos_q12(45));
    TEST_ASSERT_INT32_WITHIN(2, -2896, rot_cos_q12(135));
    TEST_ASSERT_INT32_WITHIN(2, -2896, rot_cos_q12(225));
    TEST_ASSERT_INT32_WITHIN(2, 2896, rot_cos_q12(315));
}

// sin via cos(90-d): cardinales y 45°.
void test_sin_cardinales(void) {
    TEST_ASSERT_EQUAL_INT32(0,     rot_sin_q12(0));
    TEST_ASSERT_EQUAL_INT32(4096,  rot_sin_q12(90));
    TEST_ASSERT_EQUAL_INT32(0,     rot_sin_q12(180));
    TEST_ASSERT_EQUAL_INT32(-4096, rot_sin_q12(270));
    TEST_ASSERT_INT32_WITHIN(2, 2896, rot_sin_q12(45));
    TEST_ASSERT_INT32_WITHIN(2, 2896, rot_sin_q12(135));
    TEST_ASSERT_INT32_WITHIN(2, -2896, rot_sin_q12(225));
    TEST_ASSERT_INT32_WITHIN(2, -2896, rot_sin_q12(315));
}

// Ángulos negativos y > 360 se normalizan.
void test_normalizacion(void) {
    TEST_ASSERT_EQUAL_INT32(rot_cos_q12(270), rot_cos_q12(-90));
    TEST_ASSERT_EQUAL_INT32(rot_cos_q12(10),  rot_cos_q12(370));
    TEST_ASSERT_EQUAL_INT32(rot_sin_q12(45),  rot_sin_q12(-315));
}

// Identidad pitagórica: cos²+sin² ≈ 4096² en muchos ángulos.
void test_pitagoras(void) {
    for (int d = 0; d < 360; d += 7) {
        int32_t c = rot_cos_q12(d), s = rot_sin_q12(d);
        int32_t mag2 = c * c + s * s;
        // 4096² = 16777216; tolerancia por el redondeo de la tabla (~0.5%).
        TEST_ASSERT_INT32_WITHIN(170000, 16777216, mag2);
    }
}

// Rotar (100,0) por los cardinales: 90→(0,100), 180→(-100,0), 270→(0,-100).
void test_rota_vector_cardinales(void) {
    int32_t ox, oy;
    rot_rotate_q12(100, 0, 0, ox, oy);    TEST_ASSERT_INT32_WITHIN(1, 100, ox); TEST_ASSERT_INT32_WITHIN(1, 0, oy);
    rot_rotate_q12(100, 0, 90, ox, oy);   TEST_ASSERT_INT32_WITHIN(1, 0, ox);   TEST_ASSERT_INT32_WITHIN(1, 100, oy);
    rot_rotate_q12(100, 0, 180, ox, oy);  TEST_ASSERT_INT32_WITHIN(1, -100, ox);TEST_ASSERT_INT32_WITHIN(1, 0, oy);
    rot_rotate_q12(100, 0, 270, ox, oy);  TEST_ASSERT_INT32_WITHIN(1, 0, ox);   TEST_ASSERT_INT32_WITHIN(1, -100, oy);
}

// Rotar (0,100) por 90 → (-100,0) (consistente con la matriz CCW).
void test_rota_vector_y(void) {
    int32_t ox, oy;
    rot_rotate_q12(0, 100, 90, ox, oy);
    TEST_ASSERT_INT32_WITHIN(1, -100, ox);
    TEST_ASSERT_INT32_WITHIN(1, 0, oy);
}

// Round-trip: rotar +deg y luego −deg vuelve cerca del original (redondeo acotado).
void test_round_trip(void) {
    const int angs[5] = {45, 90, 135, 225, 315};
    for (int k = 0; k < 5; ++k) {
        int32_t ax, ay, bx, by;
        rot_rotate_q12(123, -57, angs[k], ax, ay);
        rot_rotate_q12(ax, ay, -angs[k], bx, by);
        TEST_ASSERT_INT32_WITHIN(2, 123, bx);
        TEST_ASSERT_INT32_WITHIN(2, -57, by);
    }
}

// Deriva acumulada: 36 rotaciones de 10° (= 360°) sobre un vector chico vuelven cerca
// del original (el sesgo del redondeo /4096 queda acotado).
void test_deriva_acumulada(void) {
    int32_t x = 80, y = 0;
    for (int i = 0; i < 36; ++i) {
        int32_t nx, ny;
        rot_rotate_q12(x, y, 10, nx, ny);
        x = nx; y = ny;
    }
    TEST_ASSERT_INT32_WITHIN(6, 80, x);
    TEST_ASSERT_INT32_WITHIN(6, 0, y);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_cos_cardinales);
    RUN_TEST(test_sin_cardinales);
    RUN_TEST(test_normalizacion);
    RUN_TEST(test_pitagoras);
    RUN_TEST(test_rota_vector_cardinales);
    RUN_TEST(test_rota_vector_y);
    RUN_TEST(test_round_trip);
    RUN_TEST(test_deriva_acumulada);
    return UNITY_END();
}
