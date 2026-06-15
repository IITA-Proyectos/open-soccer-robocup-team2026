// test_motor_slew — limitador de slew/rampa por eje (Capa 2 del lazo RT CENTRAL).
// Corre host con: bash scripts/run-host-tests.sh test_motor_slew
//
// Cubre el módulo PURO motor_slew.h. Verifica:
//   • un SALTO grande se recorta a max_delta en un tick (subiendo y bajando);
//   • varios ticks CONVERGEN exactamente al objetivo y ahí se quedan;
//   • el BYPASS (slew_force) salta directo al target sin recortar;
//   • slew_reset deja todo en 0;
//   • un salto chico (<= max_delta) llega en un solo tick;
//   • max_delta por eje independiente (un eje rampa, otro llega);
//   • max_delta <= 0 CONGELA el eje (no se mueve);
//   • nunca sobrepasa el objetivo (no oscila);
//   • magnitud: una config negativa de max_delta se trata como |valor|.

#include <unity.h>
#include "motor_slew.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Helper: arranca un estado en cero.
static SlewState fresh() {
    SlewState s;
    slew_reset(s);
    return s;
}

// ── SALTO GRANDE RECORTADO ────────────────────────────────────────────────────

// Subiendo: prev=0, cmd=1000, max_delta=100 → salida 100 (recortada), no 1000.
void test_salto_grande_sube_recortado(void) {
    SlewState s = fresh();
    SlewCfg cfg{ {100, 100, 100} };
    int16_t cmd[3] = {1000, 0, 0};
    slew_apply(s, cmd, cfg);
    TEST_ASSERT_EQUAL_INT16(100, cmd[0]);
    TEST_ASSERT_EQUAL_INT16(100, s.prev[0]);   // estado guardado para el próximo tick
}

// Bajando: prev=500, cmd=-500, max_delta=100 → salida 400 (un paso hacia abajo).
void test_salto_grande_baja_recortado(void) {
    SlewState s = fresh();
    s.prev[0] = 500;
    SlewCfg cfg{ {100, 100, 100} };
    int16_t cmd[3] = {-500, 0, 0};
    slew_apply(s, cmd, cfg);
    TEST_ASSERT_EQUAL_INT16(400, cmd[0]);      // 500 - 100
}

// El SENTIDO del cambio es correcto en ambas direcciones desde el mismo arranque.
void test_sentido_sube_y_baja(void) {
    SlewCfg cfg{ {50, 50, 50} };
    // Sube
    { SlewState s = fresh(); s.prev[0] = 200; int16_t c[3] = {1000,0,0};
      slew_apply(s, c, cfg); TEST_ASSERT_EQUAL_INT16(250, c[0]); }
    // Baja
    { SlewState s = fresh(); s.prev[0] = 200; int16_t c[3] = {-1000,0,0};
      slew_apply(s, c, cfg); TEST_ASSERT_EQUAL_INT16(150, c[0]); }
}

// ── CONVERGENCIA EN VARIOS TICKS ──────────────────────────────────────────────

// prev=0 → cmd=250, max_delta=100. Ticks: 100, 200, 250 (llega exacto), 250 (se queda).
void test_varios_ticks_convergen(void) {
    SlewState s = fresh();
    SlewCfg cfg{ {100, 100, 100} };

    int16_t c1[3] = {250, 0, 0}; slew_apply(s, c1, cfg);
    TEST_ASSERT_EQUAL_INT16(100, c1[0]);

    int16_t c2[3] = {250, 0, 0}; slew_apply(s, c2, cfg);
    TEST_ASSERT_EQUAL_INT16(200, c2[0]);

    int16_t c3[3] = {250, 0, 0}; slew_apply(s, c3, cfg);
    TEST_ASSERT_EQUAL_INT16(250, c3[0]);       // último tramo (50 <= 100) → exacto

    int16_t c4[3] = {250, 0, 0}; slew_apply(s, c4, cfg);
    TEST_ASSERT_EQUAL_INT16(250, c4[0]);       // ya en destino, no sobrepasa
}

// Nunca SOBREPASA: en el tick donde el resto (50) es menor que el paso (100),
// la salida es el target exacto, no target+algo.
void test_no_sobrepasa_objetivo(void) {
    SlewState s = fresh();
    s.prev[0] = 90;
    SlewCfg cfg{ {100, 100, 100} };
    int16_t cmd[3] = {100, 0, 0};              // resto = 10 < 100
    slew_apply(s, cmd, cfg);
    TEST_ASSERT_EQUAL_INT16(100, cmd[0]);      // clava el 100, no 190
}

// ── SALTO CHICO LLEGA EN UN TICK ──────────────────────────────────────────────

