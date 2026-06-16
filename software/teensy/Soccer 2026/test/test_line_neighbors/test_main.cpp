// test_line_neighbors — tests unitarios para src/shared/line_neighbors.h
// Corre en host con: pio test -e test_native -f test_line_neighbors
// (o: bash scripts/run-host-tests.sh test_line_neighbors)
//
// Lo que prueba (F3, §4.2 del diseño RT de DOWN):
//   - VECINO FÍSICO ≠ vecino de índice: los pares contiguos-de-índice pero
//     LEJOS (7↔8=141mm, 23↔24=116mm, 31↔0=102mm) NO salen como vecinos;
//   - cada sensor y su vecino REAL están a <~30mm;
//   - los sensores internos aislados tienen 1 solo vecino;
//   - simetría, helpers (ln_is_neighbor / ln_adjacent / ln_has_white_neighbor);
//   - guardas (nullptr, n<=0, radio).

#include <unity.h>
#include <cmath>
#include "line_neighbors.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Geometría REAL del PCB DOWN — copia byte-idéntica de SENSOR_POS
// (sensor_geometry.cpp:10-52). Se copia acá a propósito: el test del módulo
// PURO no debe depender de sensor_geometry.cpp. Si esas coords cambian, este
// array debe cambiar igual (y entonces los números esperados se recomputan).
// ============================================================================
static const LnSensorPos DOWN_POS[32] = {
    {-36.28f, +82.04f}, {-30.57f, +75.06f}, {-20.28f, +74.17f}, {-10.12f, +74.17f},
    {+10.20f, +74.17f}, {+20.36f, +74.17f}, {+30.52f, +75.18f}, {+36.49f, +82.04f},
    {-86.32f, +12.19f}, {-86.32f,  -0.51f}, {-84.92f, -13.21f}, {-81.24f, -26.04f},
    {-67.65f, -50.04f}, {-60.03f, -60.20f}, {-48.98f, -69.09f}, {-36.28f, -76.71f},
    {+36.36f, -76.71f}, {+49.31f, -69.09f}, {+60.87f, -60.20f}, {+69.63f, -50.04f},
    {+82.21f, -25.91f}, {+85.64f, -13.21f}, {+87.29f,  -0.51f}, {+86.40f, +12.06f},
    {-22.82f, +50.93f}, {-12.66f, +50.93f}, {+12.74f, +51.05f}, {+22.90f, +50.93f},
    {+34.55f,  -9.51f}, {+34.55f, -19.67f}, {-33.25f,  -9.51f}, {-33.25f, -19.80f},
};

static float dist(int a, int b) {
    return std::hypot(DOWN_POS[a].x_mm - DOWN_POS[b].x_mm,
                      DOWN_POS[a].y_mm - DOWN_POS[b].y_mm);
}

// ============================================================================
// 1) TEST-REGRESIÓN OBLIGATORIO — el vecino FÍSICO ≠ vecino de ÍNDICE
//    Los pares contiguos de índice pero LEJOS (down_model.cpp:245-246) NO deben
//    salir como vecinos.
// ============================================================================

void test_index_contiguous_but_far_are_NOT_neighbors(void) {
    LineNeighbors lut;
    ln_build(DOWN_POS, 32, &lut);

    // Primero confirmamos que las distancias son las del diseño (sanity de coords).
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 141.3f, dist(7, 8));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 115.9f, dist(23, 24));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 101.9f, dist(31, 0));

    // Y ahora lo que importa: NO son vecinos (ni en una dirección ni en la otra).
    TEST_ASSERT_FALSE(ln_is_neighbor(&lut, 7, 8));
    TEST_ASSERT_FALSE(ln_is_neighbor(&lut, 8, 7));
    TEST_ASSERT_FALSE(ln_adjacent(&lut, 7, 8));

    TEST_ASSERT_FALSE(ln_is_neighbor(&lut, 23, 24));
    TEST_ASSERT_FALSE(ln_is_neighbor(&lut, 24, 23));
    TEST_ASSERT_FALSE(ln_adjacent(&lut, 23, 24));

    TEST_ASSERT_FALSE(ln_is_neighbor(&lut, 31, 0));
    TEST_ASSERT_FALSE(ln_is_neighbor(&lut, 0, 31));
    TEST_ASSERT_FALSE(ln_adjacent(&lut, 31, 0));
}

