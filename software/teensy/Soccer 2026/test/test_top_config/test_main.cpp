// test_top_config — tests del módulo puro top_config.{h,cpp} (A2.1).
// Corre con: bash scripts/run-host-tests.sh test_top_config
//
// Cubre: defaults no-op, round-trip serialize/deserialize, CRC16-CCITT, rechazo de
// magic/version/CRC malos y de EEPROM en blanco (0xFF), byte-layout del blob.

#include <unity.h>
#include <cstring>
#include "top_config.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

static bool tof_equal(const TofZoneConfig& a, const TofZoneConfig& b) {
    return a.enabled == b.enabled && a.mount_bearing_deg == b.mount_bearing_deg &&
           a.zone_rotation_deg == b.zone_rotation_deg && a.flip == b.flip &&
           a.zone_mask == b.zone_mask;
}
static bool cfg_equal(const TopConfig& a, const TopConfig& b) {
    if (a.cam_front_en != b.cam_front_en || a.cam_back_en != b.cam_back_en ||
        a.bno_left_en != b.bno_left_en || a.bno_right_en != b.bno_right_en ||
        a.ultrasonic_en != b.ultrasonic_en) return false;
    for (int i = 0; i < TOP_CFG_NUM_TOF; ++i)
        if (!tof_equal(a.tof[i], b.tof[i])) return false;
    return true;
}

// ============================================================================
// Formato / defaults
// ============================================================================

void test_blob_size(void) {
    TEST_ASSERT_EQUAL_INT(93, TOP_CONFIG_BLOB_SIZE);
    TEST_ASSERT_EQUAL_INT(89, TOP_CONFIG_PAYLOAD);
}

void test_defaults_no_op(void) {
    // Defaults = todo habilitado + bearings del mapeo de hoy {0,180,270,90,0,0}.
    TopConfig c;
    top_config_defaults(c);
    TEST_ASSERT_EQUAL_UINT8(1, c.cam_front_en);
    TEST_ASSERT_EQUAL_UINT8(1, c.cam_back_en);
    TEST_ASSERT_EQUAL_UINT8(1, c.bno_left_en);
    TEST_ASSERT_EQUAL_UINT8(1, c.bno_right_en);
    TEST_ASSERT_EQUAL_UINT8(1, c.ultrasonic_en);
    const int16_t expect[TOP_CFG_NUM_TOF] = {0, 180, 270, 90, 0, 0};
    for (int i = 0; i < TOP_CFG_NUM_TOF; ++i) {
        TEST_ASSERT_EQUAL_UINT8(1, c.tof[i].enabled);
        TEST_ASSERT_EQUAL_INT16(expect[i], c.tof[i].mount_bearing_deg);
        TEST_ASSERT_EQUAL_INT16(0, c.tof[i].zone_rotation_deg);
        TEST_ASSERT_EQUAL_UINT8(0, c.tof[i].flip);
        TEST_ASSERT_EQUAL_UINT64(~(uint64_t)0, c.tof[i].zone_mask);
    }
}

void test_defaults_byte_layout(void) {
    // Guarda contra cambios de layout del blob (magic/version/orden de campos).
    TopConfig c;
    top_config_defaults(c);
    uint8_t buf[TOP_CONFIG_BLOB_SIZE];
    TEST_ASSERT_EQUAL_INT(TOP_CONFIG_BLOB_SIZE, top_config_serialize(buf, sizeof(buf), c));
    TEST_ASSERT_EQUAL_HEX8(0x7C, buf[0]);          // magic
    TEST_ASSERT_EQUAL_UINT8(1, buf[1]);            // version
    TEST_ASSERT_EQUAL_UINT8(1, buf[2]);            // cam_front_en
    TEST_ASSERT_EQUAL_UINT8(1, buf[6]);            // ultrasonic_en
    TEST_ASSERT_EQUAL_UINT8(1, buf[7]);            // tof0.enabled
    TEST_ASSERT_EQUAL_UINT8(0x00, buf[8]);         // tof0.bearing LE lo (0)
    TEST_ASSERT_EQUAL_UINT8(0x00, buf[9]);         // tof0.bearing LE hi
    TEST_ASSERT_EQUAL_UINT8(0xB4, buf[7 + 14 + 1]); // tof1.bearing LE lo (180 = 0x00B4)
    TEST_ASSERT_EQUAL_UINT8(0x00, buf[7 + 14 + 2]); // tof1.bearing LE hi
    TEST_ASSERT_EQUAL_UINT8(0xFF, buf[7 + 6]);     // tof0.zone_mask byte0 (~0)
}

// ============================================================================
// Round-trip
// ============================================================================

void test_defaults_roundtrip(void) {
    TopConfig a;
    top_config_defaults(a);
    uint8_t buf[TOP_CONFIG_BLOB_SIZE];
    TEST_ASSERT_EQUAL_INT(TOP_CONFIG_BLOB_SIZE, top_config_serialize(buf, sizeof(buf), a));
    TopConfig b;
    std::memset(&b, 0xAA, sizeof(b));   // ensuciar para asegurar que deserialize llena todo
    TEST_ASSERT_TRUE(top_config_deserialize(buf, sizeof(buf), b));
    TEST_ASSERT_TRUE(cfg_equal(a, b));
}

