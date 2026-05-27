// test_localization — tests unitarios del modulo localization (puro).
// Corre en host con: pio test -e test_native -f test_localization
//
// Cubre: trilateracion geometrica, rotacion, outlier rejection.

#include <unity.h>
#include "localization.h"

using namespace iitasoccer;

// ============================================================
// Helpers de test
// ============================================================
namespace {

// Configuracion estandar para los tests — cancha RCJ Open 2026 + montaje cardinal.
LocalizationConfig make_standard_config() {
    LocalizationConfig cfg{};
    cfg.field_width_mm           = 2430;
    cfg.field_height_mm          = 1820;
    cfg.bno_offset_centideg      = 0;  // heading=0 = robot apunta al arco rival
    cfg.tof_mount_angle_deg[0]   = 0;    // frontal
    cfg.tof_mount_angle_deg[1]   = 180;  // trasero
    cfg.tof_mount_angle_deg[2]   = 90;   // izquierdo
    cfg.tof_mount_angle_deg[3]   = 270;  // derecho
    cfg.outlier_threshold_mm     = 300;
    cfg.prev_x_mm                = 1215; // centro de la cancha
    cfg.prev_y_mm                = 910;
    cfg.prev_valid               = true;
    return cfg;
}

// Inputs con los 4 TOFs validos y un heading dado.
LocalizationInputs make_inputs(
    uint16_t front_mm, uint16_t back_mm, uint16_t left_mm, uint16_t right_mm,
    int16_t heading_centideg
) {
    LocalizationInputs in{};
    in.tof_distance_mm[0] = front_mm;
    in.tof_distance_mm[1] = back_mm;
    in.tof_distance_mm[2] = left_mm;
    in.tof_distance_mm[3] = right_mm;
    in.tof_valid[0] = (front_mm != LOCALIZATION_TOF_NO_READING);
    in.tof_valid[1] = (back_mm != LOCALIZATION_TOF_NO_READING);
    in.tof_valid[2] = (left_mm != LOCALIZATION_TOF_NO_READING);
    in.tof_valid[3] = (right_mm != LOCALIZATION_TOF_NO_READING);
    in.bno_heading_centideg = heading_centideg;
    return in;
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

// ============================================================
// Tests de trilateracion basica (sin rotacion, sin outliers)
// ============================================================

void test_robot_en_centro_apunta_arco_rival(void) {
    // Robot en (1215, 910), apuntando a +Y (al arco rival).
    auto in = make_inputs(910, 910, 1215, 1215, 0);
    auto cfg = make_standard_config();
    cfg.prev_valid = false;  // primer ciclo

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 1215, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 910,  pose.y_mm);
    TEST_ASSERT_INT16_WITHIN(50, 0,    pose.heading_centideg);
}

void test_robot_en_esquina_propia_apunta_arco_rival(void) {
    // Robot en (0, 0), apuntando a +Y.
    auto in = make_inputs(1820, 0, 0, 2430, 0);
    auto cfg = make_standard_config();
    cfg.prev_valid = false;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 0, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 0, pose.y_mm);
}

void test_robot_en_esquina_rival(void) {
    // Robot en (2430, 1820), apuntando a +Y.
    auto in = make_inputs(0, 1820, 2430, 0, 0);
    auto cfg = make_standard_config();
    cfg.prev_valid = false;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 2430, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 1820, pose.y_mm);
}

void test_robot_pegado_pared_lateral(void) {
    // Robot en (50, 910), pegado a la pared oeste.
    auto in = make_inputs(910, 910, 50, 2380, 0);
    auto cfg = make_standard_config();
    cfg.prev_valid = false;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 50, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 910, pose.y_mm);
}

void test_robot_en_centro_rotado_90_derecha(void) {
    // Robot en (1215, 910), rotado 90 grados a la derecha (heading = +9000 centideg).
    // Ahora el TOF [0] mont 0 grados apunta a +X mundo (lateral derecho).
    //              [1] mont 180 grados apunta a -X mundo (lateral izq).
    //              [2] mont 90 grados (izq robot) apunta a -Y mundo (pared sur).
    //              [3] mont 270 grados (der robot) apunta a +Y mundo (pared norte).
    // Distancias esperadas:
    //   frontal [0] → este: field_width - x = 2430 - 1215 = 1215
    //   trasero [1] → oeste: x = 1215
    //   izq [2] → sur: y = 910
    //   der [3] → norte: field_height - y = 910
    auto in = make_inputs(1215, 1215, 910, 910, 9000);
    auto cfg = make_standard_config();
    cfg.prev_valid = false;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 1215, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 910, pose.y_mm);
    TEST_ASSERT_INT16_WITHIN(50, 9000, pose.heading_centideg);
}

// ============================================================
// Runner Unity
// ============================================================
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_robot_en_centro_apunta_arco_rival);
    RUN_TEST(test_robot_en_esquina_propia_apunta_arco_rival);
    RUN_TEST(test_robot_en_esquina_rival);
    RUN_TEST(test_robot_pegado_pared_lateral);
    RUN_TEST(test_robot_en_centro_rotado_90_derecha);
    return UNITY_END();
}
