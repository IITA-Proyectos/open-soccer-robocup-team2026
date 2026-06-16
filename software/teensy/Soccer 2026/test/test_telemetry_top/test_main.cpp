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
    f.tof_zones[0][0] = 140; f.tof_zones[0][1] = 160; f.tof_zones[0][3] = 175;  // v2 "z" (aditivo): zona 2 queda sentinel
    f.snap_valid = 1; f.snap_my_x_mm = 100; f.snap_my_y_mm = -200;
    f.snap_my_heading_cd = 4250; f.snap_my_confidence = 80;
    f.snap_ball_x_mm = -118; f.snap_ball_y_mm = 338; f.snap_ball_visible = 1;
    f.snap_ball_confidence = 77; f.snap_ball_vx_mm_s = -15; f.snap_ball_vy_mm_s = 200;
    f.snap_goal_opp_angle_cd = 4500; f.snap_goal_opp_distance_mm = 1200; f.snap_goal_opp_visible = 1;
    f.snap_goal_own_visible = 0; f.snap_goal_own_angle_cd = 0; f.snap_goal_own_distance_mm = 0;
    f.snap_min_obstacle_mm = 150; f.snap_referee_cmd = 1; f.snap_flags = 0x18;
    f.frames_sent = 1234;
    // v2 — detecciones POR CÁMARA (front/back distintos a propósito → delta visible)
    f.ball_front_visible = 1; f.ball_front_x_mm = -118; f.ball_front_y_mm = 338;
    f.ball_back_visible = 0;  f.ball_back_x_mm = 0; f.ball_back_y_mm = 0;
    f.goal_yellow_front_visible = 1; f.goal_yellow_front_angle_cd = 4500; f.goal_yellow_front_distance_mm = 1200;
    f.goal_yellow_back_visible = 0;
    f.goal_blue_front_visible = 0;
    f.goal_blue_back_visible = 1; f.goal_blue_back_angle_cd = -9000; f.goal_blue_back_distance_mm = 800;
    // v2 — base (DOWN): pose fresh + vel fresh + línea válida; cross_track queda en
    // sentinela N/A (no se sobreescribe) para cubrir el camino "--".
    f.down_pose_fresh = 1; f.down_pose_x_mm = 1500; f.down_pose_y_mm = 2000;
    f.down_pose_heading_cd = 9000; f.down_pose_confidence = 75;
    f.down_vel_fresh = 1; f.down_vel_vx_mm_s = 120; f.down_vel_vy_mm_s = -30;
    f.down_vel_omega_cd_s = 4500; f.down_vel_slip = 12;
    f.down_line_fresh = 1; f.down_line_schema = 2; f.down_line_data_valid = 1;
    f.down_line_angle_cd = 4500; f.down_escape_angle_cd = -13500; f.down_line_penetration_mm = 80;
    f.down_line_present = 1; f.down_line_sensors_on = 7; f.down_line_event_flags = 0x01;
    f.down_line_quality = 88; f.down_line_sample_age_ms = 3;
}

