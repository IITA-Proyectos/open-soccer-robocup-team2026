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
// TASK-031 — blindaje de robustez del enlace inter-placa (TESTS HOST)
//
// Nota sobre el modelo de framing (documentado por la eval): el protocolo
// NO usa byte-stuffing. El framing NO depende de escapar bytes de payload que
// coincidan con START(0xAA)/END(0x55): depende de
//   (a) leer EXACTAMENTE LEN bytes de payload (la state machine cuenta), y
//   (b) validar CRC-16 + el byte END al cierre.
// Por eso un byte de PAYLOAD == 0xAA o == 0x55 se consume como payload y NO
// rompe el framing. La resincronización tras basura/corrupción es por
// RE-ESCANEO de START + CRC, no por stuffing. Los tests de abajo pinean ese
// contrato exacto para que la validación de banco arranque con red.
// ============================================================================

// --- Colisión de bytes de PAYLOAD con los marcadores START/END ---

// Un payload lleno de 0xAA (START) NO debe romper el framing: se consume como
// payload por el conteo de LEN, no se reinterpreta como inicio de frame.
void test_payload_full_of_start_bytes_decodes(void) {
    uint8_t payload[8];
    for (auto& b : payload) b = PROTO_START;  // 0xAA × 8
    Frame in = make_frame(DOWN_LINE_STATUS, 3, payload, 8);

    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(in, buf, sizeof(buf));

    FrameDecoder decoder;
    bool decoded = false;
    for (size_t i = 0; i < n; ++i) if (decoder.feed(buf[i])) decoded = true;

    TEST_ASSERT_TRUE(decoded);
    const Frame& out = decoder.get_frame();
    TEST_ASSERT_EQUAL_size_t(8, out.payload_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out.payload, 8);
    TEST_ASSERT_EQUAL_UINT32(1, decoder.frames_decoded());
    TEST_ASSERT_EQUAL_UINT32(0, decoder.crc_errors());
}

// Un payload lleno de 0x55 (END) tampoco rompe el framing: el END real sólo se
// chequea en estado READ_END, después de consumir LEN bytes de payload.
void test_payload_full_of_end_bytes_decodes(void) {
    uint8_t payload[8];
    for (auto& b : payload) b = PROTO_END;  // 0x55 × 8
    Frame in = make_frame(DOWN_LINE_STATUS, 4, payload, 8);

    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(in, buf, sizeof(buf));

    FrameDecoder decoder;
    bool decoded = false;
    for (size_t i = 0; i < n; ++i) if (decoder.feed(buf[i])) decoded = true;

    TEST_ASSERT_TRUE(decoded);
    const Frame& out = decoder.get_frame();
    TEST_ASSERT_EQUAL_size_t(8, out.payload_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out.payload, 8);
    TEST_ASSERT_EQUAL_UINT32(0, decoder.crc_errors());
}

// Mezcla adversaria: payload alterna START/END, y además LEN, TYPE y SEQ
// coinciden con marcadores. Nada de esto debe confundir a la state machine.
void test_markers_in_len_type_seq_and_payload_decodes(void) {
    // LEN=0x55(=85) es > PROTO_MAX_PAYLOAD(32) ⇒ inválido como LEN; usamos un
    // payload con marcadores pero LEN legal. Para cubrir TYPE/SEQ == marcador
    // forzamos type=0xAA (no es un MsgType nombrado, pero el decoder lo pasa
    // crudo) y seq=0x55.
    uint8_t payload[6] = { PROTO_START, PROTO_END, PROTO_START, PROTO_END,
                           PROTO_START, PROTO_END };
    Frame in{};
    in.type = static_cast<MsgType>(PROTO_START);  // TYPE = 0xAA
    in.seq  = PROTO_END;                           // SEQ  = 0x55
    in.payload_len = 6;
    memcpy(in.payload, payload, 6);

    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(in, buf, sizeof(buf));

    FrameDecoder decoder;
    bool decoded = false;
    for (size_t i = 0; i < n; ++i) if (decoder.feed(buf[i])) decoded = true;

    TEST_ASSERT_TRUE(decoded);
    const Frame& out = decoder.get_frame();
    TEST_ASSERT_EQUAL_UINT8(PROTO_START, static_cast<uint8_t>(out.type));
    TEST_ASSERT_EQUAL_UINT8(PROTO_END, out.seq);
    TEST_ASSERT_EQUAL_size_t(6, out.payload_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out.payload, 6);
}

