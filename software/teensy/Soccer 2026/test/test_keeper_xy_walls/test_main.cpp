// test_keeper_xy_walls — estimador XY heading-free del arquero por paredes.
// Puro, host-testeable. Fixtures = capturas REALES de banco (TOP, 2026-06-18/19).
// Corre con: bash scripts/run-host-tests.sh test_keeper_xy_walls
//        o:  pio test -e test_native -f test_keeper_xy_walls

#include <unity.h>
#include "keeper_xy_walls.h"

using namespace iitasoccer;

static const uint16_t NR = KEEPER_TOF_NO_READING;
static const uint8_t  TRIM = 35;   // recorte simetrico (+-35% de la mediana)

// Config de banco: cancha 1820 ancho, offset 95, alcance 1000, consistencia 200, simetria 150.
static KeeperXYConfig cfg() {
    KeeperXYConfig c;
    c.field_width_mm = 1820;
    c.tof_offset_mm  = 95;
    c.wall_reach_mm  = 1000;
    c.consistency_mm = 200;
    c.symmetric_mm   = 150;
    return c;
}

void setUp(void) {}
void tearDown(void) {}

// ===========================================================================
// 1) REDUCCION de 16 zonas -> distancia de pared (capturas reales)
// ===========================================================================

// Izquierdo TOCANDO la linea izquierda: pared ~224, todas las zonas uniformes.
void test_reduce_left_touching(void) {
    const uint16_t z[16] = {235,229,222,219, 241,227,213,210,
                            242,228,213,214, 237,236,222,222};
    const uint16_t d = keeper_wall_dist_mm(z, 16, NR, TRIM);
    TEST_ASSERT_UINT16_WITHIN(4, 225, d);
}

// Izquierdo a ~50 cm: piso (~334) + PARED (~545, 8 zonas) + por-encima (~975).
// La mediana+recorte tiene que quedarse con la PARED, no contaminarse.
void test_reduce_left_50cm_wall_not_floor_nor_over(void) {
    const uint16_t z[16] = {327,533,564,901, 337,527,560,920,
                            339,527,545,1053, 335,513,586,1026};
    const uint16_t d = keeper_wall_dist_mm(z, 16, NR, TRIM);
    TEST_ASSERT_UINT16_WITHIN(8, 544, d);   // pared, NO ~688 (lo que daria el reductor viejo)
}

// Derecho a ~32 cm con 2 picos espurios (~1090): hay que ignorarlos.
void test_reduce_right_rejects_spurious_spikes(void) {
    const uint16_t z[16] = {219,223,231,226, 245,225,235,238,
                            1092,216,237,244, 1083,217,227,230};
    const uint16_t d = keeper_wall_dist_mm(z, 16, NR, TRIM);
    TEST_ASSERT_UINT16_WITHIN(5, 229, d);
}

// Trasero tocando el fondo: pared propia ~213, uniforme.
void test_reduce_back_touching(void) {
    const uint16_t z[16] = {224,225,219,210, 238,220,211,214,
                            236,217,209,194, 191,203,197,204};
    const uint16_t d = keeper_wall_dist_mm(z, 16, NR, TRIM);
    TEST_ASSERT_UINT16_WITHIN(4, 213, d);
}

// Con 65535 (zonas enmascaradas) se ignoran; reduce sobre las validas.
void test_reduce_ignores_no_reading(void) {
    const uint16_t z[16] = {NR,NR,540,560, NR,NR,545,555,
                            NR,NR,550,548, NR,NR,552,544};
    const uint16_t d = keeper_wall_dist_mm(z, 16, NR, TRIM);
    TEST_ASSERT_UINT16_WITHIN(6, 549, d);
}

// Todas invalidas -> no_reading.
void test_reduce_all_invalid(void) {
    const uint16_t z[4] = {NR,NR,NR,NR};
    TEST_ASSERT_EQUAL_UINT16(NR, keeper_wall_dist_mm(z, 4, NR, TRIM));
}