// GOLDEN v2 (regenerado 2026-06-14, A1): +camf/camb (per-cámara) +base/line (DOWN).
// Fuente de verdad cross-lenguaje: idéntico a tools/monitor-base/tests/golden_top_v2.jsonl.
static const char* GOLDEN =
    "{\"v\":2,\"seq\":3,\"t_ms\":5000,\"cam\":{\"fok\":1,\"bok\":0,\"bvis\":1,\"bx\":-120,\"by\":340,\"bconf\":77,\"bvx\":-15,\"bvy\":200,\"gy_vis\":1,\"gy_ang\":4500,\"gy_dist\":1200,\"gb_vis\":0,\"gb_ang\":-9000,\"gb_dist\":2500,\"crc\":2,\"resync\":5},\"camf\":{\"bvis\":1,\"bx\":-118,\"by\":338,\"gy_vis\":1,\"gy_ang\":4500,\"gy_dist\":1200,\"gb_vis\":0,\"gb_ang\":0,\"gb_dist\":0},\"camb\":{\"bvis\":0,\"bx\":0,\"by\":0,\"gy_vis\":0,\"gy_ang\":0,\"gy_dist\":0,\"gb_vis\":1,\"gb_ang\":-9000,\"gb_dist\":800},\"imu\":{\"hdg\":42.50,\"left\":42.10,\"right\":42.90,\"disagree\":0.80,\"lok\":1,\"rok\":1,\"valid\":1,\"sok\":0,\"sdeg\":0.00},\"tof\":{\"n\":4,\"d\":[150,800,65535,1200],\"hc\":300,\"min\":150,\"z\":[[140,160,65535,175,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535],[65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535],[65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535],[65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535]]},\"snap\":{\"valid\":1,\"x\":100,\"y\":-200,\"hdg_cd\":4250,\"conf\":80,\"bx\":-118,\"by\":338,\"bvis\":1,\"bconf\":77,\"bvx\":-15,\"bvy\":200,\"opp_ang\":4500,\"opp_dist\":1200,\"opp_vis\":1,\"own_vis\":0,\"own_ang\":0,\"own_dist\":0,\"obst\":150,\"ref\":1,\"flags\":24},\"base\":{\"pfresh\":1,\"px\":1500,\"py\":2000,\"phdg_cd\":9000,\"pconf\":75,\"vfresh\":1,\"vx\":120,\"vy\":-30,\"omega\":4500,\"slip\":12},\"line\":{\"fresh\":1,\"schema\":2,\"valid\":1,\"angle_cd\":4500,\"escape_cd\":-13500,\"pen_mm\":80,\"cross_mm\":-32768,\"present\":1,\"sensors\":7,\"events\":1,\"quality\":88,\"age_ms\":3},\"diag\":{\"frames_sent\":1234,\"xval_v\":0,\"xval_s\":0,\"xval_ni\":0}}\n";

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

// ── Formato humano (bloque de texto legible — modo ENTER) ──
// telemetry_top gana una tercera salida (2026-06-13, TASK-205): además del JSON
// para la app, un bloque de TEXTO que el monitor dormido de competencia imprime
// cuando un humano aprieta ENTER en un monitor serie crudo.

void test_tt_human_shape_and_sections(void) {
    TopTelemetryFrame f;
    make_golden_frame(f);
    char buf[1024];
    const int n = tt_format_human(buf, sizeof(buf), f);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_INT((int)strlen(buf), n);
    TEST_ASSERT_EQUAL_CHAR('\n', buf[n - 1]);
    // Las 5 secciones presentes y etiquetadas (no es JSON: arranca con '[').
    TEST_ASSERT_EQUAL_CHAR('[', buf[0]);
    TEST_ASSERT_NOT_NULL(strstr(buf, "[TOP] seq 3"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "  CAM "));
    TEST_ASSERT_NOT_NULL(strstr(buf, "  CAMF "));
    TEST_ASSERT_NOT_NULL(strstr(buf, "  CAMB "));
    TEST_ASSERT_NOT_NULL(strstr(buf, "  IMU "));
    TEST_ASSERT_NOT_NULL(strstr(buf, "  ToF "));
    TEST_ASSERT_NOT_NULL(strstr(buf, "  SNAP "));
    TEST_ASSERT_NOT_NULL(strstr(buf, "  BASE "));
    TEST_ASSERT_NOT_NULL(strstr(buf, "  LINE "));
}

// v2: per-cámara + base/línea — valores y caminos FRESH/STALE/N-A.
void test_tt_human_v2_per_camera_and_base(void) {
    TopTelemetryFrame f;
    make_golden_frame(f);
    char buf[1024];
    TEST_ASSERT_TRUE(tt_format_human(buf, sizeof(buf), f) > 0);
    // per-cámara: la frontal ve la pelota en (-118,338); la trasera no.
    TEST_ASSERT_NOT_NULL(strstr(buf, "CAMF ball(-118,338)"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "CAMB ball --"));
    // base fresh + línea con vector de escape (escape_cd=-13500 → -135.0)
    TEST_ASSERT_NOT_NULL(strstr(buf, "BASE pose x1500 y2000"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "LINE FRESH"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "esc a-135.0"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "pen80mm"));
}