// Caso crítico de la eval: un frame cuyo CRC contiene un byte 0x55 (END) en
// medio NO debe cerrarse antes de tiempo. Construimos un payload elegido para
// que el CRC tenga 0x55 y verificamos que igual decodifica entero.
void test_crc_byte_equal_end_marker_does_not_truncate(void) {
    // Buscamos un payload de 1 byte cuyo CRC contenga 0x55 en algún octeto.
    bool found = false;
    for (int v = 0; v < 256 && !found; ++v) {
        uint8_t pl[1] = { static_cast<uint8_t>(v) };
        Frame in = make_frame(DOWN_LINE_STATUS, 0, pl, 1);
        uint8_t buf[PROTO_MAX_FRAME];
        size_t n = proto_encode(in, buf, sizeof(buf));
        // CRC ocupa buf[n-3] (high) y buf[n-2] (low).
        if (buf[n - 3] == PROTO_END || buf[n - 2] == PROTO_END) {
            found = true;
            FrameDecoder decoder;
            bool decoded = false;
            for (size_t i = 0; i < n; ++i) if (decoder.feed(buf[i])) decoded = true;
            TEST_ASSERT_TRUE(decoded);
            TEST_ASSERT_EQUAL_UINT32(1, decoder.frames_decoded());
            TEST_ASSERT_EQUAL_UINT32(0, decoder.crc_errors());
            TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(v),
                                    decoder.get_frame().payload[0]);
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "no se halló payload con 0x55 en el CRC");
}

// --- LEN al límite: 0, MAX (32), y > MAX rechazado ---