// El vecino REAL de cada uno de esos índices es OTRO (el cercano físico real).
void test_real_neighbors_of_the_tricky_indices(void) {
    LineNeighbors lut;
    ln_build(DOWN_POS, 32, &lut);

    // idx 7: su vecino físico es idx 6 (9.1mm), NO idx 8 (141mm).
    TEST_ASSERT_TRUE(ln_is_neighbor(&lut, 7, 6));
    // idx 8: su vecino físico es idx 9 (12.7mm), NO idx 7.
    TEST_ASSERT_TRUE(ln_is_neighbor(&lut, 8, 9));
    // idx 0: su vecino físico es idx 1 (9.0mm), NO idx 31.
    TEST_ASSERT_TRUE(ln_is_neighbor(&lut, 0, 1));
    // idx 31: su vecino físico es idx 30 (10.3mm), NO idx 0.
    TEST_ASSERT_TRUE(ln_is_neighbor(&lut, 31, 30));
    // idx 24: su vecino físico es idx 25 (10.2mm), NO idx 23.
    TEST_ASSERT_TRUE(ln_is_neighbor(&lut, 24, 25));
}

// ============================================================================
// 2) Cada sensor y su(s) vecino(s) REAL(es) están a <~30mm
// ============================================================================

void test_every_neighbor_is_within_radius(void) {
    LineNeighbors lut;
    ln_build(DOWN_POS, 32, &lut);

    for (int i = 0; i < 32; ++i) {
        for (int k = 0; k < lut.count[i]; ++k) {
            const int j = lut.idx[i][k];
            TEST_ASSERT_TRUE(j >= 0 && j < 32);
            // Vecino real => físicamente cerca. Margen del radio default (30mm).
            TEST_ASSERT_TRUE_MESSAGE(dist(i, j) <= LN_DEFAULT_RADIUS_MM,
                                     "vecino fuera del radio de corte");
        }
    }
}

// Todo sensor del anillo tiene al menos 1 vecino físico (ninguno queda huérfano:
// el más cercano de CADA sensor está a <=15mm, bien dentro de 30mm).
void test_every_sensor_has_at_least_one_neighbor(void) {
    LineNeighbors lut;
    ln_build(DOWN_POS, 32, &lut);
    for (int i = 0; i < 32; ++i) {
        TEST_ASSERT_TRUE_MESSAGE(lut.count[i] >= 1, "sensor sin vecino fisico");
    }
}

// El vecino #0 (más cercano) coincide con el mínimo real de distancia.
void test_closest_neighbor_is_the_physical_minimum(void) {
    LineNeighbors lut;
    ln_build(DOWN_POS, 32, &lut);
    for (int i = 0; i < 32; ++i) {
        // mínimo absoluto por fuerza bruta
        int argmin = -1; float dmin = 1e9f;
        for (int j = 0; j < 32; ++j) {
            if (j == i) continue;
            float d = dist(i, j);
            if (d < dmin) { dmin = d; argmin = j; }
        }
        // El vecino #0 de la LUT debe ser ese mínimo (todos los mínimos están
        // dentro del radio, así que siempre entran).
        TEST_ASSERT_EQUAL_INT(argmin, lut.idx[i][0]);
    }
}

// ============================================================================
// 3) Sensores internos aislados → 1 solo vecino
//    idx 28-31 (anillo interno) tienen su 2º candidato a ~45-52mm > 30mm.
// ============================================================================

