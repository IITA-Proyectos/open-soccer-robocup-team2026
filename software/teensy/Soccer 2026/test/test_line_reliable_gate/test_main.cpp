// test_line_reliable_gate — compuerta de "lectura confiable" DOWN (§6.3-6.4 del diseño).
// Corre host: bash scripts/run-host-tests.sh test_line_reliable_gate
//
// Lo que VERIFICA, en orden de criticidad:
//   1. BYTE-IDENTIDAD con cfg default: data_valid == base_data_valid (camino vivo intacto).
//   2. Cada eje (B salud, C soporte) recorta confianza cuando supera el piso, NO antes.
//   3. La compuerta NUNCA es MÁS PERMISIVA que base (no inventa un valid=1 nuevo).
//   4. seal_geometry == !data_valid (la fuente sella la geometría cuando no es confiable).

#include <unity.h>
#include "line_reliable_gate.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Helper: arma RgInputs sin repetir el brace-init en cada test.
static RgInputs mk(bool base, int healthy, int validated) {
    RgInputs in;
    in.base_data_valid  = base;
    in.healthy_count    = healthy;
    in.validated_count  = validated;
    return in;
}

// ── 1. BYTE-IDENTIDAD: cfg default ⇒ data_valid == base_data_valid ─────────────────────

// rg_cfg_default() es transparente: base=1 pasa tal cual a data_valid=1.
void test_default_passthrough_valid_true(void) {
    RgResult r = rg_evaluate(mk(true, 10, 1), rg_cfg_default());
    TEST_ASSERT_TRUE(r.data_valid);
    TEST_ASSERT_FALSE(r.seal_geometry);   // valid ⇒ no sella
}

// base=0 pasa tal cual a data_valid=0 incluso con anillo sano y muchos validados:
// el default NO recupera confianza que base ya negó.
void test_default_passthrough_valid_false(void) {
    RgResult r = rg_evaluate(mk(false, 32, 32), rg_cfg_default());
    TEST_ASSERT_FALSE(r.data_valid);
    TEST_ASSERT_TRUE(r.seal_geometry);    // !valid ⇒ sella
}

// RgCfg zero-inicializado debe comportarse IGUAL que rg_cfg_default() (no-op total):
// healthy=0 y muy pocos sensores no deben bajar nada con los pisos en 0 y enabled=false.
void test_zero_init_cfg_is_noop(void) {
    RgCfg zero{};   // enabled=false, min_healthy=0, min_sensors_for_vector=0
    RgResult r1 = rg_evaluate(mk(true, 0, 0), zero);
    TEST_ASSERT_TRUE(r1.data_valid);            // base=1 → valid=1 aunque healthy/validated=0
    RgResult r0 = rg_evaluate(mk(false, 0, 0), zero);
    TEST_ASSERT_FALSE(r0.data_valid);           // base=0 → valid=0
}

// Con el gate ENABLED pero ambos pisos en 0, sigue siendo no-op (los AND son triviales):
// data_valid == base. Verifica que prender el master switch sin umbrales no cambia nada.
void test_enabled_but_zero_floors_is_noop(void) {
    RgCfg c; c.min_healthy = 0; c.min_sensors_for_vector = 0; c.enabled = true;
    TEST_ASSERT_TRUE (rg_evaluate(mk(true,  0, 0), c).data_valid);
    TEST_ASSERT_FALSE(rg_evaluate(mk(false, 0, 0), c).data_valid);
}

// ── 2. EJE B — piso de salud global (§6.4-B) ───────────────────────────────────────────

// healthy bajo el piso ⇒ data_valid=0 + seal, aunque base=1 y haya soporte de sobra.
void test_healthy_below_floor_invalidates(void) {
    RgCfg c; c.min_healthy = 24; c.min_sensors_for_vector = 0; c.enabled = true;
    RgResult r = rg_evaluate(mk(true, 23, 10), c);   // 23 < 24
    TEST_ASSERT_FALSE(r.data_valid);
    TEST_ASSERT_TRUE(r.seal_geometry);
}

// healthy EXACTAMENTE en el piso ⇒ pasa (corte >=, no >). El borde cuenta como sano.
void test_healthy_at_floor_passes(void) {
    RgCfg c; c.min_healthy = 24; c.min_sensors_for_vector = 0; c.enabled = true;
    RgResult r = rg_evaluate(mk(true, 24, 10), c);   // 24 >= 24
    TEST_ASSERT_TRUE(r.data_valid);
    TEST_ASSERT_FALSE(r.seal_geometry);
}

// ── 3. EJE C — soporte mínimo del vector (§6.3-5 / §6.4-C) ─────────────────────────────

// soporte bajo el piso ⇒ data_valid=0 + seal (1-2 sensores aislados no fundan un vector).
void test_support_below_floor_invalidates(void) {
    RgCfg c; c.min_healthy = 0; c.min_sensors_for_vector = 3; c.enabled = true;
    RgResult r = rg_evaluate(mk(true, 32, 2), c);    // 2 < 3
    TEST_ASSERT_FALSE(r.data_valid);
    TEST_ASSERT_TRUE(r.seal_geometry);
}

