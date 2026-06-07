// test_telemetry_top — tests del módulo puro telemetry_top.{h,cpp} (FASE 2).
// Corre con: bash scripts/run-host-tests.sh test_telemetry_top
//
// GOLDEN exacto + sentinels + overflow + parser de comandos. El mismo golden se
// commitea como fixture de la app (tools/monitor-base/tests/golden_top_v1.jsonl).

#include <unity.h>
#include <cstring>
#include "telemetry_top.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

static void make_golden_frame(TopTelemetryFrame& f) {
    tt_frame_init(f, 4);
    f.seq = 3; f.t_ms = 5000;
    f.cam_front_ok = 1; f.cam_back_ok = 0;
    f.ball_visible = 1; f.ball_x_mm = -120; f.ball_y_mm = 340;
    f.ball_confidence = 77; f.ball_vx_mm_s = -15; f.ball_vy_mm_s = 200;
    f.goal_yellow_visible = 1; f.goal_yellow_angle_cd = 4500; f.goal_yellow_distance_mm = 1200;
    f.goal_blue_visible = 0; f.goal_blue_angle_cd = -9000; f.goal_blue_distance_mm = 2500;
    f.cam_crc_errors = 2; f.cam_resyncs = 5;
    f.imu_heading_deg = 42.50f; f.imu_left_deg = 42.10f; f.imu_right_deg = 42.90f;
    f.imu_disagreement_deg = 0.80f; f.imu_left_ok = 1; f.imu_right_ok = 1; f.imu_heading_valid = 1;
    f.tof_mm[0] = 150; f.tof_mm[1] = 800; f.tof_mm[2] = 65535; f.tof_mm[3] = 1200;
    f.hcsr04_mm = 300; f.tof_min_mm = 150;
    f.snap_valid = 1; f.snap_my_x_mm = 100; f.snap_my_y_mm = -200;
    f.snap_my_heading_cd = 4250; f.snap_my_confidence = 80;
    f.snap_ball_x_mm = -118; f.snap_ball_y_mm = 338; f.snap_ball_visible = 1;
    f.snap_ball_confidence = 77; f.snap_ball_vx_mm_s = -15; f.snap_ball_vy_mm_s = 200;
    f.snap_goal_opp_angle_cd = 4500; f.snap_goal_opp_distance_mm = 1200; f.snap_goal_opp_visible = 1;
    f.snap_goal_own_visible = 0; f.snap_goal_own_angle_cd = 0; f.snap_goal_own_distance_mm = 0;
    f.snap_min_obstacle_mm = 150; f.snap_referee_cmd = 1; f.snap_flags = 0x18;
    f.frames_sent = 1234;
}

static const char* GOLDEN =
    "{\"v\":1,\"seq\":3,\"t_ms\":5000,\"cam\":{\"fok\":1,\"bok\":0,\"bvis\":1,"
    "\"bx\":-120,\"by\":340,\"bconf\":77,\"bvx\":-15,\"bvy\":200,\"gy_vis\":1,"
    "\"gy_ang\":4500,\"gy_dist\":1200,\"gb_vis\":0,\"gb_ang\":-9000,"
    "\"gb_dist\":2500,\"crc\":2,\"resync\":5},\"imu\":{\"hdg\":42.50,"
    "\"left\":42.10,\"right\":42.90,\"disagree\":0.80,\"lok\":1,\"rok\":1,"
    "\"valid\":1},\"tof\":{\"n\":4,\"d\":[150,800,65535,1200],\"hc\":300,"
    "\"min\":150},\"snap\":{\"valid\":1,\"x\":100,\"y\":-200,\"hdg_cd\":4250,"
    "\"conf\":80,\"bx\":-118,\"by\":338,\"bvis\":1,\"bconf\":77,\"bvx\":-15,"
    "\"bvy\":200,\"opp_ang\":4500,\"opp_dist\":1200,\"opp_vis\":1,\"own_vis\":0,"
    "\"own_ang\":0,\"own_dist\":0,\"obst\":150,\"ref\":1,\"flags\":24},"
    "\"diag\":{\"frames_sent\":1234}}\n";

void test_tt_serialize_golden_exact(void) {
    TopTelemetryFrame f;
    make_golden_frame(f);
    char buf[2048];
    const int n = tt_serialize_jsonl(buf, sizeof(buf), f);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_INT((int)strlen(buf), n);
    TEST_ASSERT_EQUAL_STRING(GOLDEN, buf);
    TEST_ASSERT_EQUAL_CHAR('\n', buf[n - 1]);
}