void test_inner_isolated_sensors_have_single_neighbor(void) {
    LineNeighbors lut;
    ln_build(DOWN_POS, 32, &lut);

    // idx 28 <-> 29 son par (10.2mm); el siguiente de 28 está a ~50mm (idx 20).
    TEST_ASSERT_EQUAL_INT8(1, lut.count[28]);
    TEST_ASSERT_EQUAL_INT(29, lut.idx[28][0]);

    TEST_ASSERT_EQUAL_INT8(1, lut.count[29]);
    TEST_ASSERT_EQUAL_INT(28, lut.idx[29][0]);

    // idx 30 <-> 31 son par (10.3mm); el siguiente está a ~45mm.
    TEST_ASSERT_EQUAL_INT8(1, lut.count[30]);
    TEST_ASSERT_EQUAL_INT(31, lut.idx[30][0]);

    TEST_ASSERT_EQUAL_INT8(1, lut.count[31]);
    TEST_ASSERT_EQUAL_INT(30, lut.idx[31][0]);
}

// Un sensor del anillo externo SÍ tiene 2 vecinos (densamente poblado).
void test_outer_dense_sensor_has_two_neighbors(void) {
    LineNeighbors lut;
    ln_build(DOWN_POS, 32, &lut);
    // idx 1: vecinos 0(9.0) y 2(10.3), ambos < 30mm.
    TEST_ASSERT_EQUAL_INT8(2, lut.count[1]);
    TEST_ASSERT_TRUE(ln_is_neighbor(&lut, 1, 0));
    TEST_ASSERT_TRUE(ln_is_neighbor(&lut, 1, 2));
    // Y ordenados por cercanía: el #0 es el más cercano (idx 0 a 9.0mm).
    TEST_ASSERT_EQUAL_INT(0, lut.idx[1][0]);
}

// ============================================================================
// 4) ln_adjacent es simétrico; ln_has_white_neighbor reemplaza i±1
// ============================================================================

void test_adjacent_is_symmetric(void) {
    LineNeighbors lut;
    ln_build(DOWN_POS, 32, &lut);
    // Para todo par marcado vecino en una dirección, ln_adjacent da true en ambas.
    for (int i = 0; i < 32; ++i) {
        for (int k = 0; k < lut.count[i]; ++k) {
            int j = lut.idx[i][k];
            TEST_ASSERT_TRUE(ln_adjacent(&lut, i, j));
            TEST_ASSERT_TRUE(ln_adjacent(&lut, j, i));
        }
    }
}

void test_has_white_neighbor_uses_physical_layout(void) {
    LineNeighbors lut;
    ln_build(DOWN_POS, 32, &lut);

    bool white[32] = {false};
    // Encendemos SOLO idx 8 (vecino de índice de 7, pero físicamente a 141mm).
    white[8] = true;
    // idx 7 NO debe ver vecino blanco: idx 8 no es su vecino físico.
    TEST_ASSERT_FALSE(ln_has_white_neighbor(&lut, white, 7, 32));
    // idx 9 (vecino físico real de 8) SÍ ve a 8 en blanco.
    TEST_ASSERT_TRUE(ln_has_white_neighbor(&lut, white, 9, 32));

    // Ahora encendemos idx 6 (vecino físico real de 7).
    bool white2[32] = {false};
    white2[6] = true;
    TEST_ASSERT_TRUE(ln_has_white_neighbor(&lut, white2, 7, 32));
}

// ============================================================================
// 5) Radio configurable y guardas
// ============================================================================

void test_radius_zero_gives_no_neighbors(void) {
    LineNeighbors lut;
    ln_build(DOWN_POS, 32, &lut, 0.0f);   // radio 0 => nada cuenta
    for (int i = 0; i < 32; ++i) {
        TEST_ASSERT_EQUAL_INT8(0, lut.count[i]);
        TEST_ASSERT_EQUAL_INT(-1, lut.idx[i][0]);
    }
}

