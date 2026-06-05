// test_down_encode — corre con: pio test -e test_native -f test_down_encode
#include <unity.h>
#include <cstring>
#include <cstddef>   // offsetof
#include "types.h"
#include "down_encode.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

// Reconstruye un int16/uint16 little-endian desde dos bytes del buffer. DOWN y
// CENTRAL son Teensy (ARM little-endian) y el host de test es x86 (LE), así que
// el orden del wire coincide con el del struct en memoria.
static int16_t le_i16(const uint8_t* p){
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint16_t le_u16(const uint8_t* p){
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

void test_linestatusv2_is_16_bytes(void){
    TEST_ASSERT_EQUAL_UINT32(16, sizeof(LineStatusV2));
}
void test_linestatusv2_constants(void){
    TEST_ASSERT_EQUAL_UINT8(2, LSV2_SCHEMA);
    TEST_ASSERT_EQUAL_INT16(-32768, LSV2_NA_I16);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, LSV2_NA_U16);
    TEST_ASSERT_EQUAL_UINT8(0x01, EV_IMMINENT_EXIT);
    TEST_ASSERT_EQUAL_UINT8(0x02, EV_CORNER);
    TEST_ASSERT_EQUAL_UINT8(0x04, EV_LINE_END);
    TEST_ASSERT_EQUAL_UINT8(0x08, EV_LIFTED);
    TEST_ASSERT_EQUAL_UINT8(0x10, EV_CALIB_SUSPECT);
    TEST_ASSERT_EQUAL_UINT8(0x20, EV_MUX_DEAD);
    TEST_ASSERT_EQUAL_UINT8(0x40, EV_DEGRADED_GEOMETRY);
}
// Ejemplo B del contrato (docs/firmware/CONTRATO-DATOS-DOWN.md §3.6), SEQ=0x01.
void test_encode_matches_contract_example_B(void){
    LineStatusV2 s{};
    s.schema_version=2; s.data_valid=1;
    s.line_angle_centideg=4500; s.escape_angle_centideg=-13500;
    s.penetration_mm=15; s.cross_track_mm=-8;
    s.line_present=1; s.sensors_on_line=4; s.event_flags=0;
    s.quality=88; s.sample_age_ms=1; s.reserved=0;
    uint8_t out[64];
    size_t n = down_encode_line(s, 0x01, out, sizeof(out));
    const uint8_t exp[] = {
      0xAA,0x10,0x10,0x01, 0x02,0x01,0x94,0x11,0x44,0xCB,0x0F,0x00,
      0xF8,0xFF,0x01,0x04,0x00,0x58,0x01,0x00, 0xDF,0xBF,0x55 };
    TEST_ASSERT_EQUAL_UINT32(sizeof(exp), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(exp, out, sizeof(exp));
}

// ── GOLDEN de OFFSETS del payload LineStatusV2 (16 B) ────────────────────────
// El test contract_example_B compara el frame ENTERO contra un array dorado: SÍ
// dispara ante un reorder (los bytes cambian), pero usa un único set de valores
// del contrato y NO documenta/pinea QUÉ campo vive en QUÉ offset. Un swap de dos
// campos del MISMO tamaño (p.ej. line_angle↔escape_angle, ambos int16; o
// penetration↔cross_track) deja sizeof==16 intacto y solo se delata si los
// valores difieren. Estos tests fijan el offset de CADA campo con offsetof y
// además leen el byte/word crudo en ese offset DENTRO DEL FRAME SERIALIZADO
// (payload arranca en el byte 4: [0xAA,LEN,TYPE,SEQ, payload..]), sembrando un
// valor ÚNICO por campo → cualquier reorder/desfase del payload dispara acá.

// Offsets esperados del struct packed (contrato types.h / CONTRATO-DATOS-DOWN.md).
void test_linestatusv2_struct_offsets_golden(void){
    TEST_ASSERT_EQUAL_UINT32(0,  offsetof(LineStatusV2, schema_version));
    TEST_ASSERT_EQUAL_UINT32(1,  offsetof(LineStatusV2, data_valid));
    TEST_ASSERT_EQUAL_UINT32(2,  offsetof(LineStatusV2, line_angle_centideg));
    TEST_ASSERT_EQUAL_UINT32(4,  offsetof(LineStatusV2, escape_angle_centideg));
    TEST_ASSERT_EQUAL_UINT32(6,  offsetof(LineStatusV2, penetration_mm));
    TEST_ASSERT_EQUAL_UINT32(8,  offsetof(LineStatusV2, cross_track_mm));
    TEST_ASSERT_EQUAL_UINT32(10, offsetof(LineStatusV2, line_present));
    TEST_ASSERT_EQUAL_UINT32(11, offsetof(LineStatusV2, sensors_on_line));
    TEST_ASSERT_EQUAL_UINT32(12, offsetof(LineStatusV2, event_flags));
    TEST_ASSERT_EQUAL_UINT32(13, offsetof(LineStatusV2, quality));
    TEST_ASSERT_EQUAL_UINT32(14, offsetof(LineStatusV2, sample_age_ms));
    TEST_ASSERT_EQUAL_UINT32(15, offsetof(LineStatusV2, reserved));
}

// Siembra TODOS los campos con valores únicos, serializa y pinea el byte/word de
// CADA campo en su offset dentro del frame (payload comienza en frame[4]).
void test_linestatusv2_payload_offsets_in_frame_golden(void){
    LineStatusV2 s{};
    s.schema_version       = 2;        // payload[0]
    s.data_valid           = 1;        // payload[1]
    s.line_angle_centideg  = 0x1122;   // payload[2..3]  (4386)
    s.escape_angle_centideg= 0x3344;   // payload[4..5]  (13124)
    s.penetration_mm       = 0x5566;   // payload[6..7]  (21862)
    s.cross_track_mm       = 0x0708;   // payload[8..9]  (1800, >0 para no chocar signo)
    s.line_present         = 0x09;     // payload[10]
    s.sensors_on_line      = 0x0A;     // payload[11]
    s.event_flags          = 0x0B;     // payload[12]
    s.quality              = 0x0C;     // payload[13]
    s.sample_age_ms        = 0x0D;     // payload[14]
    s.reserved             = 0x0E;     // payload[15]

    uint8_t out[64];
    size_t n = down_encode_line(s, 0x2A, out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT32(23, n);   // 7 overhead + 16 payload

    // Encabezado del frame proto.h: [0xAA, LEN=16, TYPE=0x10, SEQ=0x2A].
    TEST_ASSERT_EQUAL_UINT8(0xAA, out[0]);
    TEST_ASSERT_EQUAL_UINT8(16,   out[1]);   // LEN = sizeof(payload)
    TEST_ASSERT_EQUAL_UINT8(0x10, out[2]);   // TYPE = LINE_URGENT
    TEST_ASSERT_EQUAL_UINT8(0x2A, out[3]);   // SEQ
    TEST_ASSERT_EQUAL_UINT8(0x55, out[22]);  // END

    // Payload arranca en out+4. Cada campo en SU offset (little-endian).
    const uint8_t* pl = out + 4;
    TEST_ASSERT_EQUAL_UINT8 (2,        pl[0]);            // schema_version
    TEST_ASSERT_EQUAL_UINT8 (1,        pl[1]);            // data_valid
    TEST_ASSERT_EQUAL_INT16 (0x1122,   le_i16(pl + 2));   // line_angle_centideg
    TEST_ASSERT_EQUAL_INT16 (0x3344,   le_i16(pl + 4));   // escape_angle_centideg
    TEST_ASSERT_EQUAL_UINT16(0x5566,   le_u16(pl + 6));   // penetration_mm
    TEST_ASSERT_EQUAL_INT16 (0x0708,   le_i16(pl + 8));   // cross_track_mm
    TEST_ASSERT_EQUAL_UINT8 (0x09,     pl[10]);           // line_present
    TEST_ASSERT_EQUAL_UINT8 (0x0A,     pl[11]);           // sensors_on_line
    TEST_ASSERT_EQUAL_UINT8 (0x0B,     pl[12]);           // event_flags
    TEST_ASSERT_EQUAL_UINT8 (0x0C,     pl[13]);           // quality
    TEST_ASSERT_EQUAL_UINT8 (0x0D,     pl[14]);           // sample_age_ms
    TEST_ASSERT_EQUAL_UINT8 (0x0E,     pl[15]);           // reserved
}

// Cubre el signo de los campos i16: un negativo en line_angle/escape/cross_track
// debe serializar en complemento a dos little-endian en SU offset. Esto delata
// un campo i16 que por error se tratara como u16 o se leyera del offset vecino.
void test_linestatusv2_payload_offsets_signed_negatives(void){
    LineStatusV2 s{};
    s.schema_version        = 2;
    s.data_valid            = 1;
    s.line_angle_centideg   = -1;      // 0xFFFF LE → FF FF
    s.escape_angle_centideg = -2;      // 0xFFFE LE → FE FF
    s.cross_track_mm        = -3;      // 0xFFFD LE → FD FF
    s.penetration_mm        = 0xABCD;  // u16 distinto, para no confundirse

    uint8_t out[64];
    size_t n = down_encode_line(s, 0x00, out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT32(23, n);
    const uint8_t* pl = out + 4;
    TEST_ASSERT_EQUAL_INT16 (-1,      le_i16(pl + 2));   // line_angle
    TEST_ASSERT_EQUAL_INT16 (-2,      le_i16(pl + 4));   // escape_angle
    TEST_ASSERT_EQUAL_UINT16(0xABCD,  le_u16(pl + 6));   // penetration (u16, sin signo)
    TEST_ASSERT_EQUAL_INT16 (-3,      le_i16(pl + 8));   // cross_track
}

// === lsv2_sample_age_ms (audit 2026-06-03 #2) ===
// Antes el campo sample_age_ms viajaba pegado en 255 porque comm_central restaba
// la DURACIÓN del tick (no el timestamp). El helper puro hace resta unsigned +
// clamp; comm_central lo llama con (micros(), line_ring_get_last_sample_us()).
void test_sample_age_zero_when_now_equals_sample(void){
    TEST_ASSERT_EQUAL_UINT8(0, lsv2_sample_age_ms(123456u, 123456u));
}
void test_sample_age_5ms(void){
    TEST_ASSERT_EQUAL_UINT8(5, lsv2_sample_age_ms(5000u + 200u, 200u));
}
void test_sample_age_truncates_to_ms(void){
    // 254999 us -> 254 ms (división entera, sin redondeo). No satura.
    TEST_ASSERT_EQUAL_UINT8(254, lsv2_sample_age_ms(254999u, 0u));
}
void test_sample_age_clamps_at_255(void){
    TEST_ASSERT_EQUAL_UINT8(255, lsv2_sample_age_ms(300000u, 0u));     // 300 ms
    TEST_ASSERT_EQUAL_UINT8(255, lsv2_sample_age_ms(0xFFFFFFFFu, 0u)); // enorme
}
void test_sample_age_handles_micros_wrap(void){
    // sample_us cerca del overflow de micros(), now_us ya envolvió: la resta
    // unsigned da el delta REAL (pocos ms), NO 255. Antes del fix esto rompía.
    const uint32_t sample = 0xFFFFFF00u;  // ~4.29e9, a 256 us del wrap
    const uint32_t now    = sample + 3000u;  // +3 ms (envuelve a ~2756)
    TEST_ASSERT_EQUAL_UINT8(3, lsv2_sample_age_ms(now, sample));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_linestatusv2_is_16_bytes);
    RUN_TEST(test_linestatusv2_constants);
    RUN_TEST(test_encode_matches_contract_example_B);
    RUN_TEST(test_linestatusv2_struct_offsets_golden);
    RUN_TEST(test_linestatusv2_payload_offsets_in_frame_golden);
    RUN_TEST(test_linestatusv2_payload_offsets_signed_negatives);
    RUN_TEST(test_sample_age_zero_when_now_equals_sample);
    RUN_TEST(test_sample_age_5ms);
    RUN_TEST(test_sample_age_truncates_to_ms);
    RUN_TEST(test_sample_age_clamps_at_255);
    RUN_TEST(test_sample_age_handles_micros_wrap);
    return UNITY_END();
}
