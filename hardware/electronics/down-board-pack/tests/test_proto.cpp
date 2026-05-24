// test_proto — tests unitarios del protocolo UART (proto.h, crc16.h)
// Corre en host con: pio test -e test_native -f test_proto

#include <unity.h>
#include <cstring>
#include "proto.h"
#include "crc16.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// CRC16
// ============================================================================

void test_crc16_known_value_123456789(void) {
    // Vector de prueba estándar para CRC-16/CCITT-FALSE: "123456789" → 0x29B1
    uint8_t data[] = {'1','2','3','4','5','6','7','8','9'};
    uint16_t crc = crc16_ccitt(data, sizeof(data));
    TEST_ASSERT_EQUAL_UINT16(0x29B1, crc);
}

void test_crc16_empty_input_is_init_value(void) {
    uint16_t crc = crc16_ccitt(nullptr, 0);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, crc);
}

void test_crc16_single_byte_differs(void) {
    uint8_t a[] = {0x00};
    uint8_t b[] = {0x01};
    TEST_ASSERT_NOT_EQUAL(crc16_ccitt(a, 1), crc16_ccitt(b, 1));
}

// ============================================================================
// proto_encode / FrameDecoder loopback
// ============================================================================

static Frame make_frame(MsgType type, uint8_t seq, const uint8_t* payload, uint8_t len) {
    Frame f{};
    f.type = type;
    f.seq = seq;
    f.payload_len = len;
    for (uint8_t i = 0; i < len; ++i) f.payload[i] = payload[i];
    return f;
}