void test_salto_chico_un_tick(void) {
    SlewState s = fresh();
    SlewCfg cfg{ {100, 100, 100} };
    int16_t cmd[3] = {80, -80, 100};           // todos |.| <= 100
    slew_apply(s, cmd, cfg);
    TEST_ASSERT_EQUAL_INT16(80,  cmd[0]);
    TEST_ASSERT_EQUAL_INT16(-80, cmd[1]);
    TEST_ASSERT_EQUAL_INT16(100, cmd[2]);      // borde exacto (diff == step) → llega
}

// ── BYPASS (REFLEJO) ──────────────────────────────────────────────────────────

// slew_force salta directo al target: el slew_apply siguiente devuelve el target
// sin recortar (la rampa quedó "teletransportada" al destino).
void test_bypass_salta_directo(void) {
    SlewState s = fresh();                      // prev = {0,0,0}
    SlewCfg cfg{ {10, 10, 10} };               // rampa MUY lenta a propósito
    int16_t target[3] = {1000, -1000, 500};
    slew_force(s, target);                      // BYPASS
    TEST_ASSERT_EQUAL_INT16(1000,  s.prev[0]);
    TEST_ASSERT_EQUAL_INT16(-1000, s.prev[1]);
    TEST_ASSERT_EQUAL_INT16(500,   s.prev[2]);

    // El próximo apply con el mismo target NO recorta (ya estamos ahí).
    int16_t cmd[3] = {1000, -1000, 500};
    slew_apply(s, cmd, cfg);
    TEST_ASSERT_EQUAL_INT16(1000,  cmd[0]);
    TEST_ASSERT_EQUAL_INT16(-1000, cmd[1]);
    TEST_ASSERT_EQUAL_INT16(500,   cmd[2]);
}

// slew_reset deja los 3 ejes en cero (STOP del árbitro / boot).
void test_reset_pone_en_cero(void) {
    SlewState s = fresh();
    s.prev[0] = 123; s.prev[1] = -45; s.prev[2] = 999;
    slew_reset(s);
    TEST_ASSERT_EQUAL_INT16(0, s.prev[0]);
    TEST_ASSERT_EQUAL_INT16(0, s.prev[1]);
    TEST_ASSERT_EQUAL_INT16(0, s.prev[2]);
}

// ── EJES INDEPENDIENTES ───────────────────────────────────────────────────────

// Cada eje rampa con su propio max_delta: el eje 0 recorta, el eje 1 llega de una.
void test_ejes_independientes(void) {
    SlewState s = fresh();
    SlewCfg cfg{ {20, 1000, 100} };            // eje0 lento, eje1 rápido
    int16_t cmd[3] = {500, 500, 30};
    slew_apply(s, cmd, cfg);
    TEST_ASSERT_EQUAL_INT16(20,  cmd[0]);      // recortado a su paso chico
    TEST_ASSERT_EQUAL_INT16(500, cmd[1]);      // 500 <= 1000 → llega entero
    TEST_ASSERT_EQUAL_INT16(30,  cmd[2]);      // 30 <= 100 → llega
}

// ── BORDES DE CONFIG ──────────────────────────────────────────────────────────

// max_delta <= 0 CONGELA el eje (no se mueve de prev). Para salto inmediato real
// existe slew_force, no un delta de 0.
void test_max_delta_cero_congela(void) {
    SlewState s = fresh();
    s.prev[0] = 77;
    SlewCfg cfg{ {0, 0, 0} };
    int16_t cmd[3] = {1000, -1000, 12};
    slew_apply(s, cmd, cfg);
    TEST_ASSERT_EQUAL_INT16(77, cmd[0]);       // se queda en prev
    TEST_ASSERT_EQUAL_INT16(0,  cmd[1]);
    TEST_ASSERT_EQUAL_INT16(0,  cmd[2]);
}

// max_delta NEGATIVO se trata como magnitud (|−100| == 100): mismo recorte.
void test_max_delta_negativo_es_magnitud(void) {
    SlewState s = fresh();
    SlewCfg cfg{ {-100, -100, -100} };
    int16_t cmd[3] = {1000, 0, 0};
    slew_apply(s, cmd, cfg);
    TEST_ASSERT_EQUAL_INT16(100, cmd[0]);      // igual que con +100
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_salto_grande_sube_recortado);
    RUN_TEST(test_salto_grande_baja_recortado);
    RUN_TEST(test_sentido_sube_y_baja);
    RUN_TEST(test_varios_ticks_convergen);
    RUN_TEST(test_no_sobrepasa_objetivo);
    RUN_TEST(test_salto_chico_un_tick);
    RUN_TEST(test_bypass_salta_directo);
    RUN_TEST(test_reset_pone_en_cero);
    RUN_TEST(test_ejes_independientes);
    RUN_TEST(test_max_delta_cero_congela);
    RUN_TEST(test_max_delta_negativo_es_magnitud);
    return UNITY_END();
}
