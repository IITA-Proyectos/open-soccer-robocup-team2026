// test_localization_f1a_f1b_geometria — fix de geometria de localizacion.
//
// NOTA SOBRE EL NOMBRE DEL DIRECTORIO:
//   El diseno pedia el dir
//   "test_localization__src_shared_localization__h_cpp_____fix_de_geometria__..."
//   (276 chars). Windows NO permite nombres de directorio > 248 chars y este
//   sistema tiene long-path support deshabilitado, asi que ese nombre es
//   FISICAMENTE imposible de crear. Se usa este nombre corto y unico. El
//   harness scripts/run-host-tests.sh filtra por SUBSTRING del nombre del dir,
//   asi que correr con filtro "test_localization_f1a_f1b" selecciona SOLO este
//   test (no choca con el legacy test_localization, que no contiene "_f1a").
//
// Cubre:
//   F1a: offset sensor->centro del robot (radio). El ToF esta en el borde, no
//        en el centro; con 1 solo sensor por eje hay que sumar el radio antes
//        de proyectar (con 2 opuestos el offset se cancela en el promedio).
//   F1b: proyeccion por coseno cuando el robot esta rotado a angulo no-cardinal.
//        El ToF mide la hipotenusa al rayo; hay que proyectar por cos(alpha)
//        para recuperar la distancia perpendicular del centro a la pared.
//
// Invariante de NO-REGRESION: con tof_offset_mm=0 y heading cardinal
// (alpha=0 -> cos=1), dperp == d exacto -> comportamiento legacy intacto.
//
// Corre en host con:
//   bash scripts/run-host-tests.sh test_localization_f1a_f1b

#include <unity.h>
#include "localization.h"

using namespace iitasoccer;

// ============================================================
// Helpers de test
// ============================================================
namespace {

// Config estandar — cancha RCJ Open 2026 + montaje cardinal CANONICO
// (pinout_common.h: TOF_MOUNT_ANGLE_DEG = {0,180,270,90}).
//   indice [0] = FRENTE    -> 0    (mira +Y, NORTH)
//   indice [1] = ATRAS     -> 180  (mira -Y, SOUTH)
//   indice [2] = DERECHA   -> 270  (mira +X, EAST)
//   indice [3] = IZQUIERDA -> 90   (mira -X, WEST)
LocalizationConfig make_config() {
    LocalizationConfig cfg{};
    cfg.field_width_mm         = 2430;   // X largo
    cfg.field_height_mm        = 1820;   // Y corto (al arco rival)
    cfg.bno_offset_centideg    = 0;      // heading=0 = robot apunta al arco rival
    cfg.tof_mount_angle_deg[0] = 0;      // frente
    cfg.tof_mount_angle_deg[1] = 180;    // atras
    cfg.tof_mount_angle_deg[2] = 270;    // derecha
    cfg.tof_mount_angle_deg[3] = 90;     // izquierda
    cfg.tof_offset_mm          = 0;      // F1a OFF por default (legacy)
    cfg.outlier_threshold_mm   = 300;
    cfg.prev_x_mm              = 1215;
    cfg.prev_y_mm              = 910;
    cfg.prev_valid             = false;  // sin outlier rejection salvo que se pida
    return cfg;
}

// Sentinel para marcar un ToF como invalido en el helper de inputs.
constexpr uint16_t INV = LOCALIZATION_TOF_NO_READING;

// Inputs por indice fisico de ToF: [0]=frente [1]=atras [2]=derecha [3]=izquierda.
// Pasar INV en cualquier posicion la marca invalida.
LocalizationInputs make_inputs(
    uint16_t front_mm, uint16_t back_mm, uint16_t right_mm, uint16_t left_mm,
    int16_t heading_centideg
) {
    LocalizationInputs in{};
    in.tof_distance_mm[0] = front_mm;
    in.tof_distance_mm[1] = back_mm;
    in.tof_distance_mm[2] = right_mm;
    in.tof_distance_mm[3] = left_mm;
    in.tof_valid[0] = (front_mm != INV);
    in.tof_valid[1] = (back_mm  != INV);
    in.tof_valid[2] = (right_mm != INV);
    in.tof_valid[3] = (left_mm  != INV);
    in.bno_heading_centideg = heading_centideg;
    return in;
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

// ============================================================
// F1a — offset sensor->centro del robot (radio)
// ============================================================

void test_F1a_un_solo_sensor_eje_Y_corrige_radio(void) {
    // Robot centro (1215,910), heading 0, r=95. El sensor frontal esta en
    // y=1005 (centro+95), asi que MIDE 1820-1005 = 815 a la pared NORTE.
    // Con F1a: dperp=(815+95)=910 -> y=1820-910=910. CORRECTO.
    // Control (sin F1a): y=1820-815=1005 (error +95).
    // X lo dan derecha+izquierda (opuestos, el +r se cancela).
    auto in  = make_inputs(/*front*/815, /*back*/INV, /*right*/1215, /*left*/1215, /*h*/0);
    auto cfg = make_config();
    cfg.tof_offset_mm = 95;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 910,  pose.y_mm);  // F1a corrige el radio
    TEST_ASSERT_INT16_WITHIN(10, 1215, pose.x_mm);
}

void test_F1a_dos_sensores_opuestos_siguen_cancelando(void) {
    // Robot centro (1215,910), heading 0, r=95. Ambos sensores Y validos.
    // Sensor frontal lee 815 (esta en y=1005), trasero lee 815 (esta en y=815).
    // dperp_front=(815+95)=910 -> y=1820-910=910 ; dperp_back=(815+95)=910 -> y=910.
    // promedio=910. El +r se cancela en opuestos (sin doble correccion).
    // Laterales en x=1215+-95 leen 1120 cada uno -> X promedia 1215.
    auto in  = make_inputs(815, 815, /*right*/1120, /*left*/1120, 0);
    auto cfg = make_config();
    cfg.tof_offset_mm = 95;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 1215, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 910,  pose.y_mm);
}

