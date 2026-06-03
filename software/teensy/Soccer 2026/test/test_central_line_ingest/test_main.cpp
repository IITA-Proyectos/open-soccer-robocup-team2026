// test_central_line_ingest — corre con: pio test -e test_native -f test_central_line_ingest
//
// Cubre el camino que estaba ROTO antes de 2026-05-29: DOWN encodea LineStatusV2
// (16 bytes) y CENTRAL exigía LineStatus viejo (5 bytes) → todo frame de línea
// se descartaba. Acá probamos el chain completo encode→decode→interpret con los
// helpers de line_view.h (los mismos que usa world_model.cpp).
#include <unity.h>
#include "types.h"
#include "proto.h"
#include "down_encode.h"
#include "line_view.h"
using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Encodea s con down_encode_line y lo vuelve a decodificar byte-a-byte por la
// state machine del FrameDecoder (igual que comm_down_tick drena Serial2).
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

// Ejemplo B del contrato (mismo que test_down_encode): data_valid, sin eventos.
static LineStatusV2 make_example_B(void) {
    LineStatusV2 s{};
    s.schema_version = 2; s.data_valid = 1;
    s.line_angle_centideg = 4500; s.escape_angle_centideg = -13500;
    s.penetration_mm = 15; s.cross_track_mm = -8;
    s.line_present = 1; s.sensors_on_line = 4; s.event_flags = 0;
    s.quality = 88; s.sample_age_ms = 1; s.reserved = 0;
    return s;
}

// --- El chain completo: lo que DOWN manda, CENTRAL lo decodifica y lo lee bien.
void test_roundtrip_example_B(void) {
    LineStatusV2 s = make_example_B();
    Frame f{};
    TEST_ASSERT_TRUE(encode_then_decode(s, f));

    LineStatusV2 got{};
    TEST_ASSERT_TRUE(lsv2_from_frame(f, got));
    TEST_ASSERT_EQUAL_UINT8(2, got.schema_version);
    TEST_ASSERT_EQUAL_UINT8(1, got.data_valid);
    TEST_ASSERT_EQUAL_INT16(4500, got.line_angle_centideg);
    TEST_ASSERT_EQUAL_UINT16(15, got.penetration_mm);
    TEST_ASSERT_EQUAL_UINT8(4, got.sensors_on_line);

    // Interpretación (lo que consulta strategy/main vía world_model):
    TEST_ASSERT_TRUE(lsv2_line_present(got));
    TEST_ASSERT_FALSE(lsv2_imminent_exit(got));   // event_flags == 0
    TEST_ASSERT_FALSE(lsv2_lifted(got));
    TEST_ASSERT_EQUAL_FLOAT(45.0f, lsv2_line_angle_deg(got));
    TEST_ASSERT_EQUAL_UINT8(15, lsv2_penetration_u8(got));
    TEST_ASSERT_EQUAL_UINT8(4, lsv2_sensors_on_line(got));
}

// --- EV_IMMINENT_EXIT dispara imminent_exit... salvo que esté lifted.
void test_imminent_exit_set(void) {
    LineStatusV2 s = make_example_B();
    s.event_flags = EV_IMMINENT_EXIT;
    Frame f{};
    TEST_ASSERT_TRUE(encode_then_decode(s, f));
    LineStatusV2 got{};
    TEST_ASSERT_TRUE(lsv2_from_frame(f, got));
    TEST_ASSERT_TRUE(lsv2_imminent_exit(got));
}

// EL fix de contrato: lifted ⇒ imminent_exit false (strategy.cpp:17).
// Probado a través del chain real encode→decode, no sólo el helper.
void test_imminent_exit_gated_by_lifted(void) {
    LineStatusV2 s = make_example_B();
    s.event_flags = EV_IMMINENT_EXIT | EV_LIFTED;
    Frame f{};
    TEST_ASSERT_TRUE(encode_then_decode(s, f));
    LineStatusV2 got{};
    TEST_ASSERT_TRUE(lsv2_from_frame(f, got));
    TEST_ASSERT_TRUE(lsv2_lifted(got));
    TEST_ASSERT_FALSE(lsv2_imminent_exit(got));
}

// data_valid == 0 es la compuerta maestra: nada se confía.
void test_data_valid_master_gate(void) {
    LineStatusV2 s{};
    s.data_valid = 0;
    s.line_present = 1;
    s.event_flags = EV_IMMINENT_EXIT;
    TEST_ASSERT_FALSE(lsv2_line_present(s));
    TEST_ASSERT_FALSE(lsv2_imminent_exit(s));
}

// Ángulo N/A ⇒ 0 grados (no basura).
void test_line_angle_na(void) {
    LineStatusV2 s = make_example_B();
    s.line_angle_centideg = LSV2_NA_I16;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, lsv2_line_angle_deg(s));
}

// Penetración: clamp a 255, N/A ⇒ 0, valor normal pasa.
void test_penetration_clamp_and_na(void) {
    LineStatusV2 s{};
    s.penetration_mm = 500;            TEST_ASSERT_EQUAL_UINT8(255, lsv2_penetration_u8(s));
    s.penetration_mm = LSV2_NA_U16;    TEST_ASSERT_EQUAL_UINT8(0,   lsv2_penetration_u8(s));
    s.penetration_mm = 200;            TEST_ASSERT_EQUAL_UINT8(200, lsv2_penetration_u8(s));
}

// Guard de regresión del P0: un payload del tamaño VIEJO (LineStatus, 5 bytes)
// debe ser RECHAZADO. Si alguien revierte el size check, este test falla.
void test_old_size_payload_rejected(void) {
    Frame f{};
    f.type = MsgType::LINE_URGENT;
    f.payload_len = sizeof(LineStatus);   // 5 — el bug original aceptaba esto
    LineStatusV2 out{};
    TEST_ASSERT_FALSE(lsv2_from_frame(f, out));
}

void test_wrong_type_rejected(void) {
    Frame f{};
    f.type = MsgType::WORLD_SNAPSHOT;
    f.payload_len = sizeof(LineStatusV2);
    LineStatusV2 out{};
    TEST_ASSERT_FALSE(lsv2_from_frame(f, out));
}

// Gate de schema (P1, analisis 2026-05-31): un frame de 16 bytes pero con
// schema_version != LSV2_SCHEMA se RECHAZA (no se reinterpreta como basura).
// Cubre el escenario DOWN/CENTRAL flasheados con schemas distintos.
void test_wrong_schema_rejected(void) {
    LineStatusV2 s = make_example_B();
    s.schema_version = 99;            // schema incompatible, MISMO tamano (16 B)
    Frame f{};
    TEST_ASSERT_TRUE(encode_then_decode(s, f));   // frame valido (CRC OK, 16 B)
    LineStatusV2 out{};
    TEST_ASSERT_FALSE(lsv2_from_frame(f, out));   // pero rechazado por schema
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip_example_B);
    RUN_TEST(test_imminent_exit_set);
    RUN_TEST(test_imminent_exit_gated_by_lifted);
    RUN_TEST(test_data_valid_master_gate);
    RUN_TEST(test_line_angle_na);
    RUN_TEST(test_penetration_clamp_and_na);
    RUN_TEST(test_old_size_payload_rejected);
    RUN_TEST(test_wrong_type_rejected);
    RUN_TEST(test_wrong_schema_rejected);
    return UNITY_END();
}
