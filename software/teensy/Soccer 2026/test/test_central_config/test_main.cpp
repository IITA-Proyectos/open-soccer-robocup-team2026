// test_central_config — tests del módulo PURO central_config.{h,cpp}.
// Calibración persistente de la CENTRAL (sin reflashear): potencias de rueda
// (min_pwm[3] + eff[3]), avance recto (fwd_pwm_l/r) y PID de giroscopio (gyro_kp/ki/kd).
// Blob con CRC (patrón top_config) + parser de comandos SET/GET/SAVE.
// Corre con: bash scripts/run-host-tests.sh test_central_config

#include <unity.h>
#include <cstring>
#include "central_config.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

static void fill(CentralConfig& c) {
    c.min_pwm[0] = 72; c.min_pwm[1] = 68; c.min_pwm[2] = 110;
    c.eff[0] = 98;     c.eff[1] = 102;    c.eff[2] = 131;
    c.fwd_pwm_l = 120; c.fwd_pwm_r = 118;
    c.gyro_kp = 1500;  c.gyro_ki = 10;    c.gyro_kd = 300;
}

// ── Serialización / blob ──────────────────────────────────────────────────────

void test_serialize_size_is_blob_size(void) {
    CentralConfig c{}; fill(c);
    uint8_t buf[64];
    TEST_ASSERT_EQUAL_INT(CENTRAL_CONFIG_BLOB_SIZE, central_config_serialize(buf, sizeof(buf), c));
}

void test_serialize_roundtrip_all_fields(void) {
    CentralConfig c{}; fill(c);
    uint8_t buf[64];
    const int n = central_config_serialize(buf, sizeof(buf), c);
    TEST_ASSERT_TRUE(n > 0);
    CentralConfig o{};
    TEST_ASSERT_TRUE(central_config_deserialize(buf, n, o));
    TEST_ASSERT_EQUAL_INT16(72, o.min_pwm[0]);
    TEST_ASSERT_EQUAL_INT16(68, o.min_pwm[1]);
    TEST_ASSERT_EQUAL_INT16(110, o.min_pwm[2]);
    TEST_ASSERT_EQUAL_INT16(98, o.eff[0]);
    TEST_ASSERT_EQUAL_INT16(102, o.eff[1]);
    TEST_ASSERT_EQUAL_INT16(131, o.eff[2]);
    TEST_ASSERT_EQUAL_INT16(120, o.fwd_pwm_l);
    TEST_ASSERT_EQUAL_INT16(118, o.fwd_pwm_r);
    TEST_ASSERT_EQUAL_INT16(1500, o.gyro_kp);
    TEST_ASSERT_EQUAL_INT16(10, o.gyro_ki);
    TEST_ASSERT_EQUAL_INT16(300, o.gyro_kd);
}

void test_serialize_buf_too_small_returns_neg1(void) {
    CentralConfig c{}; uint8_t small[4];
    TEST_ASSERT_EQUAL_INT(-1, central_config_serialize(small, sizeof(small), c));
    TEST_ASSERT_EQUAL_INT(-1, central_config_serialize(nullptr, 64, c));
}

void test_deserialize_blank_eeprom_fails(void) {
    uint8_t blank[CENTRAL_CONFIG_BLOB_SIZE];
    memset(blank, 0xFF, sizeof(blank));
    CentralConfig o{};
    TEST_ASSERT_FALSE(central_config_deserialize(blank, sizeof(blank), o));
}

void test_deserialize_bad_magic_fails(void) {
    CentralConfig c{}; fill(c); uint8_t buf[64];
    const int n = central_config_serialize(buf, sizeof(buf), c);
    buf[0] ^= 0xFF;
    CentralConfig o{};
    TEST_ASSERT_FALSE(central_config_deserialize(buf, n, o));
}

void test_deserialize_bad_version_fails(void) {
    CentralConfig c{}; fill(c); uint8_t buf[64];
    const int n = central_config_serialize(buf, sizeof(buf), c);
    buf[1] = 99;
    CentralConfig o{};
    TEST_ASSERT_FALSE(central_config_deserialize(buf, n, o));
}

void test_deserialize_corrupt_payload_fails(void) {
    CentralConfig c{}; fill(c); uint8_t buf[64];
    const int n = central_config_serialize(buf, sizeof(buf), c);
    buf[5] ^= 0x01;
    CentralConfig o{};
    TEST_ASSERT_FALSE(central_config_deserialize(buf, n, o));
}

// ── Clamps de seguridad ───────────────────────────────────────────────────────

void test_clamp_pwm_bounds(void) {
    // Piso/avance: NUNCA > 149 (motores 5V a 7.4V se queman > ~70%); nunca < 0.
    TEST_ASSERT_EQUAL_INT16(0, central_config_clamp_pwm(-5));
    TEST_ASSERT_EQUAL_INT16(90, central_config_clamp_pwm(90));
    TEST_ASSERT_EQUAL_INT16(149, central_config_clamp_pwm(149));
    TEST_ASSERT_EQUAL_INT16(149, central_config_clamp_pwm(255));
}

void test_clamp_eff_bounds(void) {
    TEST_ASSERT_EQUAL_INT16(50, central_config_clamp_eff(10));
    TEST_ASSERT_EQUAL_INT16(131, central_config_clamp_eff(131));
    TEST_ASSERT_EQUAL_INT16(200, central_config_clamp_eff(999));
}