void test_F1a_offset_cero_equivale_legacy(void) {
    // Invariante de NO-REGRESION: offset 0 + heading cardinal == comportamiento viejo.
    // Mismo input que el test legacy de robot en centro.
    auto in  = make_inputs(910, 910, 1215, 1215, 0);
    auto cfg = make_config();  // tof_offset_mm = 0 por default

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 1215, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 910,  pose.y_mm);
}

void test_F1a_esquina_un_sensor_por_eje_con_offset(void) {
    // Robot en (300,300), heading 0, r=95. Solo front (Y) y right (X) validos.
    // Caso degradado donde F1a es CRITICO: no hay opuesto que cancele el radio.
    //   Sensor frontal en y=395 -> mide 1820-395=1425 a NORTH.
    //     dperp=(1425+95)=1520 -> y=1820-1520=300. CORRECTO.
    //   Sensor derecho (mount270->EAST) en x=395 -> mide 2430-395=2035 a EAST.
    //     dperp=(2035+95)=2130 -> x=2430-2130=300. CORRECTO.
    auto in  = make_inputs(/*front*/1425, /*back*/INV, /*right*/2035, /*left*/INV, 0);
    auto cfg = make_config();
    cfg.tof_offset_mm = 95;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(15, 300, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(15, 300, pose.y_mm);
}

// ============================================================
// F1b — proyeccion por coseno (robot rotado a angulo no-cardinal)
// ============================================================

void test_F1b_rotado_30_grados_proyecta_perpendicular(void) {
    // Robot centro (1215,910), heading=+3000 (30 grados CCW), r=0 (aislar F1b).
    // Cada ToF mide la HIPOTENUSA al rayo (perp/cos30):
    //   front  world30  -> NORTH, perp=910  -> mide 910/cos30 = 1051
    //   back   world210 -> SOUTH, perp=910  -> mide 1051
    //   right  world300 -> EAST,  perp=1215 -> mide 1215/cos30 = 1403
    //   left   world120 -> WEST,  perp=1215 -> mide 1403
    // Con F1b: dperp=hip*cos30 recupera la perpendicular -> centro (1215,910).
    // Control SIN F1b: y=1820-1051=769 (error 141mm).
    auto in  = make_inputs(/*front*/1051, /*back*/1051, /*right*/1403, /*left*/1403, /*h*/3000);
    auto cfg = make_config();  // tof_offset_mm = 0

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(20, 1215, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(20, 910,  pose.y_mm);
}

void test_F1b_control_sin_proyeccion_seria_erroneo(void) {
    // MISMO input que el caso 30 grados. Documenta el delta del control:
    //   SIN F1b: y = 1820 - 1051 = 769  (error 141mm respecto a 910)
    //            x = 2430 - 1403 = 1027 (error 188mm respecto a 1215)
    // CON F1b (el codigo actual): error < 20mm. Aca asertamos lo positivo y
    // verificamos que el resultado esta LEJOS del valor erroneo del control.
    auto in  = make_inputs(1051, 1051, 1403, 1403, 3000);
    auto cfg = make_config();

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    // CON F1b: cerca del centro real.
    TEST_ASSERT_INT16_WITHIN(20, 1215, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(20, 910,  pose.y_mm);
    // Y lejos del valor que daria SIN proyeccion (769/1027).
    TEST_ASSERT_TRUE(pose.y_mm - 769 > 100);
    TEST_ASSERT_TRUE(pose.x_mm - 1027 > 100);
}

void test_F1b_heading_cardinal_no_altera(void) {
    // Robot centro, heading=9000 (90 grados, cardinal). alpha=0 -> cos=1 -> F1b
    // es no-op. Distancias con mount canonico {0,180,270,90}, r=0:
    //   front world90  -> WEST,  x=d        -> 1215
    //   back  world270 -> EAST,  x=fw-d=1215 -> d=1215
    //   right world360=0 -> NORTH, y=fh-d=910 -> d=910
    //   left  world180 -> SOUTH, y=d=910     -> d=910
    auto in  = make_inputs(/*front*/1215, /*back*/1215, /*right*/910, /*left*/910, /*h*/9000);
    auto cfg = make_config();

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 1215, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 910,  pose.y_mm);
}

// ============================================================
// F1a + F1b combinados
// ============================================================

void test_F1a_y_F1b_combinados_rotado_15_con_offset(void) {
    // Robot centro (1215,910), heading=+1500 (15 grados), r=95.
    // Objetivo: dperp recupere la perpendicular del CENTRO a cada pared.
    //   Para Y (perp=910): (d+95)*cos15=910 -> d = 910/cos15 - 95 ~= 847
    //   Para X (perp=1215): (d+95)*cos15=1215 -> d = 1215/cos15 - 95 ~= 1163
    auto in  = make_inputs(/*front*/847, /*back*/847, /*right*/1163, /*left*/1163, /*h*/1500);
    auto cfg = make_config();
    cfg.tof_offset_mm = 95;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(20, 1215, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(20, 910,  pose.y_mm);
}

// ============================================================
// Sanidad de la LUT de coseno (indirecta, via casos conocidos)
// ============================================================

void test_cos_lut_sanidad_via_45_no_crash(void) {
    // cos_q12 esta en namespace anonimo (no exportado), asi que se valida
    // indirectamente: heading 45 (borde de cuadrante) no debe crashear ni
    // producir coordenadas negativas/absurdas. alpha=45 -> cos~0.707.
    auto in  = make_inputs(910, 910, 1215, 1215, 4500);
    auto cfg = make_config();

    auto pose = localization_compute(in, cfg);

    // No exigimos precision (el flip de clasificacion en 45 es preexistente),
    // solo que no crashee y que si es valido, las coords esten en rango fisico.
    if (pose.valid) {
        TEST_ASSERT_TRUE(pose.x_mm >= -200 && pose.x_mm <= 2630);
        TEST_ASSERT_TRUE(pose.y_mm >= -200 && pose.y_mm <= 2020);
    }
    TEST_ASSERT_TRUE(true);  // gate de no-crash
}

void test_cos_lut_monotona_via_proyeccion(void) {
    // Verifica que cos decrece con el angulo: a mayor alpha, mismo d cruda
    // produce dperp menor. Comparamos heading 0 (alpha=0) vs heading 30
    // (alpha=30) con UN solo sensor de cada eje, r=0, MISMA d cruda=910.
    // alpha=0  -> dperp=910        -> y=1820-910=910
    // alpha=30 -> dperp=910*cos30~788 -> y=1820-788=1032 (mayor que 910)
    auto cfg = make_config();

    auto in0  = make_inputs(/*front*/910, /*back*/INV, /*right*/1215, /*left*/1215, /*h*/0);
    auto p0   = localization_compute(in0, cfg);
    auto in30 = make_inputs(/*front*/910, /*back*/INV, /*right*/1215, /*left*/1215, /*h*/3000);
    auto p30  = localization_compute(in30, cfg);

    TEST_ASSERT_TRUE(p0.valid);
    TEST_ASSERT_TRUE(p30.valid);
    // alpha=0 da y=910; alpha=30 con misma d cruda da y mayor (dperp menor).
    TEST_ASSERT_INT16_WITHIN(10, 910, p0.y_mm);
    TEST_ASSERT_TRUE(p30.y_mm > p0.y_mm);  // cos(30) < cos(0)
}

// ============================================================
// Runner Unity
// ============================================================
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();
    // F1a
    RUN_TEST(test_F1a_un_solo_sensor_eje_Y_corrige_radio);
    RUN_TEST(test_F1a_dos_sensores_opuestos_siguen_cancelando);
    RUN_TEST(test_F1a_offset_cero_equivale_legacy);
    RUN_TEST(test_F1a_esquina_un_sensor_por_eje_con_offset);
    // F1b
    RUN_TEST(test_F1b_rotado_30_grados_proyecta_perpendicular);
    RUN_TEST(test_F1b_control_sin_proyeccion_seria_erroneo);
    RUN_TEST(test_F1b_heading_cardinal_no_altera);
    // F1a + F1b
    RUN_TEST(test_F1a_y_F1b_combinados_rotado_15_con_offset);
    // LUT sanidad
    RUN_TEST(test_cos_lut_sanidad_via_45_no_crash);
    RUN_TEST(test_cos_lut_monotona_via_proyeccion);
    return UNITY_END();
}
