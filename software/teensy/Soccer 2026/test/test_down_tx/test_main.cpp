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
//
// TASK-031 — blindaje del enlace (tests host añadidos):
//   5) Los TRES frames del broadcast (LINE 0x10, POSE 0x11, VEL 0x12)
//      round-trip-ean por el FrameDecoder con su TYPE/SEQ/payload.
//   6) El SEQ es MONÓTONO POR ENLACE: down_tx incrementa lk.seq++ una vez por
//      frame enviado, COMPARTIDO entre los 3 tipos (ver down_tx.h). Se simula el
//      bucle de broadcast de down_tx.cpp y se verifica la cadena de SEQ que ve
//      el receptor de cada enlace.
//   7) Un frame CORRUPTO en medio del stream de un enlace NO desincroniza los
//      siguientes (resync por re-escaneo + CRC).
//
// NOTA: down_tx.cpp incluye <Arduino.h> (HardwareSerial), así que NO se linkea
// host. Para POSE/VEL replicamos EXACTAMENTE su encoder interno (encode_generic:
// Frame{type,seq, payload=memcpy(struct)} → proto_encode) — el mismo armado que
// corre en producción. Si alguien cambia ese armado o el layout de Pose2D/
// Velocity2D, estos tests lo gritan. (La LÍNEA sí usa el encoder vivo real,
// down_encode_line.)

#include <unity.h>
#include <cstring>
#include "types.h"
#include "proto.h"
#include "down_encode.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Réplica host-side del encoder interno de down_tx.cpp (encode_generic), usado
// por down_tx_broadcast_pose() y _vel(). Mismo armado byte-a-byte.
template <typename T>
static size_t encode_generic_like_down_tx(MsgType type, uint8_t seq, const T& payload,
                                          uint8_t* out, size_t out_size) {
    Frame f{};
    f.type = type;
    f.seq = seq;
    f.payload_len = static_cast<uint8_t>(sizeof(T));
    memcpy(f.payload, &payload, sizeof(T));
    return proto_encode(f, out, out_size);
}

static Pose2D make_pose(void) {
    Pose2D p{};
    p.x_mm = -1234;
    p.y_mm = 2200;
    p.heading_centideg = -4500;
    p.confidence = 77;
    return p;
}

static Velocity2D make_vel(void) {
    Velocity2D v{};
    v.vx_mm_s = 250;
    v.vy_mm_s = -310;
    v.omega_centideg_s = 1200;
    v.slip_estimate = 9;
    return v;
}

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

// ============================================================================
// TASK-031 — los 3 frames del broadcast round-trip-ean (LINE, POSE, VEL)
// ============================================================================

// POSE (0x11): el frame que arma down_tx_broadcast_pose() decodifica con su
// TYPE=DOWN_OTOS_POSE, SEQ y payload Pose2D intactos.
void test_broadcast_pose_frame_roundtrips(void) {
    Pose2D p = make_pose();
    uint8_t out[PROTO_MAX_FRAME];
    size_t n = encode_generic_like_down_tx(MsgType::DOWN_OTOS_POSE, 0x33, p,
                                           out, sizeof(out));
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_UINT8(0x11, out[2]);  // TYPE en el wire

    FrameDecoder dec;
    bool ok = false;
    for (size_t i = 0; i < n; ++i) if (dec.feed(out[i])) ok = true;
    TEST_ASSERT_TRUE(ok);

    const Frame& f = dec.get_frame();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MsgType::DOWN_OTOS_POSE),
                            static_cast<uint8_t>(f.type));
    TEST_ASSERT_EQUAL_UINT8(0x33, f.seq);
    TEST_ASSERT_EQUAL_size_t(sizeof(Pose2D), f.payload_len);

    Pose2D back{};
    memcpy(&back, f.payload, sizeof(back));
    TEST_ASSERT_EQUAL_INT16(p.x_mm, back.x_mm);
    TEST_ASSERT_EQUAL_INT16(p.y_mm, back.y_mm);
    TEST_ASSERT_EQUAL_INT16(p.heading_centideg, back.heading_centideg);
    TEST_ASSERT_EQUAL_UINT8(p.confidence, back.confidence);
    TEST_ASSERT_EQUAL_UINT32(0, dec.crc_errors());
}

