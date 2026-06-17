// test_telemetry_central — tests del módulo puro telemetry_central.{h,cpp} (Sprint A).
// Corre con: bash scripts/run-host-tests.sh test_telemetry_central
//
// GOLDEN exacto + init/sentinels + overflow + parser de comandos. El mismo golden se
// commitea como fixture de la app (tools/monitor-base/tests/golden_central_v1.jsonl).

#include <unity.h>
#include <cstring>
#include "telemetry_central.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// El golden frame es la PRIMERA línea de tools/monitor-base/tests/golden_central_v1.jsonl
// (la fixture cross-lenguaje de la app). Firmware y app DEBEN producir/consumir el MISMO
// byte exacto — por eso el frame y la cadena de abajo se copian de esa fixture verbatim.
static void make_golden_frame(CentralTelemetryFrame& f) {
    tc_frame_init(f);
    f.seq = 10; f.t_ms = 2000;
    // FSM
    f.fsm_role = 1;                  // ATK
    f.fsm_state = "ATTACK";
    f.fsm_match = 1;
    // cmd aplicado
    f.cmd_vx_mm_s = 120; f.cmd_vy_mm_s = -200; f.cmd_w_cd_s = 4250; f.cmd_spin_pwm = 0;
    // pwm real por rueda
    f.pwm[0] = 200; f.pwm[1] = -150; f.pwm[2] = 40;
    // salud TOP
    f.top_bytes = 4096; f.top_frames = 372; f.top_crc = 1; f.top_resync = 0; f.top_badsz = 0;
    f.top_fresh = 1; f.top_age_ms = 4;
    // salud DOWN
    f.down_frames = 355; f.down_crc = 2; f.down_lost = 1; f.down_resync = 0; f.down_badsch = 0;
    f.down_line_fresh = 1; f.down_valid = 1; f.down_event_flags = 0x01; f.down_age_ms = 3;
    // OTOS
    f.otos_fresh = 1; f.otos_heading_deg = 42.50f;
    // eco del snapshot
    f.snap_ball_visible = 1; f.snap_ball_x_mm = -118; f.snap_ball_y_mm = 338;
    f.snap_goal_own_visible = 1; f.snap_goal_own_angle_deg = 178.50f; f.snap_goal_own_distance_mm = 1800;
    f.snap_referee_cmd = 1; f.snap_flags = 0x18;   // 24
    // loop-time
    f.loop_max_us = 9500; f.loop_ema_us = 2200;
}

// GOLDEN v1 (2026-06-15) — IDÉNTICO a la línea 1 de
// tools/monitor-base/tests/golden_central_v1.jsonl (fuente de verdad cross-lenguaje).
static const char* GOLDEN =
    "{\"central\":1,\"v\":1,\"seq\":10,\"t_ms\":2000,"
    "\"fsm\":{\"role\":\"ATK\",\"state\":\"ATTACK\",\"match\":1},"
    "\"cmd\":{\"vx\":120,\"vy\":-200,\"w\":4250,\"spin\":0},"
    "\"pwm\":[200,-150,40],"
    "\"top\":{\"rxB\":4096,\"fr\":372,\"crc\":1,\"rsy\":0,\"badsz\":0,\"fresh\":1,\"age\":4},"
    "\"down\":{\"rx\":355,\"crc\":2,\"lost\":1,\"rsy\":0,\"badsch\":0,\"line_fresh\":1,\"valid\":1,\"ev\":1,\"age\":3},"
    "\"otos\":{\"fresh\":1,\"hdg\":42.50},"
    "\"snap\":{\"ball_vis\":1,\"ball_x\":-118,\"ball_y\":338,\"goal_own_ang\":178.50,\"goal_own_dist\":1800,\"referee\":1,\"flags\":24,"
    "\"my_x\":0,\"my_y\":0,\"my_hdg\":0.00,\"hdg_valid\":0,\"my_conf\":0,"
    "\"ball_vx\":0,\"ball_vy\":0,"
    "\"goal_opp_vis\":0,\"goal_opp_ang\":0.00,\"goal_opp_dist\":0},"
    "\"loop\":{\"max_us\":9500,\"ema_us\":2200}}\n";

void test_tc_serialize_golden_exact(void) {
    CentralTelemetryFrame f;
    make_golden_frame(f);
    char buf[1024];
    const int n = tc_serialize_jsonl(buf, sizeof(buf), f);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_INT((int)strlen(buf), n);
    TEST_ASSERT_EQUAL_STRING(GOLDEN, buf);
    TEST_ASSERT_EQUAL_CHAR('\n', buf[n - 1]);
}

void test_tc_sniff_key_and_v_present(void) {
    // La app sniffea por "central":1; is_telemetry_line exige '{' + '"v":'.
    CentralTelemetryFrame f;
    make_golden_frame(f);
    char buf[1024];
    TEST_ASSERT_TRUE(tc_serialize_jsonl(buf, sizeof(buf), f) > 0);
    TEST_ASSERT_EQUAL_CHAR('{', buf[0]);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"central\":1"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"v\":1"));
}

