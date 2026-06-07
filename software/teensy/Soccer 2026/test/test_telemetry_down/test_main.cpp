// test_telemetry_down — tests del módulo puro telemetry_down.{h,cpp}.
// Corre con: bash scripts/run-host-tests.sh test_telemetry_down
//            (o pio test -e test_native -f test_telemetry_down con red)
//
// Cubre: serialización JSONL (golden exacto byte-a-byte + sentinels N/A +
// overflow de buffer) y parser de comandos host→firmware.
//
// El GOLDEN LINE de abajo es EXACTAMENTE lo que emite el serializador para el
// frame conocido. El MISMO string se commitea como fixture de la app de PC en
// tools/monitor-base/tests/golden_frame_v1.jsonl → contrato cross-lenguaje
// (firmware C++ emite ⇄ Python parsea). Si cambia el schema, regenerar AMBOS.

#include <unity.h>
#include <cstring>
#include "telemetry_down.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Arma el frame canónico usado para el golden (idéntico al arnés que generó el
// fixture de Python).
static void make_golden_frame(TelemetryDownFrame& f) {
    td_frame_init(f, 32);
    f.seq = 7; f.t_ms = 123456;
    for (int i = 0; i < 32; ++i) {
        f.raw[i]       = (uint16_t)(100 + i * 10);
        f.carpet[i]    = (uint16_t)(150 + i);
        f.white_cal[i] = (uint16_t)(800 + i);
    }
    f.white_bits = 0x00030003u;  // sensores 0,1,16,17 en blanco
    f.data_valid = 1; f.line_present = 1;
    f.line_angle_cd = -1250; f.escape_angle_cd = 16750;
    f.penetration_mm = 42; f.cross_track_mm = -8;
    f.sensors_on_line = 5; f.event_flags = 0x01;
    f.quality = 88; f.sample_age_ms = 3;
    f.otos_count = 2; f.otos_left_ok = 1; f.otos_right_ok = 0;
    f.otos_x_mm = 123.45f; f.otos_y_mm = -67.80f; f.otos_heading_deg = 12.34f;
    f.otos_vx_mm_s = 5.0f; f.otos_vy_mm_s = -3.20f;
    f.otos_omega_rad_s = 0.123f; f.otos_slip_mm_s = 1.50f;
    f.otos_left_x_mm = 120.10f; f.otos_left_y_mm = -65.20f; f.otos_left_heading_deg = 11.50f;
    f.otos_right_x_mm = 126.80f; f.otos_right_y_mm = -70.40f; f.otos_right_heading_deg = 13.18f;
    f.lifted = 0; f.line_tick_count = 100000; f.line_tick_us = 250;
}

static const char* GOLDEN =
    "{\"v\":2,\"seq\":7,\"t_ms\":123456,\"ring\":{\"n\":32,\"raw\":"
    "[100,110,120,130,140,150,160,170,180,190,200,210,220,230,240,250,260,270,"
    "280,290,300,310,320,330,340,350,360,370,380,390,400,410],\"white\":196611,"
    "\"carpet\":[150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,"
    "166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181],"
    "\"white_cal\":[800,801,802,803,804,805,806,807,808,809,810,811,812,813,814,"
    "815,816,817,818,819,820,821,822,823,824,825,826,827,828,829,830,831]},"
    "\"line\":{\"schema\":2,\"valid\":1,\"present\":1,\"angle_cd\":-1250,"
    "\"escape_cd\":16750,\"pen_mm\":42,\"xtrack_mm\":-8,\"non\":5,\"flags\":1,"
    "\"q\":88,\"age_ms\":3},\"otos\":{\"n\":2,\"lok\":1,\"rok\":0,\"x\":123.45,"
    "\"y\":-67.80,\"hdg\":12.34,\"vx\":5.00,\"vy\":-3.20,\"w\":0.123,"
    "\"slip\":1.50,\"lx\":120.10,\"ly\":-65.20,\"lh\":11.50,\"rx\":126.80,"
    "\"ry\":-70.40,\"rh\":13.18},\"diag\":{\"lifted\":0,\"ltick\":100000,"
    "\"ltick_us\":250}}\n";

// ============================================================================
// SERIALIZACIÓN — golden exacto
// ============================================================================

