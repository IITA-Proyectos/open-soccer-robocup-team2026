// test_central_gk_cross_track — corre con:
//   pio test -e test_native -f test_central_gk_cross_track
//   (o: bash scripts/run-host-tests.sh test_central_gk_cross_track)
//
// WP-3-GK (Capa 3, lado CENTRAL). El ARQUERO se desplaza PARALELO a la línea
// usando `cross_track` (distancia perpendicular firmada del centro del robot a
// la recta de la línea). Este test fija el CONTRATO PURO que consumen, sobre el
// mismo `LineStatusV2`, los dos accessors nuevos de world_model.cpp:
//
//   • world_model_get_cross_track_mm()  -> lsv2_cross_track_mm(g_line)
//   • world_model_cross_track_valid()   -> (data_valid && cross_track != N/A)
//
// world_model.cpp NO se puede compilar host (incluye Arduino.h via millis()), así
// que —igual que test_central_line_ingest hace con los demás campos de línea—
// probamos los helpers PUROS de line_view.h que esos accessors delegan, a través
// del chain real encode->decode (lo mismo que drena comm_down del CENTRAL).
//
// El gate de validez (data_valid && cross_track != N/A) es EXACTAMENTE la señal
// que strategy.cpp (goalkeeper_tick) usa para decidir entre el PID lateral por
// cross_track (Capa 3, strafe paralelo) y el fallback por depth (comportamiento
// previo). Por eso se fija acá como contrato, antes de la implementación.
#include <unity.h>
#include "types.h"
#include "proto.h"
#include "down_encode.h"
#include "line_view.h"
using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Encodea s con down_encode_line y lo redecodifica byte-a-byte por el
// FrameDecoder (igual que comm_down_tick del CENTRAL drena el UART de DOWN).
static bool encode_then_decode(const LineStatusV2& s, Frame& out) {
    uint8_t buf[64];
    size_t n = down_encode_line(s, 0x07, buf, sizeof(buf));
    if (n == 0) return false;
    FrameDecoder dec;
    bool got = false;
    for (size_t i = 0; i < n; ++i) {
        if (dec.feed(buf[i])) got = true;
    }
    if (got) out = dec.get_frame();
    return got;
}

// Línea válida con cross_track conocido (mm). El resto neutro.
static LineStatusV2 make_line(int16_t cross_track_mm, uint8_t data_valid = 1) {
    LineStatusV2 s{};
    s.schema_version = LSV2_SCHEMA;
    s.data_valid = data_valid;
    s.line_angle_centideg = 0;
    s.escape_angle_centideg = LSV2_NA_I16;
    s.penetration_mm = 20;
    s.cross_track_mm = cross_track_mm;
    s.line_present = 1;
    s.sensors_on_line = 6;
    s.event_flags = 0;
    s.quality = 90;
    s.sample_age_ms = 1;
    s.reserved = 0;
    return s;
}

// El predicado de validez que strategy.cpp usa para elegir cross_track vs depth,
// y que world_model_cross_track_valid() expone. Lo replicamos acá sobre el campo
// crudo para fijar el contrato exacto (no es código del binario — es el contrato).
static bool cross_track_valid(const LineStatusV2& s) {
    return s.data_valid != 0 && s.cross_track_mm != LSV2_NA_I16;
}

// --- Valor: línea ADELANTE del centro => cross_track > 0 (convención down_model).
void test_cross_track_front_positive(void) {
    LineStatusV2 s = make_line(76);
    Frame f{};
    TEST_ASSERT_TRUE(encode_then_decode(s, f));
    LineStatusV2 got{};
    TEST_ASSERT_TRUE(lsv2_from_frame(f, got));
    TEST_ASSERT_TRUE(cross_track_valid(got));
    TEST_ASSERT_EQUAL_FLOAT(76.0f, lsv2_cross_track_mm(got));
}

// --- Valor: línea ATRÁS del centro => cross_track < 0 (signo opuesto).
void test_cross_track_rear_negative(void) {
    LineStatusV2 s = make_line(-69);
    Frame f{};
    TEST_ASSERT_TRUE(encode_then_decode(s, f));
    LineStatusV2 got{};
    TEST_ASSERT_TRUE(lsv2_from_frame(f, got));
    TEST_ASSERT_TRUE(cross_track_valid(got));
    TEST_ASSERT_EQUAL_FLOAT(-69.0f, lsv2_cross_track_mm(got));
}

// --- Robot centrado en la línea: cross_track == 0 PERO sigue siendo VÁLIDO.
// Clave del diseño: 0 mm "centrado" debe distinguirse de N/A (sin señal). Si el
// arquero confundiera ambos, nunca usaría el strafe paralelo cuando está justo
// sobre la línea. El gate es data_valid+sentinela, NO "valor == 0".
void test_cross_track_zero_is_valid_not_na(void) {
    LineStatusV2 s = make_line(0);
    Frame f{};
    TEST_ASSERT_TRUE(encode_then_decode(s, f));
    LineStatusV2 got{};
    TEST_ASSERT_TRUE(lsv2_from_frame(f, got));
    TEST_ASSERT_TRUE(cross_track_valid(got));               // centrado != N/A
    TEST_ASSERT_EQUAL_FLOAT(0.0f, lsv2_cross_track_mm(got));
}

// --- N/A (LSV2_NA_I16): NO válido => strategy cae al fallback por depth.
// lsv2_cross_track_mm devuelve 0.0f como valor seguro, pero el gate dice "no usar".
void test_cross_track_na_is_invalid(void) {
    LineStatusV2 s = make_line(LSV2_NA_I16);
    Frame f{};
    TEST_ASSERT_TRUE(encode_then_decode(s, f));
    LineStatusV2 got{};
    TEST_ASSERT_TRUE(lsv2_from_frame(f, got));
    TEST_ASSERT_FALSE(cross_track_valid(got));              // N/A => fallback
    TEST_ASSERT_EQUAL_FLOAT(0.0f, lsv2_cross_track_mm(got));  // valor seguro
}

// --- Compuerta maestra: data_valid==0 invalida la geometría aunque el campo
// tenga un número. lsv2_cross_track_mm honra data_valid (devuelve 0.0f) y el
// gate también => fallback por depth.
void test_cross_track_data_invalid_gates_everything(void) {
    LineStatusV2 s{};
    s.data_valid = 0;            // compuerta maestra cerrada
    s.cross_track_mm = 123;      // basura potencial
    s.line_present = 1;
    TEST_ASSERT_FALSE(cross_track_valid(s));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, lsv2_cross_track_mm(s));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_cross_track_front_positive);
    RUN_TEST(test_cross_track_rear_negative);
    RUN_TEST(test_cross_track_zero_is_valid_not_na);
    RUN_TEST(test_cross_track_na_is_invalid);
    RUN_TEST(test_cross_track_data_invalid_gates_everything);
    return UNITY_END();
}