void test_encode_then_decode_recovers_frame(void) {
    uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
    Frame in = make_frame(DOWN_LINE_STATUS, 42, payload, 4);

    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(in, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(PROTO_FRAME_OVERHEAD + 4, n);
    TEST_ASSERT_EQUAL_UINT8(PROTO_START, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(PROTO_END, buf[n - 1]);

    FrameDecoder decoder;
    bool decoded = false;
    for (size_t i = 0; i < n; ++i) {
        if (decoder.feed(buf[i])) decoded = true;
    }
    TEST_ASSERT_TRUE(decoded);

    const Frame& out = decoder.get_frame();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(in.type), static_cast<uint8_t>(out.type));
    TEST_ASSERT_EQUAL_UINT8(in.seq, out.seq);
    TEST_ASSERT_EQUAL_size_t(in.payload_len, out.payload_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(in.payload, out.payload, in.payload_len);
    TEST_ASSERT_EQUAL_UINT32(1, decoder.frames_decoded());
    TEST_ASSERT_EQUAL_UINT32(0, decoder.crc_errors());
}

void test_encode_empty_payload_works(void) {
    Frame in = make_frame(MsgType::DEBUG_PING, 7, nullptr, 0);
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(in, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(PROTO_FRAME_OVERHEAD, n);

    FrameDecoder decoder;
    bool decoded = false;
    for (size_t i = 0; i < n; ++i) {
        if (decoder.feed(buf[i])) decoded = true;
    }
    TEST_ASSERT_TRUE(decoded);
    TEST_ASSERT_EQUAL_size_t(0, decoder.get_frame().payload_len);
}

void test_encode_max_payload_works(void) {
    uint8_t payload[PROTO_MAX_PAYLOAD];
    for (size_t i = 0; i < PROTO_MAX_PAYLOAD; ++i) payload[i] = static_cast<uint8_t>(i * 3);
    Frame in = make_frame(MsgType::DOWN_OTOS_POSE, 100, payload, PROTO_MAX_PAYLOAD);

    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(in, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(PROTO_FRAME_OVERHEAD + PROTO_MAX_PAYLOAD, n);
}

void test_encode_buffer_too_small_returns_zero(void) {
    Frame in = make_frame(MsgType::DEBUG_PING, 0, nullptr, 0);
    uint8_t small[3];
    size_t n = proto_encode(in, small, sizeof(small));
    TEST_ASSERT_EQUAL_size_t(0, n);
}

// ============================================================================
// Robustness — resync, corruption, packet loss
// ============================================================================

void test_decoder_skips_garbage_before_valid_frame(void) {
    Frame in = make_frame(MsgType::DEBUG_PING, 1, nullptr, 0);
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(in, buf, sizeof(buf));

    FrameDecoder decoder;
    // Inyectar 5 bytes basura antes del frame válido
    const uint8_t garbage[5] = {0x11, 0x22, 0x33, 0x44, 0x55};
    for (uint8_t b : garbage) decoder.feed(b);

    bool decoded = false;
    for (size_t i = 0; i < n; ++i) {
        if (decoder.feed(buf[i])) decoded = true;
    }
    TEST_ASSERT_TRUE(decoded);
    TEST_ASSERT_EQUAL_UINT32(1, decoder.frames_decoded());
}

void test_decoder_rejects_corrupted_crc(void) {
    uint8_t payload[] = {0xAB, 0xCD};
    Frame in = make_frame(DOWN_LINE_STATUS, 10, payload, 2);

    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(in, buf, sizeof(buf));

    // Corromper el CRC high byte
    buf[n - 3] ^= 0xFF;

    FrameDecoder decoder;
    bool decoded = false;
    for (size_t i = 0; i < n; ++i) {
        if (decoder.feed(buf[i])) decoded = true;
    }
    TEST_ASSERT_FALSE(decoded);
    TEST_ASSERT_EQUAL_UINT32(0, decoder.frames_decoded());
    TEST_ASSERT_EQUAL_UINT32(1, decoder.crc_errors());
}

void test_decoder_rejects_bad_end_byte(void) {
    Frame in = make_frame(MsgType::DEBUG_PING, 0, nullptr, 0);
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(in, buf, sizeof(buf));

    // Corromper el byte END
    buf[n - 1] = 0xFF;

    FrameDecoder decoder;
    bool decoded = false;
    for (size_t i = 0; i < n; ++i) {
        if (decoder.feed(buf[i])) decoded = true;
    }
    TEST_ASSERT_FALSE(decoded);
    TEST_ASSERT_EQUAL_UINT32(1, decoder.resync_events());
}

void test_decoder_handles_back_to_back_frames(void) {
    FrameDecoder decoder;
    int decoded_count = 0;
    uint8_t big_buf[10 * PROTO_MAX_FRAME];
    size_t total = 0;

    for (int seq = 0; seq < 10; ++seq) {
        uint8_t pl[1] = { static_cast<uint8_t>(seq * 17) };
        Frame f = make_frame(MsgType::DEBUG_PING, static_cast<uint8_t>(seq), pl, 1);
        size_t n = proto_encode(f, big_buf + total, sizeof(big_buf) - total);
        TEST_ASSERT_TRUE(n > 0);
        total += n;
    }

    for (size_t i = 0; i < total; ++i) {
        if (decoder.feed(big_buf[i])) decoded_count++;
    }
    TEST_ASSERT_EQUAL_INT(10, decoded_count);
    TEST_ASSERT_EQUAL_UINT32(10, decoder.frames_decoded());
    TEST_ASSERT_EQUAL_UINT32(0, decoder.crc_errors());
}

void test_decoder_recovers_after_corrupted_frame(void) {
    // 1) frame válido → 2) frame corrupto → 3) frame válido
    FrameDecoder decoder;
    int decoded_count = 0;

    for (int round = 0; round < 3; ++round) {
        uint8_t pl[2] = { static_cast<uint8_t>(round), 0xFF };
        Frame f = make_frame(MsgType::DEBUG_PING, static_cast<uint8_t>(round), pl, 2);
        uint8_t buf[PROTO_MAX_FRAME];
        size_t n = proto_encode(f, buf, sizeof(buf));
        if (round == 1) {
            buf[n - 3] ^= 0xFF;  // corrompemos el CRC del frame intermedio
        }
        for (size_t i = 0; i < n; ++i) {
            if (decoder.feed(buf[i])) decoded_count++;
        }
    }
    TEST_ASSERT_EQUAL_INT(2, decoded_count);
    TEST_ASSERT_EQUAL_UINT32(2, decoder.frames_decoded());
    TEST_ASSERT_EQUAL_UINT32(1, decoder.crc_errors());
}

void test_decoder_rejects_oversized_len(void) {
    // Frame con LEN > PROTO_MAX_PAYLOAD → debe resincronizar y descartar
    FrameDecoder decoder;
    uint8_t bad_frame[] = { PROTO_START, 100 /*LEN demasiado grande*/, 0x10, 0x00 };
    bool decoded = false;
    for (uint8_t b : bad_frame) {
        if (decoder.feed(b)) decoded = true;
    }
    TEST_ASSERT_FALSE(decoded);
    TEST_ASSERT_EQUAL_UINT32(1, decoder.resync_events());
}

// ============================================================================
// Main
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();

    // CRC
    RUN_TEST(test_crc16_known_value_123456789);
    RUN_TEST(test_crc16_empty_input_is_init_value);
    RUN_TEST(test_crc16_single_byte_differs);

    // Encode/decode loopback
    RUN_TEST(test_encode_then_decode_recovers_frame);
    RUN_TEST(test_encode_empty_payload_works);
    RUN_TEST(test_encode_max_payload_works);
    RUN_TEST(test_encode_buffer_too_small_returns_zero);

    // Robustness
    RUN_TEST(test_decoder_skips_garbage_before_valid_frame);
    RUN_TEST(test_decoder_rejects_corrupted_crc);
    RUN_TEST(test_decoder_rejects_bad_end_byte);
    RUN_TEST(test_decoder_handles_back_to_back_frames);
    RUN_TEST(test_decoder_recovers_after_corrupted_frame);
    RUN_TEST(test_decoder_rejects_oversized_len);

    return UNITY_END();
}