void test_tt_human_base_stale_and_line_na(void) {
    // Frame init: base NO fresh, línea sin datos → STALE + N/A, sin ceros falsos.
    TopTelemetryFrame f;
    tt_frame_init(f, 4);
    f.seq = 1;
    char buf[1024];
    TEST_ASSERT_TRUE(tt_format_human(buf, sizeof(buf), f) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "BASE pose STALE"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "vel STALE"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "LINE STALE"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "angle --"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "esc --"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "pen --"));
}

void test_tt_human_values_visible(void) {
    TopTelemetryFrame f;
    make_golden_frame(f);
    char buf[1024];
    TEST_ASSERT_TRUE(tt_format_human(buf, sizeof(buf), f) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "ball(-120,340) c77 v(-15,200)"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "GY a45.0 d1200"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "GB --"));               // arco azul no visible
    TEST_ASSERT_NOT_NULL(strstr(buf, "IMU hdg 42.50 VALID"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "ToF n=4 [150,800,--,1200]"));  // tof[2]=sentinel → --
    TEST_ASSERT_NOT_NULL(strstr(buf, "min=150 mm"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "flags0x18"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "own --"));              // arco propio no visible
}

void test_tt_human_dashes_when_absent(void) {
    // Frame init: sin cámaras vivas, sin pelota, sin snapshot, ToF en sentinel.
    TopTelemetryFrame f;
    tt_frame_init(f, 4);
    f.seq = 1;
    char buf[1024];
    TEST_ASSERT_TRUE(tt_format_human(buf, sizeof(buf), f) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "F:-- B:--"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "ball --"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "[--,--,--,--]"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "min=-- mm"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "IMU hdg 0.00 INVALID"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "SNAP (sin snapshot todavia)"));
}

void test_tt_human_buffer_too_small_returns_neg1(void) {
    TopTelemetryFrame f;
    make_golden_frame(f);
    char small[16];
    TEST_ASSERT_EQUAL_INT(-1, tt_format_human(small, sizeof(small), f));
}