// soporte EXACTAMENTE en el piso ⇒ pasa (corte >=). 3 validados = dosis mínima de evidencia.
void test_support_at_floor_passes(void) {
    RgCfg c; c.min_healthy = 0; c.min_sensors_for_vector = 3; c.enabled = true;
    RgResult r = rg_evaluate(mk(true, 32, 3), c);    // 3 >= 3
    TEST_ASSERT_TRUE(r.data_valid);
    TEST_ASSERT_FALSE(r.seal_geometry);
}

// ── 4. NUNCA más permisivo que base + combinaciones ────────────────────────────────────

// base=0 con TODOS los pisos satisfechos: el gate jamás resucita confianza que base negó.
// Este es el invariante de seguridad load-bearing (§6.4 "solo resta, nunca suma").
void test_never_more_permissive_than_base(void) {
    RgCfg c; c.min_healthy = 24; c.min_sensors_for_vector = 3; c.enabled = true;
    RgResult r = rg_evaluate(mk(false, 32, 32), c);  // base=0 pero anillo perfecto
    TEST_ASSERT_FALSE(r.data_valid);
    TEST_ASSERT_TRUE(r.seal_geometry);
}

// Ambos ejes activos y AMBOS satisfechos con base=1 ⇒ valid=1 (caso confiable nominal).
void test_both_axes_pass(void) {
    RgCfg c; c.min_healthy = 24; c.min_sensors_for_vector = 3; c.enabled = true;
    RgResult r = rg_evaluate(mk(true, 30, 5), c);
    TEST_ASSERT_TRUE(r.data_valid);
    TEST_ASSERT_FALSE(r.seal_geometry);
}

// Un eje OK y el otro NO ⇒ invalida (AND, no OR). Salud sana pero soporte insuficiente.
void test_one_axis_fails_invalidates(void) {
    RgCfg c; c.min_healthy = 24; c.min_sensors_for_vector = 3; c.enabled = true;
    RgResult r = rg_evaluate(mk(true, 30, 1), c);    // salud OK, soporte 1 < 3
    TEST_ASSERT_FALSE(r.data_valid);
    TEST_ASSERT_TRUE(r.seal_geometry);
}

// Pisos seteados pero enabled=false ⇒ los pisos se IGNORAN, vuelve a ser passthrough.
// Garantiza que el master switch domina (un cfg con umbrales pero apagado = camino vivo).
void test_disabled_ignores_floors(void) {
    RgCfg c; c.min_healthy = 24; c.min_sensors_for_vector = 3; c.enabled = false;
    RgResult r = rg_evaluate(mk(true, 0, 0), c);     // healthy/soporte=0 pero gate OFF
    TEST_ASSERT_TRUE(r.data_valid);                   // == base, los pisos no aplican
    TEST_ASSERT_FALSE(r.seal_geometry);
}

// seal_geometry es SIEMPRE el complemento exacto de data_valid, en todas las ramas.
void test_seal_is_complement_of_valid(void) {
    RgCfg c; c.min_healthy = 24; c.min_sensors_for_vector = 3; c.enabled = true;
    // Caso valid
    RgResult ok = rg_evaluate(mk(true, 30, 5), c);
    TEST_ASSERT_EQUAL(!ok.data_valid, ok.seal_geometry);
    // Caso invalid por base
    RgResult bad = rg_evaluate(mk(false, 30, 5), c);
    TEST_ASSERT_EQUAL(!bad.data_valid, bad.seal_geometry);
    // Caso default
    RgResult def = rg_evaluate(mk(true, 0, 0), rg_cfg_default());
    TEST_ASSERT_EQUAL(!def.data_valid, def.seal_geometry);
}

int main(int, char**) {
    UNITY_BEGIN();
    // 1. Byte-identidad
    RUN_TEST(test_default_passthrough_valid_true);
    RUN_TEST(test_default_passthrough_valid_false);
    RUN_TEST(test_zero_init_cfg_is_noop);
    RUN_TEST(test_enabled_but_zero_floors_is_noop);
    // 2. Eje B — salud
    RUN_TEST(test_healthy_below_floor_invalidates);
    RUN_TEST(test_healthy_at_floor_passes);
    // 3. Eje C — soporte
    RUN_TEST(test_support_below_floor_invalidates);
    RUN_TEST(test_support_at_floor_passes);
    // 4. Invariantes + combinaciones
    RUN_TEST(test_never_more_permissive_than_base);
    RUN_TEST(test_both_axes_pass);
    RUN_TEST(test_one_axis_fails_invalidates);
    RUN_TEST(test_disabled_ignores_floors);
    RUN_TEST(test_seal_is_complement_of_valid);
    return UNITY_END();
}
