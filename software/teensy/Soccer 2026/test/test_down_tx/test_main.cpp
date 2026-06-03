// test_down_tx — corre con: pio test -e test_native -f test_down_tx
//                (o bash scripts/run-host-tests.sh test_down_tx)
//
// Pinnea el CAMINO VIVO de TX de la placa DOWN al contrato (audit 2026-06-03 #11).
//
// Contexto: down_tx.cpp (que es el que corre en producción) NO se puede linkear
// host-side porque incluye <Arduino.h> (HardwareSerial). PERO desde el fix #11,
// down_tx_broadcast_line() construye el frame de LÍNEA llamando a
// down_encode_line() — un encoder PURO en src/shared, host-testeable. Así el
// frame que REALMENTE sale a CENTRAL/TOP queda atado al golden del contrato:
// si alguien cambia el layout de LineStatusV2 o el armado del frame, un test
// rojo lo grita (antes el golden sólo protegía un encoder que producción no
// ejecutaba).
//
// Este suite verifica:
//   1) down_encode_line (= la rama LINE de down_tx) == golden de 23 bytes.
//   2) El frame round-trip-ea por el FrameDecoder con type/seq/payload correctos.
//   3) SEQ se respeta tal cual lo pasa down_tx (lk.seq++ por enlace).
//   4) Robustez frente a valores de borde (N/A, data_valid=0, penetration grande).

#include <unity.h>
#include <cstring>
#include "types.h"
#include "proto.h"
#include "down_encode.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Helper: construye un LineStatusV2 "normal" (línea presente al frente).
static LineStatusV2 make_line(void) {
    LineStatusV2 s{};
    s.schema_version = LSV2_SCHEMA;
    s.data_valid = 1;
    s.line_angle_centideg = 4500;
    s.escape_angle_centideg = -13500;
    s.penetration_mm = 15;
    s.cross_track_mm = -8;
    s.line_present = 1;
    s.sensors_on_line = 4;
    s.event_flags = 0;
    s.quality = 88;
    s.sample_age_ms = 1;
    s.reserved = 0;
    return s;
}

// 1) El frame de LÍNEA que emite down_tx (vía down_encode_line) == golden contrato.
//    Mismo Ejemplo B que test_down_encode, pero acá pinea explícitamente el
//    camino que corre en el robot (down_tx_broadcast_line -> down_encode_line).
void test_live_line_frame_matches_contract_golden(void) {
    LineStatusV2 s = make_line();
    uint8_t out[PROTO_MAX_FRAME];
    size_t n = down_encode_line(s, 0x01, out, sizeof(out));
    const uint8_t exp[] = {
      0xAA,0x10,0x10,0x01, 0x02,0x01,0x94,0x11,0x44,0xCB,0x0F,0x00,
      0xF8,0xFF,0x01,0x04,0x00,0x58,0x01,0x00, 0xDF,0xBF,0x55 };
    TEST_ASSERT_EQUAL_UINT32(sizeof(exp), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(exp, out, sizeof(exp));
}

// 2) El frame vivo decodifica byte-idéntico (type=LINE_URGENT, payload=16B).
void test_live_line_frame_roundtrips(void) {
    LineStatusV2 s = make_line();
    uint8_t out[PROTO_MAX_FRAME];
    size_t n = down_encode_line(s, 0x2A, out, sizeof(out));
    TEST_ASSERT_TRUE(n > 0);

    FrameDecoder dec;
    bool ok = false;
    for (size_t i = 0; i < n; ++i) if (dec.feed(out[i])) ok = true;
    TEST_ASSERT_TRUE(ok);

    const Frame& f = dec.get_frame();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MsgType::LINE_URGENT),
                            static_cast<uint8_t>(f.type));
    TEST_ASSERT_EQUAL_UINT8(0x2A, f.seq);
    TEST_ASSERT_EQUAL_size_t(sizeof(LineStatusV2), f.payload_len);

    LineStatusV2 back{};
    memcpy(&back, f.payload, sizeof(back));
    TEST_ASSERT_EQUAL_INT16(s.line_angle_centideg, back.line_angle_centideg);
    TEST_ASSERT_EQUAL_INT16(s.cross_track_mm, back.cross_track_mm);
    TEST_ASSERT_EQUAL_UINT8(s.line_present, back.line_present);
    TEST_ASSERT_EQUAL_UINT8(s.sample_age_ms, back.sample_age_ms);
}

// 3) El SEQ que down_tx pasa (lk.seq++) viaja literal en el frame.
void test_live_line_frame_carries_seq(void) {
    LineStatusV2 s = make_line();
    for (uint8_t seq = 0; seq < 5; ++seq) {
        uint8_t out[PROTO_MAX_FRAME];
        size_t n = down_encode_line(s, seq, out, sizeof(out));
        TEST_ASSERT_TRUE(n > 0);
        // Layout: [START, LEN, TYPE, SEQ, ...]
        TEST_ASSERT_EQUAL_UINT8(seq, out[3]);
    }
}

// 4) Valores de borde: data_valid=0 + N/A en ángulos + penetration grande
//    round-trip-ean sin corrupción (defienden el camino vivo de regresiones).
void test_live_line_frame_edge_values(void) {
    LineStatusV2 s{};
    s.schema_version = LSV2_SCHEMA;
    s.data_valid = 0;
    s.line_angle_centideg = LSV2_NA_I16;
    s.escape_angle_centideg = LSV2_NA_I16;
    s.penetration_mm = LSV2_NA_U16;
    s.cross_track_mm = LSV2_NA_I16;
    s.line_present = 0;
    s.sensors_on_line = 0;
    s.event_flags = EV_CALIB_SUSPECT | EV_MUX_DEAD;
    s.quality = 0;
    s.sample_age_ms = 255;
    s.reserved = 0;

    uint8_t out[PROTO_MAX_FRAME];
    size_t n = down_encode_line(s, 0x77, out, sizeof(out));
    TEST_ASSERT_TRUE(n > 0);

    FrameDecoder dec;
    bool ok = false;
    for (size_t i = 0; i < n; ++i) if (dec.feed(out[i])) ok = true;
    TEST_ASSERT_TRUE(ok);

    LineStatusV2 back{};
    memcpy(&back, dec.get_frame().payload, sizeof(back));
    TEST_ASSERT_EQUAL_UINT8(0, back.data_valid);
    TEST_ASSERT_EQUAL_INT16(LSV2_NA_I16, back.line_angle_centideg);
    TEST_ASSERT_EQUAL_UINT16(LSV2_NA_U16, back.penetration_mm);
    TEST_ASSERT_EQUAL_INT16(LSV2_NA_I16, back.cross_track_mm);
    TEST_ASSERT_EQUAL_UINT8(EV_CALIB_SUSPECT | EV_MUX_DEAD, back.event_flags);
    TEST_ASSERT_EQUAL_UINT8(255, back.sample_age_ms);
    TEST_ASSERT_EQUAL_UINT32(0, dec.crc_errors());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_live_line_frame_matches_contract_golden);
    RUN_TEST(test_live_line_frame_roundtrips);
    RUN_TEST(test_live_line_frame_carries_seq);
    RUN_TEST(test_live_line_frame_edge_values);
    return UNITY_END();
}