// VEL (0x12): idem para down_tx_broadcast_vel() y el payload Velocity2D.
void test_broadcast_vel_frame_roundtrips(void) {
    Velocity2D v = make_vel();
    uint8_t out[PROTO_MAX_FRAME];
    size_t n = encode_generic_like_down_tx(MsgType::DOWN_OTOS_VEL, 0x44, v,
                                           out, sizeof(out));
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_UINT8(0x12, out[2]);  // TYPE en el wire

    FrameDecoder dec;
    bool ok = false;
    for (size_t i = 0; i < n; ++i) if (dec.feed(out[i])) ok = true;
    TEST_ASSERT_TRUE(ok);

    const Frame& f = dec.get_frame();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MsgType::DOWN_OTOS_VEL),
                            static_cast<uint8_t>(f.type));
    TEST_ASSERT_EQUAL_UINT8(0x44, f.seq);
    TEST_ASSERT_EQUAL_size_t(sizeof(Velocity2D), f.payload_len);

    Velocity2D back{};
    memcpy(&back, f.payload, sizeof(back));
    TEST_ASSERT_EQUAL_INT16(v.vx_mm_s, back.vx_mm_s);
    TEST_ASSERT_EQUAL_INT16(v.vy_mm_s, back.vy_mm_s);
    TEST_ASSERT_EQUAL_INT16(v.omega_centideg_s, back.omega_centideg_s);
    TEST_ASSERT_EQUAL_UINT8(v.slip_estimate, back.slip_estimate);
    TEST_ASSERT_EQUAL_UINT32(0, dec.crc_errors());
}

// Los 3 tipos pegados (LINE, POSE, VEL) decodifican cada uno con su TYPE/SEQ.
void test_three_frame_types_back_to_back(void) {
    uint8_t stream[3 * PROTO_MAX_FRAME];
    size_t total = 0;
    total += down_encode_line(make_line(), 0x10, stream + total, sizeof(stream) - total);
    total += encode_generic_like_down_tx(MsgType::DOWN_OTOS_POSE, 0x11,
                                         make_pose(), stream + total, sizeof(stream) - total);
    total += encode_generic_like_down_tx(MsgType::DOWN_OTOS_VEL, 0x12,
                                         make_vel(), stream + total, sizeof(stream) - total);

    FrameDecoder dec;
    uint8_t types[3]; uint8_t seqs[3];
    int idx = 0;
    for (size_t i = 0; i < total; ++i) {
        if (dec.feed(stream[i])) {
            types[idx] = static_cast<uint8_t>(dec.get_frame().type);
            seqs[idx]  = dec.get_frame().seq;
            idx++;
        }
    }
    TEST_ASSERT_EQUAL_INT(3, idx);
    TEST_ASSERT_EQUAL_UINT8(0x10, types[0]);  // LINE_URGENT
    TEST_ASSERT_EQUAL_UINT8(0x11, types[1]);  // DOWN_OTOS_POSE
    TEST_ASSERT_EQUAL_UINT8(0x12, types[2]);  // DOWN_OTOS_VEL
    TEST_ASSERT_EQUAL_UINT8(0x10, seqs[0]);
    TEST_ASSERT_EQUAL_UINT8(0x11, seqs[1]);
    TEST_ASSERT_EQUAL_UINT8(0x12, seqs[2]);
    TEST_ASSERT_EQUAL_UINT32(3, dec.frames_decoded());
    TEST_ASSERT_EQUAL_UINT32(0, dec.crc_errors());
}

// ============================================================================
// TASK-031 — SEQ monótono POR ENLACE (simula el bucle de down_tx)
// ============================================================================