void test_huge_radius_fills_max_neighbors(void) {
    LineNeighbors lut;
    ln_build(DOWN_POS, 32, &lut, 1000.0f);  // todo entra dentro del radio
    // Con radio enorme, cada sensor llena LN_MAX_NEIGHBORS slots (hay >=2 sensores).
    for (int i = 0; i < 32; ++i) {
        TEST_ASSERT_EQUAL_INT8(LN_MAX_NEIGHBORS, lut.count[i]);
    }
    // Aun así, los dos más cercanos de idx 7 son 6 y 5 (NO el 8, que sigue siendo
    // el más lejano relevante): el orden por distancia se mantiene.
    TEST_ASSERT_EQUAL_INT(6, lut.idx[7][0]);
    TEST_ASSERT_EQUAL_INT(5, lut.idx[7][1]);
    // 8 sigue sin ser vecino aunque el radio sea enorme (no entra en el top-2).
    TEST_ASSERT_FALSE(ln_is_neighbor(&lut, 7, 8));
}

void test_null_and_zero_guards(void) {
    LineNeighbors lut;

    // out == nullptr: no crashea (no hace nada).
    ln_build(DOWN_POS, 32, nullptr);

    // pos == nullptr: LUT queda vacía y segura.
    ln_build(nullptr, 32, &lut);
    for (int i = 0; i < LN_MAX_SENSORS; ++i) {
        TEST_ASSERT_EQUAL_INT8(0, lut.count[i]);
        TEST_ASSERT_EQUAL_INT(-1, lut.idx[i][0]);
    }

    // n <= 0: idem vacía.
    ln_build(DOWN_POS, 0, &lut);
    TEST_ASSERT_EQUAL_INT8(0, lut.count[0]);

    // Helpers con lut nullptr / índices fuera de rango → false (nunca crashean).
    TEST_ASSERT_FALSE(ln_is_neighbor(nullptr, 0, 1));
    TEST_ASSERT_FALSE(ln_adjacent(nullptr, 0, 1));
    TEST_ASSERT_FALSE(ln_has_white_neighbor(nullptr, nullptr, 0, 32));
    ln_build(DOWN_POS, 32, &lut);
    TEST_ASSERT_FALSE(ln_is_neighbor(&lut, -1, 0));
    TEST_ASSERT_FALSE(ln_is_neighbor(&lut, 0, 99));
}

// n menor que el array: solo se consideran los primeros n sensores.
void test_partial_n_only_considers_first_n(void) {
    LineNeighbors lut;
    ln_build(DOWN_POS, 8, &lut);   // solo los 8 frontales
    // idx 0..7 tienen vecinos; idx 8.. quedan en 0 (no se procesaron).
    TEST_ASSERT_TRUE(lut.count[0] >= 1);
    TEST_ASSERT_EQUAL_INT8(0, lut.count[8]);
    // Y ningún vecino de los procesados apunta a un índice >= 8.
    for (int i = 0; i < 8; ++i)
        for (int k = 0; k < lut.count[i]; ++k)
            TEST_ASSERT_TRUE(lut.idx[i][k] < 8);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();

    // 1) regresión: vecino físico != vecino de índice
    RUN_TEST(test_index_contiguous_but_far_are_NOT_neighbors);
    RUN_TEST(test_real_neighbors_of_the_tricky_indices);

    // 2) vecinos dentro del radio, mínimo correcto
    RUN_TEST(test_every_neighbor_is_within_radius);
    RUN_TEST(test_every_sensor_has_at_least_one_neighbor);
    RUN_TEST(test_closest_neighbor_is_the_physical_minimum);

    // 3) internos aislados vs externos densos
    RUN_TEST(test_inner_isolated_sensors_have_single_neighbor);
    RUN_TEST(test_outer_dense_sensor_has_two_neighbors);

    // 4) simetría + helper de blanco-vecino
    RUN_TEST(test_adjacent_is_symmetric);
    RUN_TEST(test_has_white_neighbor_uses_physical_layout);

    // 5) radio y guardas
    RUN_TEST(test_radius_zero_gives_no_neighbors);
    RUN_TEST(test_huge_radius_fills_max_neighbors);
    RUN_TEST(test_null_and_zero_guards);
    RUN_TEST(test_partial_n_only_considers_first_n);

    return UNITY_END();
}