// ===========================================================================
// 2) (x,y) a partir de distancias ya reducidas
// ===========================================================================

// Robot pegado a la pared IZQUIERDA: x ~ 224+95 = 319; sin trasera valida -> y invalida.
void test_xy_near_left_wall(void) {
    KeeperWallDist d{224, NR, NR};
    KeeperXY r = keeper_xy_from_walls(d, cfg());
    TEST_ASSERT_TRUE(r.x_valid);
    TEST_ASSERT_INT16_WITHIN(5, 319, r.x_mm);
    TEST_ASSERT_FALSE(r.y_valid);
}

// Esquina FONDO-DERECHA real: izq contaminada a ~416 (piso+algo), der real ~229,
// atras ~213. La X tiene que confiar en la DERECHA (cercana) -> x ~ 1496; y ~ 308.
void test_xy_back_right_corner_trusts_near_wall(void) {
    KeeperWallDist d{416, 229, 213};
    KeeperXY r = keeper_xy_from_walls(d, cfg());
    TEST_ASSERT_TRUE(r.x_valid);
    TEST_ASSERT_INT16_WITHIN(8, 1496, r.x_mm);   // 1820-(229+95)
    TEST_ASSERT_TRUE(r.y_valid);
    TEST_ASSERT_INT16_WITHIN(5, 308, r.y_mm);    // 213+95
}

// Centrado con ambas laterales alcanzables y consistentes -> promedio ~910.
void test_xy_centered_consistent_averages(void) {
    KeeperWallDist d{815, 815, NR};   // xL=910, xR=1820-910=910
    KeeperXY r = keeper_xy_from_walls(d, cfg());
    TEST_ASSERT_TRUE(r.x_valid);
    TEST_ASSERT_INT16_WITHIN(5, 910, r.x_mm);
}

// Centrado pero ambas leen PISO (~350) -> x no cierra y leen ~lo mismo -> AMBIGUO.
void test_xy_centered_floor_is_ambiguous(void) {
    KeeperWallDist d{350, 350, 300};
    KeeperXY r = keeper_xy_from_walls(d, cfg());
    TEST_ASSERT_FALSE(r.x_valid);   // honesto: centrado sobre piso = X incierta
    TEST_ASSERT_TRUE(r.y_valid);    // pero la Y (trasera) sigue valida
}

// Pared lateral fuera de alcance (lejana) -> ese lado no cuenta.
void test_xy_far_wall_ignored(void) {
    KeeperWallDist d{NR, 2500, 250};   // der "2500" > wall_reach -> no ok
    KeeperXY r = keeper_xy_from_walls(d, cfg());
    TEST_ASSERT_FALSE(r.x_valid);
    TEST_ASSERT_TRUE(r.y_valid);
}

// Trasera fuera de alcance (arquero lejos del fondo) -> Y invalida.
void test_xy_back_out_of_reach_y_invalid(void) {
    KeeperWallDist d{224, NR, 1500};
    KeeperXY r = keeper_xy_from_walls(d, cfg());
    TEST_ASSERT_TRUE(r.x_valid);
    TEST_ASSERT_FALSE(r.y_valid);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_reduce_left_touching);
    RUN_TEST(test_reduce_left_50cm_wall_not_floor_nor_over);
    RUN_TEST(test_reduce_right_rejects_spurious_spikes);
    RUN_TEST(test_reduce_back_touching);
    RUN_TEST(test_reduce_ignores_no_reading);
    RUN_TEST(test_reduce_all_invalid);
    RUN_TEST(test_xy_near_left_wall);
    RUN_TEST(test_xy_back_right_corner_trusts_near_wall);
    RUN_TEST(test_xy_centered_consistent_averages);
    RUN_TEST(test_xy_centered_floor_is_ambiguous);
    RUN_TEST(test_xy_far_wall_ignored);
    RUN_TEST(test_xy_back_out_of_reach_y_invalid);
    return UNITY_END();
}