void test_tt_human_null_args_return_neg1(void) {
    TopTelemetryFrame f;
    make_golden_frame(f);
    TEST_ASSERT_EQUAL_INT(-1, tt_format_human(nullptr, 768, f));
    char buf[8];
    TEST_ASSERT_EQUAL_INT(-1, tt_format_human(buf, 0, f));
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

// Comandos de config de sensores (A2.1).
void test_tt_parse_config_commands(void) {
    TEST_ASSERT_EQUAL(TtCmd::CAM_F_ON,  parse("CAM F ON").cmd);
    TEST_ASSERT_EQUAL(TtCmd::CAM_B_OFF, parse("cam b off").cmd);
    TEST_ASSERT_EQUAL(TtCmd::BNO_L_ON,  parse("BNO L ON").cmd);
    TEST_ASSERT_EQUAL(TtCmd::BNO_R_OFF, parse("BNO R OFF").cmd);
    TEST_ASSERT_EQUAL(TtCmd::US_ON,     parse("US ON").cmd);
    TEST_ASSERT_EQUAL(TtCmd::US_OFF,    parse("us off").cmd);

    TtCommand t = parse("TOF 2 ON");
    TEST_ASSERT_EQUAL(TtCmd::TOF_SET_ENABLED, t.cmd);
    TEST_ASSERT_EQUAL_INT32(2, t.arg);
    TEST_ASSERT_EQUAL_INT32(1, t.arg2);
    t = parse("TOF 3 OFF");
    TEST_ASSERT_EQUAL(TtCmd::TOF_SET_ENABLED, t.cmd);
    TEST_ASSERT_EQUAL_INT32(3, t.arg);
    TEST_ASSERT_EQUAL_INT32(0, t.arg2);

    t = parse("TOF 1 POS LEFT");           // 4 tokens (TT_TOK_MAX=4)
    TEST_ASSERT_EQUAL(TtCmd::TOF_SET_POS, t.cmd);
    TEST_ASSERT_EQUAL_INT32(1, t.arg);
    TEST_ASSERT_EQUAL_INT32(270, t.arg2);
    TEST_ASSERT_EQUAL_INT32(0,   parse("TOF 0 POS FRONT").arg2);
    TEST_ASSERT_EQUAL_INT32(90,  parse("TOF 4 POS RIGHT").arg2);
    TEST_ASSERT_EQUAL_INT32(180, parse("TOF 5 POS BACK").arg2);

    TEST_ASSERT_EQUAL(TtCmd::CFG_SAVE,  parse("CFG SAVE").cmd);
    TEST_ASSERT_EQUAL(TtCmd::CFG_LOAD,  parse("cfg load").cmd);
    TEST_ASSERT_EQUAL(TtCmd::CFG_RESET, parse("CFG RESET").cmd);

    // inválidos → UNKNOWN
    TEST_ASSERT_EQUAL(TtCmd::UNKNOWN, parse("CAM X ON").cmd);
    TEST_ASSERT_EQUAL(TtCmd::UNKNOWN, parse("TOF x ON").cmd);
    TEST_ASSERT_EQUAL(TtCmd::UNKNOWN, parse("TOF 1 POS NOWHERE").cmd);
    TEST_ASSERT_EQUAL(TtCmd::UNKNOWN, parse("CFG NOPE").cmd);
}

// xval (TASK-213): default 0 al init + round-trip de los 3 campos en el JSON.
void test_tt_xval_fields(void) {
    TopTelemetryFrame f;
    tt_frame_init(f, 4);
    TEST_ASSERT_EQUAL_UINT8(0, f.xval_verdict);
    TEST_ASSERT_EQUAL_UINT8(0, f.xval_score);
    TEST_ASSERT_EQUAL_UINT8(0, f.xval_n_indep);
    f.xval_verdict = 2; f.xval_score = 73; f.xval_n_indep = 2;
    char buf[2048];
    const int n = tt_serialize_jsonl(buf, sizeof(buf), f);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"xval_v\":2"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"xval_s\":73"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"xval_ni\":2"));
}

// Centinela (TASK-213): default 0/false al init + round-trip de sok/sdeg en el JSON.
void test_tt_sentinel_fields(void) {
    TopTelemetryFrame f;
    tt_frame_init(f, 4);
    TEST_ASSERT_EQUAL_UINT8(0, f.imu_sentinel_ok);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, f.imu_sentinel_deg);
    f.imu_sentinel_ok = 1; f.imu_sentinel_deg = -33.25f;
    char buf[2048];
    const int n = tt_serialize_jsonl(buf, sizeof(buf), f);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"sok\":1"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"sdeg\":-33.25"));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_tt_xval_fields);
    RUN_TEST(test_tt_sentinel_fields);
    RUN_TEST(test_tt_serialize_golden_exact);
    RUN_TEST(test_tt_init_tof_sentinels);
    RUN_TEST(test_tt_init_sets_schema);
    RUN_TEST(test_tt_clamps_num_tof);
    RUN_TEST(test_tt_buffer_too_small_returns_neg1);
    RUN_TEST(test_tt_null_buf_returns_neg1);
    RUN_TEST(test_tt_negative_fields);
    RUN_TEST(test_tt_human_shape_and_sections);
    RUN_TEST(test_tt_human_values_visible);
    RUN_TEST(test_tt_human_dashes_when_absent);
    RUN_TEST(test_tt_human_v2_per_camera_and_base);
    RUN_TEST(test_tt_human_base_stale_and_line_na);
    RUN_TEST(test_tt_human_buffer_too_small_returns_neg1);
    RUN_TEST(test_tt_human_null_args_return_neg1);
    RUN_TEST(test_tt_parse_empty_none);
    RUN_TEST(test_tt_parse_ping_stream);
    RUN_TEST(test_tt_parse_rate);
    RUN_TEST(test_tt_parse_imu);
    RUN_TEST(test_tt_parse_config_commands);
    RUN_TEST(test_tt_parse_unknown);
    return UNITY_END();
}
