// test_robot_geometry — TDD de la geometría física del robot (fuente única).
//
// Cubre src/shared/robot_geometry.h: las constantes de montaje (sanity), la rotación de
// offset robot→cancha por heading, y la corrección distancia-sensor → distancia-centro.
//
//   bash scripts/run-host-tests.sh test_robot_geometry

#include <unity.h>
#include <math.h>
#include "robot_geometry.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// (1) Constantes de montaje: radio y offsets coherentes (offset < radio; alturas dadas).
void test_geom_constants_sanity(void) {
    TEST_ASSERT_EQUAL_INT16(85, ROBOT_RADIUS_MM);
    // los 4 ToF a ~60 mm, h 170; cámaras a ~70, h 120.
    for (int i = 0; i < 4; ++i) {
        const int r = (int)lroundf(sqrtf((float)TOF_GEOM[i].off_x_mm * TOF_GEOM[i].off_x_mm +
                                          (float)TOF_GEOM[i].off_y_mm * TOF_GEOM[i].off_y_mm));
        TEST_ASSERT_INT_WITHIN(5, 60, r);                 // ~60 mm del centro
        TEST_ASSERT_TRUE(r < ROBOT_RADIUS_MM);            // adentro del borde
        TEST_ASSERT_EQUAL_INT16(170, TOF_GEOM[i].height_mm);
    }
    TEST_ASSERT_EQUAL_INT16(120, CAM_GEOM[0].height_mm);
    TEST_ASSERT_EQUAL_INT16(120, CAM_GEOM[1].height_mm);
}

// (2) Rotación a heading 0 = identidad (marco robot == marco cancha).
void test_rotate_offset_heading_zero_is_identity(void) {
    float fx = 0, fy = 0;
    geom_rotate_offset(0.0f, 60.0f, 0.0f, fx, fy);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, fx);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 60.0f, fy);
}

// (3) Rotación CCW de +90°: (0,60) → (-60,0). Rotación estándar.
void test_rotate_offset_90ccw(void) {
    float fx = 0, fy = 0;
    geom_rotate_offset(0.0f, 60.0f, (float)M_PI / 2.0f, fx, fy);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -60.0f, fx);
    TEST_ASSERT_FLOAT_WITHIN(0.05f,   0.0f, fy);
}

// (4) Rotación de 180°: (0,60) → (0,-60).
void test_rotate_offset_180(void) {
    float fx = 0, fy = 0;
    geom_rotate_offset(0.0f, 60.0f, (float)M_PI, fx, fy);
    TEST_ASSERT_FLOAT_WITHIN(0.05f,   0.0f, fx);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -60.0f, fy);
}

// (5) CASO CANÓNICO de la corrección: ToF frontal (Δ=(0,+60)) hacia el arco rival (pared
//     al frente, normal interior n̂=(0,-1)). El centro está 60 mm MÁS LEJOS que el sensor.
void test_center_dist_front_sensor(void) {
    const float center = geom_center_dist_to_wall(1000.0f, /*Δ*/0.0f, 60.0f, /*n̂*/0.0f, -1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1060.0f, center);   // sensor adelantado mide 60 mm menos
}

// (6) Sensor ATRASADO respecto de la pared mide MÁS que el centro → corrección negativa.
//     ToF trasero Δ=(0,-60) respecto de la pared del frente (n̂=(0,-1)): Δ·n̂ = +60 →
//     center = sensor - 60 (el centro está más cerca de la pared del frente que el trasero).
void test_center_dist_back_sensor(void) {
    const float center = geom_center_dist_to_wall(1000.0f, 0.0f, -60.0f, 0.0f, -1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 940.0f, center);
}

// (7) Offset paralelo a la pared (no afecta): ToF derecho Δ=(60,0) vs pared del frente
//     (n̂=(0,-1)): Δ·n̂ = 0 → center == sensor (el offset lateral no cambia la distancia
//     perpendicular a esa pared).
void test_center_dist_parallel_offset_no_change(void) {
    const float center = geom_center_dist_to_wall(1000.0f, 60.0f, 0.0f, 0.0f, -1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1000.0f, center);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_geom_constants_sanity);
    RUN_TEST(test_rotate_offset_heading_zero_is_identity);
    RUN_TEST(test_rotate_offset_90ccw);
    RUN_TEST(test_rotate_offset_180);
    RUN_TEST(test_center_dist_front_sensor);
    RUN_TEST(test_center_dist_back_sensor);
    RUN_TEST(test_center_dist_parallel_offset_no_change);
    return UNITY_END();
}
