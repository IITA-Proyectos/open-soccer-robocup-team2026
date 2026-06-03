// test_central_otos_ingest — corre con: pio test -e test_native -f test_central_otos_ingest
//
// Capa 1 del broadcast simétrico (2026-06-01): DOWN difunde Pose2D (0x11) +
// Velocity2D (0x12) también a CENTRAL. Probamos el chain encode→decode→extract con
// los helpers puros de pose_view.h (los mismos que usará comm_down.cpp del CENTRAL).
#include <unity.h>
#include <string.h>
#include "types.h"
#include "proto.h"
#include "pose_view.h"
using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Encodea un payload tipado como frame proto y lo redecodifica byte-a-byte.
template <typename T>
static bool encode_then_decode(MsgType type, const T& payload, Frame& out) {
    Frame f{};
    f.type = type;
    f.seq = 0x07;
    f.payload_len = sizeof(T);
    memcpy(f.payload, &payload, sizeof(T));
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    if (n == 0) return false;
    FrameDecoder dec;
    bool got = false;
    for (size_t i = 0; i < n; ++i) if (dec.feed(buf[i])) got = true;
    if (got) out = dec.get_frame();
    return got;
}

void test_pose_roundtrip(void) {
    Pose2D p{};
    p.x_mm = 1234; p.y_mm = -567; p.heading_centideg = 9000; p.confidence = 100;
    Frame f{};
    TEST_ASSERT_TRUE(encode_then_decode(MsgType::DOWN_OTOS_POSE, p, f));
    Pose2D got{};
    TEST_ASSERT_TRUE(pose_from_frame(f, got));
    TEST_ASSERT_EQUAL_INT16(1234, got.x_mm);
    TEST_ASSERT_EQUAL_INT16(-567, got.y_mm);
    TEST_ASSERT_EQUAL_INT16(9000, got.heading_centideg);
    TEST_ASSERT_EQUAL_UINT8(100, got.confidence);
}

void test_vel_roundtrip(void) {
    Velocity2D v{};
    v.vx_mm_s = 300; v.vy_mm_s = -120; v.omega_centideg_s = 4500; v.slip_estimate = 7;
    Frame f{};
    TEST_ASSERT_TRUE(encode_then_decode(MsgType::DOWN_OTOS_VEL, v, f));
    Velocity2D got{};
    TEST_ASSERT_TRUE(vel_from_frame(f, got));
    TEST_ASSERT_EQUAL_INT16(300, got.vx_mm_s);
    TEST_ASSERT_EQUAL_INT16(-120, got.vy_mm_s);
    TEST_ASSERT_EQUAL_INT16(4500, got.omega_centideg_s);
    TEST_ASSERT_EQUAL_UINT8(7, got.slip_estimate);
}

// pose_from_frame rechaza un frame de tipo equivocado (no es pose).
void test_pose_wrong_type_rejected(void) {
    Pose2D p{};
    Frame f{};
    TEST_ASSERT_TRUE(encode_then_decode(MsgType::LINE_URGENT, p, f));
    Pose2D got{};
    TEST_ASSERT_FALSE(pose_from_frame(f, got));
}

// vel_from_frame rechaza un payload de tamaño equivocado.
void test_vel_wrong_size_rejected(void) {
    Frame f{};
    f.type = MsgType::DOWN_OTOS_VEL;
    f.payload_len = 3;  // != sizeof(Velocity2D) (7)
    Velocity2D got{};
    TEST_ASSERT_FALSE(vel_from_frame(f, got));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_pose_roundtrip);
    RUN_TEST(test_vel_roundtrip);
    RUN_TEST(test_pose_wrong_type_rejected);
    RUN_TEST(test_vel_wrong_size_rejected);
    return UNITY_END();
}