void test_td_serialize_golden_exact(void) {
    TelemetryDownFrame f;
    make_golden_frame(f);
    char buf[2048];
    const int n = td_serialize_jsonl(buf, sizeof(buf), f);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_INT((int)strlen(buf), n);         // length == bytes escritos
    TEST_ASSERT_EQUAL_STRING(GOLDEN, buf);              // byte-a-byte
    TEST_ASSERT_EQUAL_CHAR('\n', buf[n - 1]);           // termina en newline
}

void test_td_serialize_na_sentinels(void) {
    // Frame recién init (sin datos de línea) → debe emitir los sentinels N/A.
    TelemetryDownFrame f;
    td_frame_init(f, 32);
    char buf[2048];
    const int n = td_serialize_jsonl(buf, sizeof(buf), f);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"angle_cd\":-32768"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"escape_cd\":-32768"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"xtrack_mm\":-32768"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"pen_mm\":65535"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"v\":2"));
}

void test_td_serialize_respects_num_sensors(void) {
    // num_sensors = 8 → el array raw tiene 8 elementos y "n":8.
    TelemetryDownFrame f;
    td_frame_init(f, 8);
    for (int i = 0; i < 8; ++i) f.raw[i] = (uint16_t)(10 + i);
    char buf[2048];
    const int n = td_serialize_jsonl(buf, sizeof(buf), f);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"n\":8"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"raw\":[10,11,12,13,14,15,16,17]"));
}

void test_td_serialize_clamps_num_sensors(void) {
    // num_sensors > 32 se clampa a 32 en td_frame_init.
    TelemetryDownFrame f;
    td_frame_init(f, 99);
    TEST_ASSERT_EQUAL_UINT8(32, f.num_sensors);
}

void test_td_serialize_buffer_too_small_returns_neg1(void) {
    TelemetryDownFrame f;
    make_golden_frame(f);
    char small[64];
    TEST_ASSERT_EQUAL_INT(-1, td_serialize_jsonl(small, sizeof(small), f));
}

void test_td_serialize_null_buf_returns_neg1(void) {
    TelemetryDownFrame f;
    td_frame_init(f, 32);
    TEST_ASSERT_EQUAL_INT(-1, td_serialize_jsonl(nullptr, 2048, f));
    char buf[8];
    TEST_ASSERT_EQUAL_INT(-1, td_serialize_jsonl(buf, 0, f));
}

void test_td_serialize_white_bits_full(void) {
    TelemetryDownFrame f;
    td_frame_init(f, 32);
    f.white_bits = 0xFFFFFFFFu;  // 4294967295
    char buf[2048];
    TEST_ASSERT_TRUE(td_serialize_jsonl(buf, sizeof(buf), f) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"white\":4294967295"));
}

void test_td_frame_init_sets_schema(void) {
    TelemetryDownFrame f;
    td_frame_init(f, 32);
    TEST_ASSERT_EQUAL_UINT8(TELEMETRY_DOWN_SCHEMA, f.schema);
    TEST_ASSERT_EQUAL_UINT8(32, f.num_sensors);
    TEST_ASSERT_EQUAL_INT16(TD_NA_I16, f.line_angle_cd);
    TEST_ASSERT_EQUAL_UINT16(TD_NA_U16, f.penetration_mm);
}

// ============================================================================
// PARSER DE COMANDOS
// ============================================================================

static TdCommand parse(const char* s) {
    return td_parse_command(s, (int)strlen(s));
}

void test_td_parse_empty_is_none(void) {
    TEST_ASSERT_EQUAL(TdCmd::NONE, parse("").cmd);
    TEST_ASSERT_EQUAL(TdCmd::NONE, parse("   ").cmd);
    TEST_ASSERT_EQUAL(TdCmd::NONE, parse("\r\n").cmd);
    TEST_ASSERT_EQUAL(TdCmd::NONE, td_parse_command(nullptr, 0).cmd);
}

void test_td_parse_ping(void) {
    TEST_ASSERT_EQUAL(TdCmd::PING, parse("PING").cmd);
    TEST_ASSERT_EQUAL(TdCmd::PING, parse("ping").cmd);       // case-insensitive
    TEST_ASSERT_EQUAL(TdCmd::PING, parse("  Ping  \r\n").cmd);
}

void test_td_parse_stream(void) {
    TEST_ASSERT_EQUAL(TdCmd::STREAM_ON,  parse("STREAM ON").cmd);
    TEST_ASSERT_EQUAL(TdCmd::STREAM_OFF, parse("stream off").cmd);
    TEST_ASSERT_EQUAL(TdCmd::UNKNOWN,    parse("STREAM").cmd);     // falta arg
    TEST_ASSERT_EQUAL(TdCmd::UNKNOWN,    parse("STREAM MAYBE").cmd);
}