// Simulación fiel del SEQ por enlace de down_tx.cpp: cada enlace tiene su propio
// contador lk.seq que avanza una vez por frame, COMPARTIDO entre los 3 tipos.
// Verifica que el receptor de UN enlace ve SEQ contiguos 0,1,2,... sin huecos
// (aunque los tipos se intercalen). Pinea el invariante de detección de pérdida.
void test_per_link_seq_is_monotonic_across_types(void) {
    // Un enlace. Secuencia de envíos: LINE, POSE, VEL, LINE, POSE, VEL...
    FrameDecoder dec;
    uint8_t link_seq = 0;        // == lk.seq en down_tx
    uint8_t got_seq[12];
    int idx = 0;

    for (int round = 0; round < 4; ++round) {
        uint8_t buf[PROTO_MAX_FRAME];
        size_t n;
        // LINE (encoder vivo) — SEQ por enlace, igual que down_tx_broadcast_line
        n = down_encode_line(make_line(), link_seq++, buf, sizeof(buf));
        for (size_t i = 0; i < n; ++i) if (dec.feed(buf[i])) got_seq[idx++] = dec.get_frame().seq;
        // POSE
        n = encode_generic_like_down_tx(MsgType::DOWN_OTOS_POSE, link_seq++, make_pose(), buf, sizeof(buf));
        for (size_t i = 0; i < n; ++i) if (dec.feed(buf[i])) got_seq[idx++] = dec.get_frame().seq;
        // VEL
        n = encode_generic_like_down_tx(MsgType::DOWN_OTOS_VEL, link_seq++, make_vel(), buf, sizeof(buf));
        for (size_t i = 0; i < n; ++i) if (dec.feed(buf[i])) got_seq[idx++] = dec.get_frame().seq;
    }

    TEST_ASSERT_EQUAL_INT(12, idx);
    for (int i = 0; i < 12; ++i) {
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(i), got_seq[i]);  // 0,1,2,...,11
    }
    TEST_ASSERT_EQUAL_UINT32(12, dec.frames_decoded());
}

// Dos enlaces (CENTRAL y TOP) llevan contadores SEQ INDEPENDIENTES: difundir el
// MISMO mensaje a ambos produce el MISMO SEQ en cada enlace ese tick, pero cada
// enlace avanza por su cuenta. Pinea el modelo de down_tx (g_links[2], cada uno
// con su lk.seq). Aquí ambos parten de 0 y van en paralelo → SEQ idénticos.
void test_two_links_have_independent_seq_counters(void) {
    uint8_t seq_central = 0, seq_top = 0;
    FrameDecoder dec_central, dec_top;
    uint8_t sc[3], st[3];
    int ic = 0, it = 0;

    // 3 broadcasts (line, pose, vel), cada uno a AMBOS enlaces (como down_tx).
    auto bcast = [&](MsgType type) {
        uint8_t buf[PROTO_MAX_FRAME];
        size_t n;
        if (type == MsgType::LINE_URGENT) {
            n = down_encode_line(make_line(), seq_central++, buf, sizeof(buf));
            for (size_t i = 0; i < n; ++i) if (dec_central.feed(buf[i])) sc[ic++] = dec_central.get_frame().seq;
            n = down_encode_line(make_line(), seq_top++, buf, sizeof(buf));
            for (size_t i = 0; i < n; ++i) if (dec_top.feed(buf[i])) st[it++] = dec_top.get_frame().seq;
        } else {
            Pose2D p = make_pose(); Velocity2D v = make_vel();
            n = (type == MsgType::DOWN_OTOS_POSE)
                ? encode_generic_like_down_tx(type, seq_central++, p, buf, sizeof(buf))
                : encode_generic_like_down_tx(type, seq_central++, v, buf, sizeof(buf));
            for (size_t i = 0; i < n; ++i) if (dec_central.feed(buf[i])) sc[ic++] = dec_central.get_frame().seq;
            n = (type == MsgType::DOWN_OTOS_POSE)
                ? encode_generic_like_down_tx(type, seq_top++, p, buf, sizeof(buf))
                : encode_generic_like_down_tx(type, seq_top++, v, buf, sizeof(buf));
            for (size_t i = 0; i < n; ++i) if (dec_top.feed(buf[i])) st[it++] = dec_top.get_frame().seq;
        }
    };
    bcast(MsgType::LINE_URGENT);
    bcast(MsgType::DOWN_OTOS_POSE);
    bcast(MsgType::DOWN_OTOS_VEL);

    TEST_ASSERT_EQUAL_INT(3, ic);
    TEST_ASSERT_EQUAL_INT(3, it);
    // Ambos enlaces en paralelo desde 0 → 0,1,2 cada uno.
    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(i), sc[i]);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(i), st[i]);
    }
}