void test_tc_init_sets_schema_and_defaults(void) {
    CentralTelemetryFrame f;
    tc_frame_init(f);
    TEST_ASSERT_EQUAL_UINT8(TELEMETRY_CENTRAL_SCHEMA, f.schema);
    // fsm_state nullptr → serializa como "" (sin crash), role 0 → "GK".
    char buf[1024];
    const int n = tc_serialize_jsonl(buf, sizeof(buf), f);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"role\":\"GK\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"state\":\"\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"match\":0"));
}

void test_tc_role_atk(void) {
    CentralTelemetryFrame f;
    tc_frame_init(f);
    f.fsm_role = 1;            // ATK
    f.fsm_state = "ATK_SEARCH";
    char buf[1024];
    TEST_ASSERT_TRUE(tc_serialize_jsonl(buf, sizeof(buf), f) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"role\":\"ATK\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"state\":\"ATK_SEARCH\""));
}

void test_tc_negative_fields(void) {
    CentralTelemetryFrame f;
    tc_frame_init(f);
    f.cmd_vx_mm_s = -700;
    f.pwm[1] = -149;
    f.snap_ball_x_mm = -300;
    f.otos_heading_deg = -123.45f;
    f.snap_goal_own_angle_deg = -90.00f;
    char buf[1024];
    TEST_ASSERT_TRUE(tc_serialize_jsonl(buf, sizeof(buf), f) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"vx\":-700"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"pwm\":[0,-149,0]"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"ball_x\":-300"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"hdg\":-123.45"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"goal_own_ang\":-90.00"));
}

void test_tc_buffer_too_small_returns_neg1(void) {
    CentralTelemetryFrame f;
    make_golden_frame(f);
    char small[64];
    TEST_ASSERT_EQUAL_INT(-1, tc_serialize_jsonl(small, sizeof(small), f));
}

void test_tc_null_buf_returns_neg1(void) {
    CentralTelemetryFrame f;
    tc_frame_init(f);
    TEST_ASSERT_EQUAL_INT(-1, tc_serialize_jsonl(nullptr, 1024, f));
    char buf[8];
    TEST_ASSERT_EQUAL_INT(-1, tc_serialize_jsonl(buf, 0, f));
}

// ── Parser de comandos ──
static TcCommand parse(const char* s) {
    return tc_parse_command(s, (int)strlen(s));
}

void test_tc_parse_empty_none(void) {
    // ENTER pelado / sólo separadores → NONE (NO despierta máquina; el glue lo trata
    // como un humano en el monitor crudo — pero NO debe caer en STREAM/STOP).
    TEST_ASSERT_EQUAL(TcCmd::NONE, parse("").cmd);
    TEST_ASSERT_EQUAL(TcCmd::NONE, parse("  \r\n").cmd);
    TEST_ASSERT_EQUAL(TcCmd::NONE, tc_parse_command(nullptr, 0).cmd);
}

void test_tc_parse_ping_stream(void) {
    TEST_ASSERT_EQUAL(TcCmd::PING, parse("ping").cmd);
    TEST_ASSERT_EQUAL(TcCmd::PING, parse("PING").cmd);
    TEST_ASSERT_EQUAL(TcCmd::STREAM_ON, parse("STREAM ON").cmd);
    TEST_ASSERT_EQUAL(TcCmd::STREAM_ON, parse("stream on").cmd);
    TEST_ASSERT_EQUAL(TcCmd::STREAM_OFF, parse("STREAM OFF").cmd);
    TEST_ASSERT_EQUAL(TcCmd::STREAM_OFF, parse("stream off").cmd);
    // "STREAM" sin segundo token → UNKNOWN (NO confundir con STREAM ON).
    TEST_ASSERT_EQUAL(TcCmd::UNKNOWN, parse("STREAM").cmd);
}

void test_tc_parse_unknown(void) {
    // Texto que NO es comando → UNKNOWN. CRÍTICO: la 'S' de "STREAM"/"STOP" del usuario
    // NUNCA debe parsearse como un STOP del robot — eso lo gobierna el glue, no este
    // parser; acá sólo confirmamos que palabras sueltas no matchean PING/STREAM.
    TEST_ASSERT_EQUAL(TcCmd::UNKNOWN, parse("HELLO").cmd);
    TEST_ASSERT_EQUAL(TcCmd::UNKNOWN, parse("STOP").cmd);
    TEST_ASSERT_EQUAL(TcCmd::UNKNOWN, parse("s").cmd);
    TEST_ASSERT_EQUAL(TcCmd::UNKNOWN, parse("g").cmd);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_tc_serialize_golden_exact);
    RUN_TEST(test_tc_sniff_key_and_v_present);
    RUN_TEST(test_tc_init_sets_schema_and_defaults);
    RUN_TEST(test_tc_role_atk);
    RUN_TEST(test_tc_negative_fields);
    RUN_TEST(test_tc_buffer_too_small_returns_neg1);
    RUN_TEST(test_tc_null_buf_returns_neg1);
    RUN_TEST(test_tc_parse_empty_none);
    RUN_TEST(test_tc_parse_ping_stream);
    RUN_TEST(test_tc_parse_unknown);
    return UNITY_END();
}