void test_tt_init_tof_sentinels(void) {
    // Frame recién init → ToF en sentinel 65535, num_tof clampeado.
    TopTelemetryFrame f;
    tt_frame_init(f, 4);
    char buf[2048];
    const int n = tt_serialize_jsonl(buf, sizeof(buf), f);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"d\":[65535,65535,65535,65535]"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"hc\":65535"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"min\":65535"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"snap\":{\"valid\":0"));
}

void test_tt_init_sets_schema(void) {
    TopTelemetryFrame f;
    tt_frame_init(f, 4);
    TEST_ASSERT_EQUAL_UINT8(TELEMETRY_TOP_SCHEMA, f.schema);
    TEST_ASSERT_EQUAL_UINT8(4, f.num_tof);
}

void test_tt_clamps_num_tof(void) {
    TopTelemetryFrame f;
    tt_frame_init(f, 99);
    TEST_ASSERT_EQUAL_UINT8(TT_MAX_TOF, f.num_tof);
}

void test_tt_buffer_too_small_returns_neg1(void) {
    TopTelemetryFrame f;
    make_golden_frame(f);
    char small[64];
    TEST_ASSERT_EQUAL_INT(-1, tt_serialize_jsonl(small, sizeof(small), f));
}

void test_tt_null_buf_returns_neg1(void) {
    TopTelemetryFrame f;
    tt_frame_init(f, 4);
    TEST_ASSERT_EQUAL_INT(-1, tt_serialize_jsonl(nullptr, 2048, f));
    char buf[8];
    TEST_ASSERT_EQUAL_INT(-1, tt_serialize_jsonl(buf, 0, f));
}

void test_tt_negative_fields(void) {
    TopTelemetryFrame f;
    tt_frame_init(f, 4);
    f.ball_x_mm = -300;
    f.snap_my_heading_cd = -17000;
    f.imu_heading_deg = -123.45f;
    char buf[2048];
    TEST_ASSERT_TRUE(tt_serialize_jsonl(buf, sizeof(buf), f) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"bx\":-300"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"hdg_cd\":-17000"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"hdg\":-123.45"));
}

// ── Parser de comandos ──
static TtCommand parse(const char* s) {
    return tt_parse_command(s, (int)strlen(s));
}

void test_tt_parse_empty_none(void) {
    TEST_ASSERT_EQUAL(TtCmd::NONE, parse("").cmd);
    TEST_ASSERT_EQUAL(TtCmd::NONE, parse("  \r\n").cmd);
    TEST_ASSERT_EQUAL(TtCmd::NONE, tt_parse_command(nullptr, 0).cmd);
}

void test_tt_parse_ping_stream(void) {
    TEST_ASSERT_EQUAL(TtCmd::PING, parse("ping").cmd);
    TEST_ASSERT_EQUAL(TtCmd::STREAM_ON, parse("STREAM ON").cmd);
    TEST_ASSERT_EQUAL(TtCmd::STREAM_OFF, parse("stream off").cmd);
    TEST_ASSERT_EQUAL(TtCmd::UNKNOWN, parse("STREAM").cmd);
}

void test_tt_parse_rate(void) {
    TtCommand c = parse("RATE 30");
    TEST_ASSERT_EQUAL(TtCmd::SET_RATE, c.cmd);
    TEST_ASSERT_EQUAL_INT32(30, c.arg);
    TEST_ASSERT_EQUAL(TtCmd::UNKNOWN, parse("RATE 0").cmd);
    TEST_ASSERT_EQUAL(TtCmd::UNKNOWN, parse("RATE x").cmd);
}

void test_tt_parse_imu(void) {
    TEST_ASSERT_EQUAL(TtCmd::IMU_ZERO, parse("IMU ZERO").cmd);
    TEST_ASSERT_EQUAL(TtCmd::IMU_SAVE, parse("imu save").cmd);
    TEST_ASSERT_EQUAL(TtCmd::UNKNOWN, parse("IMU").cmd);
    TEST_ASSERT_EQUAL(TtCmd::UNKNOWN, parse("IMU FOO").cmd);
}

void test_tt_parse_unknown(void) {
    TEST_ASSERT_EQUAL(TtCmd::UNKNOWN, parse("HELLO").cmd);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_tt_serialize_golden_exact);
    RUN_TEST(test_tt_init_tof_sentinels);
    RUN_TEST(test_tt_init_sets_schema);
    RUN_TEST(test_tt_clamps_num_tof);
    RUN_TEST(test_tt_buffer_too_small_returns_neg1);
    RUN_TEST(test_tt_null_buf_returns_neg1);
    RUN_TEST(test_tt_negative_fields);
    RUN_TEST(test_tt_parse_empty_none);
    RUN_TEST(test_tt_parse_ping_stream);
    RUN_TEST(test_tt_parse_rate);
    RUN_TEST(test_tt_parse_imu);
    RUN_TEST(test_tt_parse_unknown);
    return UNITY_END();
}