// ============================================================================
// TASK-031 — frame corrupto en medio NO desincroniza los siguientes
// ============================================================================

// Stream de un enlace: POSE, LINE(corrupto CRC), VEL. El corrupto se descarta
// (crc_errors++) pero VEL decodifica igual con su SEQ. Pinea resync en el camino
// vivo del broadcast.
void test_corrupt_middle_frame_does_not_desync_link(void) {
    uint8_t stream[3 * PROTO_MAX_FRAME];
    size_t total = 0;
    // POSE seq=0
    total += encode_generic_like_down_tx(MsgType::DOWN_OTOS_POSE, 0, make_pose(),
                                         stream + total, sizeof(stream) - total);
    // LINE seq=1 — guardamos su rango para corromperle el CRC.
    size_t line_start = total;
    size_t line_n = down_encode_line(make_line(), 1, stream + total, sizeof(stream) - total);
    total += line_n;
    stream[line_start + line_n - 3] ^= 0xFF;  // corromper CRC high del LINE
    // VEL seq=2
    total += encode_generic_like_down_tx(MsgType::DOWN_OTOS_VEL, 2, make_vel(),
                                         stream + total, sizeof(stream) - total);

    FrameDecoder dec;
    uint8_t types[2]; uint8_t seqs[2];
    int idx = 0;
    for (size_t i = 0; i < total; ++i) {
        if (dec.feed(stream[i])) {
            types[idx] = static_cast<uint8_t>(dec.get_frame().type);
            seqs[idx]  = dec.get_frame().seq;
            idx++;
        }
    }
    TEST_ASSERT_EQUAL_INT(2, idx);                 // POSE y VEL; LINE cayó
    TEST_ASSERT_EQUAL_UINT8(0x11, types[0]);       // POSE
    TEST_ASSERT_EQUAL_UINT8(0,    seqs[0]);
    TEST_ASSERT_EQUAL_UINT8(0x12, types[1]);       // VEL — recuperado tras el corrupto
    TEST_ASSERT_EQUAL_UINT8(2,    seqs[1]);
    TEST_ASSERT_EQUAL_UINT32(1, dec.crc_errors());
    TEST_ASSERT_EQUAL_UINT32(2, dec.frames_decoded());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_live_line_frame_matches_contract_golden);
    RUN_TEST(test_live_line_frame_roundtrips);
    RUN_TEST(test_live_line_frame_carries_seq);
    RUN_TEST(test_live_line_frame_edge_values);

    // TASK-031 — los 3 frames del broadcast
    RUN_TEST(test_broadcast_pose_frame_roundtrips);
    RUN_TEST(test_broadcast_vel_frame_roundtrips);
    RUN_TEST(test_three_frame_types_back_to_back);

    // TASK-031 — SEQ monótono por enlace
    RUN_TEST(test_per_link_seq_is_monotonic_across_types);
    RUN_TEST(test_two_links_have_independent_seq_counters);

    // TASK-031 — resync tras corrupción
    RUN_TEST(test_corrupt_middle_frame_does_not_desync_link);

    return UNITY_END();
}