void test_len_zero_roundtrips(void) {
    Frame in = make_frame(MsgType::DEBUG_PING, 9, nullptr, 0);
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(in, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(PROTO_FRAME_OVERHEAD, n);

    FrameDecoder decoder;
    bool decoded = false;
    for (size_t i = 0; i < n; ++i) if (decoder.feed(buf[i])) decoded = true;
    TEST_ASSERT_TRUE(decoded);
    TEST_ASSERT_EQUAL_size_t(0, decoder.get_frame().payload_len);
}

void test_len_max_payload_roundtrips(void) {
    uint8_t payload[PROTO_MAX_PAYLOAD];
    for (size_t i = 0; i < PROTO_MAX_PAYLOAD; ++i)
        payload[i] = static_cast<uint8_t>(0xC0 ^ i);
    Frame in = make_frame(MsgType::DOWN_OTOS_POSE, 200, payload, PROTO_MAX_PAYLOAD);

    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(in, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(PROTO_MAX_FRAME, n);

    FrameDecoder decoder;
    bool decoded = false;
    for (size_t i = 0; i < n; ++i) if (decoder.feed(buf[i])) decoded = true;
    TEST_ASSERT_TRUE(decoded);
    const Frame& out = decoder.get_frame();
    TEST_ASSERT_EQUAL_size_t(PROTO_MAX_PAYLOAD, out.payload_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out.payload, PROTO_MAX_PAYLOAD);
}

// LEN == MAX+1 en el wire: el decoder debe rechazarlo en READ_LEN y resync.
void test_len_just_over_max_is_rejected_on_wire(void) {
    FrameDecoder decoder;
    uint8_t bad[] = { PROTO_START,
                      static_cast<uint8_t>(PROTO_MAX_PAYLOAD + 1) /*=33*/,
                      0x10, 0x00 };
    bool decoded = false;
    for (uint8_t b : bad) if (decoder.feed(b)) decoded = true;
    TEST_ASSERT_FALSE(decoded);
    TEST_ASSERT_EQUAL_UINT32(1, decoder.resync_events());
    TEST_ASSERT_EQUAL_UINT32(0, decoder.frames_decoded());
}

// proto_encode debe rechazar payload_len > MAX devolviendo 0 (sin escribir).
void test_encode_rejects_payload_len_over_max(void) {
    Frame in{};
    in.type = DOWN_LINE_STATUS;
    in.seq = 0;
    in.payload_len = PROTO_MAX_PAYLOAD + 1;  // 33
    uint8_t buf[PROTO_MAX_FRAME + 8];
    size_t n = proto_encode(in, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(0, n);
}

// --- SEQ wrap 255 → 0 ---

// El SEQ es un byte opaco que el decoder devuelve literal; 255 y 0 viajan tal
// cual. Pinea que el wrap no se trunca ni se reinterpreta.
void test_seq_wraps_255_to_0(void) {
    uint8_t seqs[] = { 254, 255, 0, 1 };
    for (uint8_t seq : seqs) {
        uint8_t pl[1] = { 0xEE };
        Frame in = make_frame(MsgType::DEBUG_PING, seq, pl, 1);
        uint8_t buf[PROTO_MAX_FRAME];
        size_t n = proto_encode(in, buf, sizeof(buf));
        TEST_ASSERT_EQUAL_UINT8(seq, buf[3]);  // SEQ en el wire

        FrameDecoder decoder;
        bool decoded = false;
        for (size_t i = 0; i < n; ++i) if (decoder.feed(buf[i])) decoded = true;
        TEST_ASSERT_TRUE(decoded);
        TEST_ASSERT_EQUAL_UINT8(seq, decoder.get_frame().seq);
    }
}

// Secuencia 253,254,255,0,1 back-to-back: todos decodifican con su SEQ y el
// receptor ve el wrap sin huecos espurios.
void test_seq_wrap_back_to_back_sequence(void) {
    FrameDecoder decoder;
    uint8_t big[6 * PROTO_MAX_FRAME];
    size_t total = 0;
    const uint8_t seqs[] = { 253, 254, 255, 0, 1 };
    for (uint8_t seq : seqs) {
        uint8_t pl[1] = { seq };
        Frame f = make_frame(MsgType::DEBUG_PING, seq, pl, 1);
        total += proto_encode(f, big + total, sizeof(big) - total);
    }
    uint8_t got[5];
    int idx = 0;
    for (size_t i = 0; i < total; ++i) {
        if (decoder.feed(big[i])) got[idx++] = decoder.get_frame().seq;
    }
    TEST_ASSERT_EQUAL_INT(5, idx);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(seqs, got, 5);
}

// --- CRC corrupto en distintas posiciones ---

// Corromper UN bit en cada posición (LEN, TYPE, SEQ, cualquier payload, CRC
// high, CRC low) debe ser detectado por el CRC y NO emitir frame.
void test_single_bit_flip_anywhere_is_caught(void) {
    uint8_t payload[4] = { 0x11, 0x22, 0x33, 0x44 };
    Frame in = make_frame(DOWN_LINE_STATUS, 0x5A, payload, 4);
    uint8_t base[PROTO_MAX_FRAME];
    size_t n = proto_encode(in, base, sizeof(base));

    // Posiciones 1..n-2 cubren LEN, TYPE, SEQ, PAYLOAD, CRC(hi), CRC(lo).
    // (no tocamos out[0]=START ni out[n-1]=END; esos se prueban aparte).
    for (size_t pos = 1; pos <= n - 2; ++pos) {
        for (int bit = 0; bit < 8; ++bit) {
            uint8_t buf[PROTO_MAX_FRAME];
            memcpy(buf, base, n);
            buf[pos] ^= static_cast<uint8_t>(1u << bit);

            FrameDecoder decoder;
            bool decoded = false;
            for (size_t i = 0; i < n; ++i) if (decoder.feed(buf[i])) decoded = true;

            // Cualquier flip de 1 bit en estos campos debe NO producir frame
            // válido (lo atrapa el CRC, o el LEN inválido por resync).
            TEST_ASSERT_FALSE_MESSAGE(decoded,
                "flip de 1 bit pasó como frame válido (CRC no lo atrapó)");
            TEST_ASSERT_EQUAL_UINT32(0, decoder.frames_decoded());
        }
    }
}

// Corromper el CRC low byte (no sólo el high, que ya cubre otro test).
void test_corrupt_crc_low_byte_is_caught(void) {
    uint8_t payload[3] = { 0xDE, 0xAD, 0xBE };
    Frame in = make_frame(DOWN_LINE_STATUS, 1, payload, 3);
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(in, buf, sizeof(buf));
    buf[n - 2] ^= 0x01;  // CRC low byte (1 bit)

    FrameDecoder decoder;
    bool decoded = false;
    for (size_t i = 0; i < n; ++i) if (decoder.feed(buf[i])) decoded = true;
    TEST_ASSERT_FALSE(decoded);
    TEST_ASSERT_EQUAL_UINT32(1, decoder.crc_errors());
}

// --- Frames back-to-back sin separación + resync tras basura ---

// Mil frames pegados sin un solo byte de relleno: el decoder los separa todos.
void test_many_frames_back_to_back_no_gap(void) {
    FrameDecoder decoder;
    const int N = 1000;
    int decoded_count = 0;
    // Emitimos y alimentamos frame a frame (sin buffer gigante) para volumen.
    for (int k = 0; k < N; ++k) {
        uint8_t pl[2] = { static_cast<uint8_t>(k), static_cast<uint8_t>(k >> 8) };
        Frame f = make_frame(MsgType::DOWN_OTOS_VEL, static_cast<uint8_t>(k), pl, 2);
        uint8_t buf[PROTO_MAX_FRAME];
        size_t n = proto_encode(f, buf, sizeof(buf));
        for (size_t i = 0; i < n; ++i) {
            if (decoder.feed(buf[i])) {
                decoded_count++;
                TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(k),
                                        decoder.get_frame().seq);
            }
        }
    }
    TEST_ASSERT_EQUAL_INT(N, decoded_count);
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(N), decoder.frames_decoded());
    TEST_ASSERT_EQUAL_UINT32(0, decoder.crc_errors());
}

// Basura que incluye bytes START en medio: el decoder reescanea y se engancha
// al PRIMER frame realmente válido (sync por CRC+END, no por el primer 0xAA).
void test_resync_after_garbage_with_spurious_start_bytes(void) {
    Frame in = make_frame(DOWN_LINE_STATUS, 0x12, (const uint8_t[]){1,2,3}, 3);
    uint8_t frame[PROTO_MAX_FRAME];
    size_t n = proto_encode(in, frame, sizeof(frame));

    FrameDecoder decoder;
    // Basura con varios 0xAA sueltos (falsos START) y un 0x55 suelto.
    const uint8_t garbage[] = { 0xAA, 0x01, 0xAA, 0x55, 0xAA, 0xFF, 0x00, 0xAA };
    for (uint8_t b : garbage) decoder.feed(b);

    bool decoded = false;
    for (size_t i = 0; i < n; ++i) if (decoder.feed(frame[i])) decoded = true;
    TEST_ASSERT_TRUE(decoded);
    TEST_ASSERT_EQUAL_UINT32(1, decoder.frames_decoded());
    const Frame& out = decoder.get_frame();
    TEST_ASSERT_EQUAL_UINT8(0x12, out.seq);
    TEST_ASSERT_EQUAL_size_t(3, out.payload_len);
}

// Frame partido a la mitad, seguido de frames enteros: el parcial se pierde
// pero el decoder NO queda permanentemente desincronizado — recupera. Como un
// frame truncado puede "consumir" parte de la cabecera del siguiente (no hay
// stuffing), enviamos DOS frames completos tras el parcial y exigimos que al
// menos el ÚLTIMO decodifique limpio con su SEQ. Pinea: la corrupción no se
// propaga indefinidamente; el re-escaneo de START + CRC reengancha.
void test_truncated_frame_then_decoder_recovers(void) {
    Frame a = make_frame(MsgType::DEBUG_PING, 1, (const uint8_t[]){0xAA,0xBB}, 2);
    Frame b = make_frame(MsgType::DEBUG_PING, 2, (const uint8_t[]){0xCC,0xDD}, 2);
    Frame c = make_frame(MsgType::DEBUG_PING, 3, (const uint8_t[]){0xEE,0xFF}, 2);
    uint8_t bufa[PROTO_MAX_FRAME], bufb[PROTO_MAX_FRAME], bufc[PROTO_MAX_FRAME];
    size_t na = proto_encode(a, bufa, sizeof(bufa));
    (void)na;
    size_t nb = proto_encode(b, bufb, sizeof(bufb));
    size_t nc = proto_encode(c, bufc, sizeof(bufc));

    FrameDecoder decoder;
    uint8_t last_seq = 0xFF;
    int decoded = 0;
    // Sólo los primeros 4 bytes de A (START,LEN,TYPE,SEQ) → frame truncado.
    for (size_t i = 0; i < 4; ++i)
        if (decoder.feed(bufa[i])) { decoded++; last_seq = decoder.get_frame().seq; }
    for (size_t i = 0; i < nb; ++i)
        if (decoder.feed(bufb[i])) { decoded++; last_seq = decoder.get_frame().seq; }
    for (size_t i = 0; i < nc; ++i)
        if (decoder.feed(bufc[i])) { decoded++; last_seq = decoder.get_frame().seq; }

    // El último frame (C, seq=3) SIEMPRE debe haber decodificado: el decoder
    // reenganchó. (No fijamos cuántos en total porque el truncado puede comerse
    // parte de la cabecera de B.)
    TEST_ASSERT_TRUE_MESSAGE(decoded >= 1, "el decoder no recuperó tras truncado");
    TEST_ASSERT_EQUAL_UINT8(3, last_seq);
    TEST_ASSERT_EQUAL_UINT32(0, decoder.crc_errors());
}

// Documenta el comportamiento exacto de resync: cuando el byte END está mal y
// resulta ser 0xAA (un START), el decoder NO lo reinterpreta como inicio del
// próximo frame (ya lo consumió en READ_END). El frame que sigue SÍ se decodifica
// porque trae su propio START. (Pinea: el resync es por re-escaneo, no por
// stuffing; un START "perdido" en posición de END no arranca un frame.)
void test_end_byte_collision_with_start_does_not_chain(void) {
    Frame in = make_frame(MsgType::DEBUG_PING, 0, nullptr, 0);
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(in, buf, sizeof(buf));
    buf[n - 1] = PROTO_START;  // END corrupto que ADEMÁS es un START

    FrameDecoder decoder;
    bool decoded = false;
    for (size_t i = 0; i < n; ++i) if (decoder.feed(buf[i])) decoded = true;
    TEST_ASSERT_FALSE(decoded);
    TEST_ASSERT_EQUAL_UINT32(1, decoder.resync_events());

    // Un frame siguiente CON su propio START sí se engancha.
    Frame nx = make_frame(MsgType::DEBUG_PING, 7, nullptr, 0);
    uint8_t buf2[PROTO_MAX_FRAME];
    size_t n2 = proto_encode(nx, buf2, sizeof(buf2));
    bool decoded2 = false;
    for (size_t i = 0; i < n2; ++i) if (decoder.feed(buf2[i])) decoded2 = true;
    TEST_ASSERT_TRUE(decoded2);
    TEST_ASSERT_EQUAL_UINT8(7, decoder.get_frame().seq);
}

// reset() en medio de un frame descarta el parcial; el siguiente frame entero
// decodifica limpio.
void test_reset_midframe_discards_partial(void) {
    Frame in = make_frame(DOWN_LINE_STATUS, 5, (const uint8_t[]){9,9,9,9}, 4);
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(in, buf, sizeof(buf));

    FrameDecoder decoder;
    for (size_t i = 0; i < 4; ++i) decoder.feed(buf[i]);  // mitad
    decoder.reset();
    bool decoded = false;
    for (size_t i = 0; i < n; ++i) if (decoder.feed(buf[i])) decoded = true;
    TEST_ASSERT_TRUE(decoded);
    TEST_ASSERT_EQUAL_UINT8(5, decoder.get_frame().seq);
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

    // TASK-031 — colisión de payload con marcadores
    RUN_TEST(test_payload_full_of_start_bytes_decodes);
    RUN_TEST(test_payload_full_of_end_bytes_decodes);
    RUN_TEST(test_markers_in_len_type_seq_and_payload_decodes);
    RUN_TEST(test_crc_byte_equal_end_marker_does_not_truncate);

    // TASK-031 — LEN al límite
    RUN_TEST(test_len_zero_roundtrips);
    RUN_TEST(test_len_max_payload_roundtrips);
    RUN_TEST(test_len_just_over_max_is_rejected_on_wire);
    RUN_TEST(test_encode_rejects_payload_len_over_max);

    // TASK-031 — SEQ wrap
    RUN_TEST(test_seq_wraps_255_to_0);
    RUN_TEST(test_seq_wrap_back_to_back_sequence);

    // TASK-031 — CRC corrupto en distintas posiciones
    RUN_TEST(test_single_bit_flip_anywhere_is_caught);
    RUN_TEST(test_corrupt_crc_low_byte_is_caught);

    // TASK-031 — back-to-back + resync tras basura
    RUN_TEST(test_many_frames_back_to_back_no_gap);
    RUN_TEST(test_resync_after_garbage_with_spurious_start_bytes);
    RUN_TEST(test_truncated_frame_then_decoder_recovers);
    RUN_TEST(test_end_byte_collision_with_start_does_not_chain);
    RUN_TEST(test_reset_midframe_discards_partial);

    return UNITY_END();
}
