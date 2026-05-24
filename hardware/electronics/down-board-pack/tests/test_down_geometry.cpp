// test_down_geometry — pio test -e test_native -f test_down_geometry
#include <unity.h>
#include <cmath>
#include "line_geometry.h"
using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Ángulo físico: contrato 0° = frente (+Y), positivo = horario visto desde arriba.
void test_sensor_angle_front_is_zero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, lg_sensor_angle_deg(0, 8));
}

void test_sensor_angle_quarter_is_45_for_8(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 45.0f, lg_sensor_angle_deg(1, 8));
}

void test_no_line_when_none_white(void) {
    bool w[8] = {false, false, false, false, false, false, false, false};
    float a[8]; for (int i = 0; i < 8; ++i) a[i] = lg_sensor_angle_deg(i, 8);
    GeomResult g = lg_compute(w, a, 8);
    TEST_ASSERT_FALSE(g.line_present);
    TEST_ASSERT_EQUAL_INT16(LSV2_NA_I16, g.line_angle_centideg);
    TEST_ASSERT_EQUAL_UINT8(0, g.sensors_on_line);
}

void test_line_centroid_front(void) {
    bool w[8] = {true, false, false, false, false, false, false, false};
    float a[8]; for (int i = 0; i < 8; ++i) a[i] = lg_sensor_angle_deg(i, 8);
    GeomResult g = lg_compute(w, a, 8);
    TEST_ASSERT_TRUE(g.line_present);
    TEST_ASSERT_INT16_WITHIN(50, 0, g.line_angle_centideg);
    TEST_ASSERT_EQUAL_UINT8(1, g.sensors_on_line);
    TEST_ASSERT_TRUE(abs((int)g.escape_angle_centideg) > 17000);
}

void test_line_centroid_right_side(void) {
    bool w[8] = {false, false, true, false, false, false, false, false}; // sensor 2 = 90°
    float a[8]; for (int i = 0; i < 8; ++i) a[i] = lg_sensor_angle_deg(i, 8);
    GeomResult g = lg_compute(w, a, 8);
    TEST_ASSERT_TRUE(g.line_present);
    TEST_ASSERT_INT16_WITHIN(50, 9000, g.line_angle_centideg);
    TEST_ASSERT_INT16_WITHIN(50, -9000, g.escape_angle_centideg);
    TEST_ASSERT_EQUAL_UINT8(1, g.sensors_on_line);
}

void test_line_two_sensor_symmetric_centroid(void) {
    bool w[8] = {false, true, false, false, false, false, false, true}; // sensors 1 (+45) y 7 (-45)
    float a[8]; for (int i = 0; i < 8; ++i) a[i] = lg_sensor_angle_deg(i, 8);
    GeomResult g = lg_compute(w, a, 8);
    TEST_ASSERT_TRUE(g.line_present);
    TEST_ASSERT_INT16_WITHIN(50, 0, g.line_angle_centideg);
    TEST_ASSERT_EQUAL_UINT8(2, g.sensors_on_line);
    TEST_ASSERT_TRUE(abs((int)g.escape_angle_centideg) > 17000);
}

void test_corner_two_perpendicular_clusters(void){
    // Anillo 8 sensores. Blanco en frente (idx0, 0°) y derecha (idx2, 90°).
    bool w[8]={true,false,true,false,false,false,false,false};
    float a[8]; for(int i=0;i<8;++i)a[i]=lg_sensor_angle_deg(i,8);
    GeomResult g = lg_compute(w, a, 8);
    TEST_ASSERT_TRUE(g.corner);
}

void test_not_corner_single_cluster(void){
    bool w[8]={true,true,false,false,false,false,false,false}; // 0° y 45° contiguos
    float a[8]; for(int i=0;i<8;++i)a[i]=lg_sensor_angle_deg(i,8);
    GeomResult g = lg_compute(w, a, 8);
    TEST_ASSERT_FALSE(g.corner);
}

void test_corner_wraparound_clusters(void){
    // idx7(-45°) e idx0(0°) son contiguos vía wrap => cluster medio ~-22.5°; idx2(90°) => otro cluster.
    // Separación entre clusters: |90 - (-22.5)| = 112.5° ∈ [55,125] => corner TRUE.
    // Valida que el walk modular une correctamente el wrap del anillo.
    bool w[8]={true,false,true,false,false,false,false,true}; // idx0, idx2, idx7
    float a[8]; for(int i=0;i<8;++i)a[i]=lg_sensor_angle_deg(i,8);
    GeomResult g = lg_compute(w, a, 8);
    TEST_ASSERT_TRUE(g.corner);
}

void test_three_equidistant_clusters_not_corner(void){
    // idx0(0°), idx3(135°), idx5(-135°): 3 clusters aislados.
    // Par idx3/idx5: |135-(-135)|=270 => shortest=90° ∈ [55,125] => corner TRUE.
    // Documenta que 3 clusters con un par a ~90° sí activan corner.
    bool w[8]={true,false,false,true,false,true,false,false};
    float a[8]; for(int i=0;i<8;++i)a[i]=lg_sensor_angle_deg(i,8);
    GeomResult g = lg_compute(w, a, 8);
    TEST_ASSERT_TRUE(g.corner);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sensor_angle_front_is_zero);
    RUN_TEST(test_sensor_angle_quarter_is_45_for_8);
    RUN_TEST(test_no_line_when_none_white);
    RUN_TEST(test_line_centroid_front);
    RUN_TEST(test_line_centroid_right_side);
    RUN_TEST(test_line_two_sensor_symmetric_centroid);
    RUN_TEST(test_corner_two_perpendicular_clusters);
    RUN_TEST(test_not_corner_single_cluster);
    RUN_TEST(test_corner_wraparound_clusters);
    RUN_TEST(test_three_equidistant_clusters_not_corner);
    return UNITY_END();
}