void test_td_parse_rate(void) {
    TdCommand c = parse("RATE 20");
    TEST_ASSERT_EQUAL(TdCmd::SET_RATE, c.cmd);
    TEST_ASSERT_EQUAL_INT32(20, c.arg);
    c = parse("rate 50");
    TEST_ASSERT_EQUAL(TdCmd::SET_RATE, c.cmd);
    TEST_ASSERT_EQUAL_INT32(50, c.arg);
    TEST_ASSERT_EQUAL(TdCmd::UNKNOWN, parse("RATE").cmd);          // falta número
    TEST_ASSERT_EQUAL(TdCmd::UNKNOWN, parse("RATE abc").cmd);      // no numérico
    TEST_ASSERT_EQUAL(TdCmd::UNKNOWN, parse("RATE 0").cmd);        // hz<=0
    TEST_ASSERT_EQUAL(TdCmd::UNKNOWN, parse("RATE -5").cmd);
}

void test_td_parse_calibration(void) {
    TEST_ASSERT_EQUAL(TdCmd::CAL_CARPET,   parse("CAL CARPET").cmd);
    TEST_ASSERT_EQUAL(TdCmd::CAL_WHITE,    parse("cal white").cmd);
    TEST_ASSERT_EQUAL(TdCmd::CAL_SAVE,     parse("CAL SAVE").cmd);
    TEST_ASSERT_EQUAL(TdCmd::CAL_LOAD,     parse("CAL LOAD").cmd);
    TEST_ASSERT_EQUAL(TdCmd::CAL_AUTO_ON,  parse("CAL AUTO ON").cmd);
    TEST_ASSERT_EQUAL(TdCmd::CAL_AUTO_OFF, parse("cal auto off").cmd);
    TEST_ASSERT_EQUAL(TdCmd::UNKNOWN,      parse("CAL").cmd);
    TEST_ASSERT_EQUAL(TdCmd::UNKNOWN,      parse("CAL NONSENSE").cmd);
    TEST_ASSERT_EQUAL(TdCmd::UNKNOWN,      parse("CAL AUTO").cmd);
}

void test_td_parse_otos_reset(void) {
    TEST_ASSERT_EQUAL(TdCmd::OTOS_RESET, parse("OTOS RESET").cmd);
    TEST_ASSERT_EQUAL(TdCmd::OTOS_RESET, parse("otos reset").cmd);
    TEST_ASSERT_EQUAL(TdCmd::UNKNOWN,    parse("OTOS").cmd);
    TEST_ASSERT_EQUAL(TdCmd::UNKNOWN,    parse("OTOS GO").cmd);
}

void test_td_parse_unknown(void) {
    TEST_ASSERT_EQUAL(TdCmd::UNKNOWN, parse("HELLO").cmd);
    TEST_ASSERT_EQUAL(TdCmd::UNKNOWN, parse("FOO BAR BAZ").cmd);
}

void test_td_parse_extra_whitespace_and_commas(void) {
    TEST_ASSERT_EQUAL(TdCmd::STREAM_ON, parse("  STREAM   ON  ").cmd);
    TEST_ASSERT_EQUAL(TdCmd::CAL_CARPET, parse("CAL,CARPET").cmd);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();
    // Serialización
    RUN_TEST(test_td_serialize_golden_exact);
    RUN_TEST(test_td_serialize_na_sentinels);
    RUN_TEST(test_td_serialize_respects_num_sensors);
    RUN_TEST(test_td_serialize_clamps_num_sensors);
    RUN_TEST(test_td_serialize_buffer_too_small_returns_neg1);
    RUN_TEST(test_td_serialize_null_buf_returns_neg1);
    RUN_TEST(test_td_serialize_white_bits_full);
    RUN_TEST(test_td_frame_init_sets_schema);
    // Parser de comandos
    RUN_TEST(test_td_parse_empty_is_none);
    RUN_TEST(test_td_parse_ping);
    RUN_TEST(test_td_parse_stream);
    RUN_TEST(test_td_parse_rate);
    RUN_TEST(test_td_parse_calibration);
    RUN_TEST(test_td_parse_otos_reset);
    RUN_TEST(test_td_parse_unknown);
    RUN_TEST(test_td_parse_extra_whitespace_and_commas);
    return UNITY_END();
}