void test_clamp_gain_bounds(void) {
    // Ganancia PID ×1000: no negativa, acotada a 32000 (cabe en int16).
    TEST_ASSERT_EQUAL_INT16(0, central_config_clamp_gain(-5));
    TEST_ASSERT_EQUAL_INT16(1500, central_config_clamp_gain(1500));
    TEST_ASSERT_EQUAL_INT16(32000, central_config_clamp_gain(99999));
}

// ── Parser de comandos ────────────────────────────────────────────────────────

static CcCommand parse(const char* s) { return cc_parse_command(s, (int)strlen(s)); }

void test_parse_set_minpwm(void) {
    CcCommand c = parse("SET MINPWM 0 90");
    TEST_ASSERT_TRUE(c.valid);
    TEST_ASSERT_EQUAL(CcCmd::SET, c.cmd);
    TEST_ASSERT_EQUAL(CcParam::MINPWM, c.param);
    TEST_ASSERT_EQUAL_INT(0, c.idx);
    TEST_ASSERT_EQUAL_INT(90, c.value);
}

void test_parse_set_eff_lowercase(void) {
    CcCommand c = parse("set eff 2 115");
    TEST_ASSERT_TRUE(c.valid);
    TEST_ASSERT_EQUAL(CcParam::EFF, c.param);
    TEST_ASSERT_EQUAL_INT(2, c.idx);
    TEST_ASSERT_EQUAL_INT(115, c.value);
}

void test_parse_set_indexed_clamps_and_bad_idx(void) {
    TEST_ASSERT_EQUAL_INT(149, parse("SET MINPWM 1 999").value);   // clamp pwm
    TEST_ASSERT_EQUAL_INT(50,  parse("SET EFF 0 10").value);       // clamp eff
    TEST_ASSERT_FALSE(parse("SET MINPWM 3 90").valid);             // idx fuera [0,2]
    TEST_ASSERT_FALSE(parse("SET EFF -1 100").valid);
}

void test_parse_set_fwd_scalars(void) {
    CcCommand l = parse("SET FWDL 120");
    TEST_ASSERT_TRUE(l.valid);
    TEST_ASSERT_EQUAL(CcParam::FWDL, l.param);
    TEST_ASSERT_EQUAL_INT(-1, l.idx);
    TEST_ASSERT_EQUAL_INT(120, l.value);
    CcCommand r = parse("SET FWDR 999");
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL(CcParam::FWDR, r.param);
    TEST_ASSERT_EQUAL_INT(149, r.value);          // clamp pwm
}

void test_parse_set_gyro_pid(void) {
    CcCommand kp = parse("SET GKP 1500");
    TEST_ASSERT_TRUE(kp.valid);
    TEST_ASSERT_EQUAL(CcParam::GKP, kp.param);
    TEST_ASSERT_EQUAL_INT(1500, kp.value);
    TEST_ASSERT_EQUAL(CcParam::GKI, parse("SET GKI 10").param);
    TEST_ASSERT_EQUAL(CcParam::GKD, parse("SET GKD 300").param);
    TEST_ASSERT_EQUAL_INT(0, parse("SET GKP -5").value);   // clamp gain ≥ 0
}

void test_parse_scalar_needs_no_index(void) {
    // Un escalar con un token de más es inválido (evita ambigüedad).
    TEST_ASSERT_FALSE(parse("SET FWDL 0 120").valid);
    TEST_ASSERT_FALSE(parse("SET GKP").valid);             // falta el valor
}

void test_parse_set_bad_param_invalid(void) {
    TEST_ASSERT_FALSE(parse("SET KICK 0 90").valid);
    TEST_ASSERT_FALSE(parse("SET MINPWM 0").valid);        // indexed sin valor
}

void test_parse_get_save(void) {
    TEST_ASSERT_EQUAL(CcCmd::GET, parse("GET").cmd);
    TEST_ASSERT_TRUE(parse("GET").valid);
    TEST_ASSERT_EQUAL(CcCmd::SAVE, parse("save").cmd);
    TEST_ASSERT_TRUE(parse("save").valid);
}

void test_parse_unknown_is_none(void) {
    TEST_ASSERT_EQUAL(CcCmd::NONE, parse("HELLO").cmd);
    TEST_ASSERT_FALSE(parse("HELLO").valid);
    TEST_ASSERT_EQUAL(CcCmd::NONE, parse("").cmd);
    TEST_ASSERT_FALSE(parse("PING").valid);
    TEST_ASSERT_FALSE(parse("STREAM ON").valid);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_serialize_size_is_blob_size);
    RUN_TEST(test_serialize_roundtrip_all_fields);
    RUN_TEST(test_serialize_buf_too_small_returns_neg1);
    RUN_TEST(test_deserialize_blank_eeprom_fails);
    RUN_TEST(test_deserialize_bad_magic_fails);
    RUN_TEST(test_deserialize_bad_version_fails);
    RUN_TEST(test_deserialize_corrupt_payload_fails);
    RUN_TEST(test_clamp_pwm_bounds);
    RUN_TEST(test_clamp_eff_bounds);
    RUN_TEST(test_clamp_gain_bounds);
    RUN_TEST(test_parse_set_minpwm);
    RUN_TEST(test_parse_set_eff_lowercase);
    RUN_TEST(test_parse_set_indexed_clamps_and_bad_idx);
    RUN_TEST(test_parse_set_fwd_scalars);
    RUN_TEST(test_parse_set_gyro_pid);
    RUN_TEST(test_parse_scalar_needs_no_index);
    RUN_TEST(test_parse_set_bad_param_invalid);
    RUN_TEST(test_parse_get_save);
    RUN_TEST(test_parse_unknown_is_none);
    return UNITY_END();
}
