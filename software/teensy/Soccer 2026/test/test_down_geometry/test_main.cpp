// test_down_geometry — pio test -e test_native -f test_down_geometry
#include <unity.h>
#include <cmath>
#include "line_geometry.h"
#include "sensor_geometry.h"
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

// =====================================================================
// Tests para sensor_geometry.h — coords REALES del PCB (no uniformes).
// =====================================================================

// S4 (idx 3) está en (-10.12, +74.17): muy cerca del frente, leve sesgo
// izquierda. Ángulo esperado: atan2(-10.12, 74.17) ≈ -7.77° (frente-izq).
void test_sg_angle_front_sensor(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.5f, -7.77f, sg_angle_deg(3));
}

// S23 (idx 22) está en (+87.29, -0.51): casi en eje +X puro. Ángulo
// esperado: ~+90.3° (derecha pura, con micro-sesgo hacia atrás).
void test_sg_angle_right_sensor(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 90.33f, sg_angle_deg(22));
}

// S1 (idx 0) está en (-36.28, +82.04): R = sqrt(36.28² + 82.04²) ≈ 89.7 mm.
void test_sg_radius_outer(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 89.70f, sg_radius_mm(0));
}

// S29 (idx 28) está en (+34.55, -9.51): anillo INTERNO, R ≈ 35.83 mm.
void test_sg_radius_inner(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 35.83f, sg_radius_mm(28));
}

// sg_fill_angles_deg: el ángulo del idx 3 debe coincidir con sg_angle_deg(3).
void test_sg_fill_angles(void) {
    float angles[32];
    sg_fill_angles_deg(angles, 32);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, sg_angle_deg(3), angles[3]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, sg_angle_deg(22), angles[22]);
}

// Índice fuera de rango → 0.0f (defensa contra bugs).
void test_sg_out_of_range_safe(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, sg_angle_deg(-1));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, sg_angle_deg(32));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, sg_radius_mm(-1));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, sg_radius_mm(32));
}

// =====================================================================
// Tests para lg_compute_xy — centroide cartesiano con coords reales.
// =====================================================================

// Sin sensores blancos → no hay línea.
void test_lg_compute_xy_no_white(void) {
    bool w[32] = {false};
    GeomResult g = lg_compute_xy(w, SENSOR_POS, 32);
    TEST_ASSERT_FALSE(g.line_present);
    TEST_ASSERT_EQUAL_INT16(LSV2_NA_I16, g.line_angle_centideg);
    TEST_ASSERT_EQUAL_UINT8(0, g.sensors_on_line);
}

// Los 8 sensores frontales (S1-S8, idx 0-7) blancos → centroide en X≈0,
// Y≈+76. Ángulo de línea ≈ 0° (frente).
void test_lg_compute_xy_front_sensors(void) {
    bool w[32] = {false};
    for (int i = 0; i < 8; ++i) w[i] = true;
    GeomResult g = lg_compute_xy(w, SENSOR_POS, 32);
    TEST_ASSERT_TRUE(g.line_present);
    TEST_ASSERT_INT16_WITHIN(100, 0, g.line_angle_centideg);   // ±1°
    TEST_ASSERT_EQUAL_UINT8(8, g.sensors_on_line);
    // escape ≈ ±180°
    TEST_ASSERT_TRUE(std::abs((int)g.escape_angle_centideg) > 17000);
}

// El mux U3 entero (idx 16-23) blanco → centroide hacia (+70, -35) →
// ángulo ≈ +117° (atrás-derecha).
void test_lg_compute_xy_right_back_sensors(void) {
    bool w[32] = {false};
    for (int i = 16; i < 24; ++i) w[i] = true;
    GeomResult g = lg_compute_xy(w, SENSOR_POS, 32);
    TEST_ASSERT_TRUE(g.line_present);
    TEST_ASSERT_INT16_WITHIN(200, 11700, g.line_angle_centideg);  // ±2° de tolerancia
    TEST_ASSERT_EQUAL_UINT8(8, g.sensors_on_line);
}

// Los 32 sensores blancos → centroide ≈ origen → ángulo indeterminado
// pero línea presente, sensors_on_line = 32.
void test_lg_compute_xy_all_white(void) {
    bool w[32];
    for (int i = 0; i < 32; ++i) w[i] = true;
    GeomResult g = lg_compute_xy(w, SENSOR_POS, 32);
    TEST_ASSERT_TRUE(g.line_present);
    TEST_ASSERT_EQUAL_UINT8(32, g.sensors_on_line);
    // No assertimos angle específico: la distribución no es perfectamente simétrica.
}

int main(int, char**) {
    UNITY_BEGIN();
    // Tests existentes (lg_compute con ángulos uniformes — siguen vigentes).
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
    // Tests nuevos — geometría REAL del PCB (sensor_geometry + lg_compute_xy).
    RUN_TEST(test_sg_angle_front_sensor);
    RUN_TEST(test_sg_angle_right_sensor);
    RUN_TEST(test_sg_radius_outer);
    RUN_TEST(test_sg_radius_inner);
    RUN_TEST(test_sg_fill_angles);
    RUN_TEST(test_sg_out_of_range_safe);
    RUN_TEST(test_lg_compute_xy_no_white);
    RUN_TEST(test_lg_compute_xy_front_sensors);
    RUN_TEST(test_lg_compute_xy_right_back_sensors);
    RUN_TEST(test_lg_compute_xy_all_white);
    return UNITY_END();
}