void test_modified_config_roundtrip(void) {
    TopConfig a;
    top_config_defaults(a);
    a.cam_back_en = 0;                 // apagar la trasera (la que manda basura)
    a.bno_right_en = 0;
    a.tof[2].enabled = 0;
    a.tof[3].mount_bearing_deg = 45;   // ubicación arbitraria (futuro)
    a.tof[1].zone_rotation_deg = 90;   // campos A2.2 reservados deben round-trippear
    a.tof[1].flip = 0x03;
    a.tof[0].zone_mask = 0x00000000FFFFFFFFull;
    uint8_t buf[TOP_CONFIG_BLOB_SIZE];
    top_config_serialize(buf, sizeof(buf), a);
    TopConfig b;
    TEST_ASSERT_TRUE(top_config_deserialize(buf, sizeof(buf), b));
    TEST_ASSERT_TRUE(cfg_equal(a, b));
    TEST_ASSERT_EQUAL_UINT8(0, b.cam_back_en);
    TEST_ASSERT_EQUAL_INT16(45, b.tof[3].mount_bearing_deg);
    TEST_ASSERT_EQUAL_HEX64(0x00000000FFFFFFFFull, b.tof[0].zone_mask);
}

void test_serialize_buffer_too_small(void) {
    TopConfig c;
    top_config_defaults(c);
    uint8_t small[10];
    TEST_ASSERT_EQUAL_INT(-1, top_config_serialize(small, sizeof(small), c));
    TEST_ASSERT_EQUAL_INT(-1, top_config_serialize(nullptr, 999, c));
}

// ============================================================================
// Rechazo (magic / version / CRC / EEPROM en blanco) — sin tocar cfg
// ============================================================================

void test_reject_bad_magic(void) {
    TopConfig a; top_config_defaults(a);
    uint8_t buf[TOP_CONFIG_BLOB_SIZE];
    top_config_serialize(buf, sizeof(buf), a);
    buf[0] = 0x00;   // magic roto
    TopConfig b; top_config_defaults(b);   // preset = defaults
    TEST_ASSERT_FALSE(top_config_deserialize(buf, sizeof(buf), b));
    TEST_ASSERT_TRUE(cfg_equal(a, b));     // NO se tocó (sigue en defaults)
}

void test_reject_bad_version(void) {
    TopConfig a; top_config_defaults(a);
    uint8_t buf[TOP_CONFIG_BLOB_SIZE];
    top_config_serialize(buf, sizeof(buf), a);
    buf[1] = 99;     // version desconocida
    TopConfig b; top_config_defaults(b);
    TEST_ASSERT_FALSE(top_config_deserialize(buf, sizeof(buf), b));
}

void test_reject_bad_crc(void) {
    TopConfig a; top_config_defaults(a);
    uint8_t buf[TOP_CONFIG_BLOB_SIZE];
    top_config_serialize(buf, sizeof(buf), a);
    buf[10] ^= 0xFF;   // bit-flip en el payload
    TopConfig b; top_config_defaults(b);
    TEST_ASSERT_FALSE(top_config_deserialize(buf, sizeof(buf), b));
}

void test_reject_blank_eeprom(void) {
    // EEPROM emulada en blanco = todo 0xFF → magic (0xFF != 0x7C) falla → defaults.
    uint8_t buf[TOP_CONFIG_BLOB_SIZE];
    std::memset(buf, 0xFF, sizeof(buf));
    TopConfig b; top_config_defaults(b);
    TEST_ASSERT_FALSE(top_config_deserialize(buf, sizeof(buf), b));
    TopConfig def; top_config_defaults(def);
    TEST_ASSERT_TRUE(cfg_equal(def, b));   // queda en defaults = competencia byte-idéntica
}

void test_reject_short_buffer(void) {
    uint8_t buf[10];
    TopConfig b; top_config_defaults(b);
    TEST_ASSERT_FALSE(top_config_deserialize(buf, sizeof(buf), b));
}

// ============================================================================
// CRC16-CCITT-FALSE — valor de chequeo estándar
// ============================================================================

void test_crc16_known_value(void) {
    // "123456789" → 0x29B1 (check value de CRC-16/CCITT-FALSE).
    const uint8_t s[] = {'1','2','3','4','5','6','7','8','9'};
    TEST_ASSERT_EQUAL_HEX16(0x29B1, top_config_crc16(s, 9));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_blob_size);
    RUN_TEST(test_defaults_no_op);
    RUN_TEST(test_defaults_byte_layout);
    RUN_TEST(test_defaults_roundtrip);
    RUN_TEST(test_modified_config_roundtrip);
    RUN_TEST(test_serialize_buffer_too_small);
    RUN_TEST(test_reject_bad_magic);
    RUN_TEST(test_reject_bad_version);
    RUN_TEST(test_reject_bad_crc);
    RUN_TEST(test_reject_blank_eeprom);
    RUN_TEST(test_reject_short_buffer);
    RUN_TEST(test_crc16_known_value);
    return UNITY_END();
}
