// test_calib_storage — tests unitarios para calib_storage.{h,cpp}.
// Corre con: pio test -e test_native -f test_calib_storage

#include <unity.h>
#include <cstring>
#include "calib_storage.h"

using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// CRC
// ============================================================================

void test_cs_crc16_known_value(void) {
    // Vector de prueba conocido para CRC-16/CCITT-FALSE:
    // "123456789" → 0x29B1.
    const char* msg = "123456789";
    const uint16_t crc = cs_crc16_ccitt((const uint8_t*)msg, 9);
    TEST_ASSERT_EQUAL_UINT16(0x29B1, crc);
}

void test_cs_crc16_empty(void) {
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, cs_crc16_ccitt(nullptr, 0));
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, cs_crc16_ccitt((const uint8_t*)"", 0));
}

void test_cs_crc16_different_data_different_crc(void) {
    uint8_t a[] = {1, 2, 3, 4};
    uint8_t b[] = {1, 2, 3, 5};
    TEST_ASSERT_NOT_EQUAL(cs_crc16_ccitt(a, 4), cs_crc16_ccitt(b, 4));
}

// ============================================================================
// SERIALIZACION
// ============================================================================

void test_cs_serialize_returns_expected_size(void) {
    SensorCalib calib[32];
    for (int i = 0; i < 32; ++i) {
        calib[i].carpet = (uint16_t)(100 + i);
        calib[i].white  = (uint16_t)(800 + i);
        calib[i].threshold = 450;
    }
    uint8_t buf[CS_PAYLOAD_SIZE];
    const int written = cs_serialize(buf, CS_PAYLOAD_SIZE, calib, 32);
    TEST_ASSERT_EQUAL_INT(CS_PAYLOAD_SIZE, written);
}

void test_cs_serialize_writes_magic_and_version(void) {
    SensorCalib calib[32] = {};
    uint8_t buf[CS_PAYLOAD_SIZE] = {0};
    cs_serialize(buf, CS_PAYLOAD_SIZE, calib, 32);
    // Magic en little-endian: 0x494954A1 → bytes A1 54 49 49 → check primer byte.
    TEST_ASSERT_EQUAL_UINT8(0xA1, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x54, buf[1]);
    TEST_ASSERT_EQUAL_UINT8(0x49, buf[2]);
    TEST_ASSERT_EQUAL_UINT8(0x49, buf[3]);
    TEST_ASSERT_EQUAL_UINT8(CS_VERSION, buf[4]);
    TEST_ASSERT_EQUAL_UINT8(32, buf[5]);
}

void test_cs_serialize_null_buf_returns_error(void) {
    SensorCalib calib[32] = {};
    TEST_ASSERT_EQUAL_INT(-1, cs_serialize(nullptr, CS_PAYLOAD_SIZE, calib, 32));
}

void test_cs_serialize_null_calib_returns_error(void) {
    uint8_t buf[CS_PAYLOAD_SIZE] = {0};
    TEST_ASSERT_EQUAL_INT(-1, cs_serialize(buf, CS_PAYLOAD_SIZE, nullptr, 32));
}

void test_cs_serialize_buf_too_small_returns_error(void) {
    SensorCalib calib[32] = {};
    uint8_t buf[10];
    TEST_ASSERT_EQUAL_INT(-1, cs_serialize(buf, 10, calib, 32));
}

void test_cs_serialize_invalid_n_returns_error(void) {
    SensorCalib calib[32] = {};
    uint8_t buf[CS_PAYLOAD_SIZE];
    TEST_ASSERT_EQUAL_INT(-1, cs_serialize(buf, CS_PAYLOAD_SIZE, calib, 0));
    TEST_ASSERT_EQUAL_INT(-1, cs_serialize(buf, CS_PAYLOAD_SIZE, calib, 33));
    TEST_ASSERT_EQUAL_INT(-1, cs_serialize(buf, CS_PAYLOAD_SIZE, calib, -1));
}

// ============================================================================
// ROUNDTRIP serialize -> deserialize
// ============================================================================

void test_cs_roundtrip_preserves_values(void) {
    SensorCalib original[32];
    for (int i = 0; i < 32; ++i) {
        original[i].carpet = (uint16_t)(150 + i * 3);
        original[i].white  = (uint16_t)(820 + i * 5);
        original[i].threshold = (uint16_t)((original[i].carpet + original[i].white) / 2);
    }

    uint8_t buf[CS_PAYLOAD_SIZE];
    TEST_ASSERT_EQUAL_INT(CS_PAYLOAD_SIZE,
                          cs_serialize(buf, CS_PAYLOAD_SIZE, original, 32));

    SensorCalib loaded[32] = {};
    TEST_ASSERT_TRUE(cs_deserialize(buf, CS_PAYLOAD_SIZE, loaded, 32));

    for (int i = 0; i < 32; ++i) {
        TEST_ASSERT_EQUAL_UINT16(original[i].carpet, loaded[i].carpet);
        TEST_ASSERT_EQUAL_UINT16(original[i].white,  loaded[i].white);
        // Threshold se recalcula en deserialize como punto medio.
        const uint16_t expected_th =
            (uint16_t)((original[i].carpet + original[i].white) / 2);
        TEST_ASSERT_EQUAL_UINT16(expected_th, loaded[i].threshold);
    }
}

void test_cs_roundtrip_partial_n(void) {
    // Serializar con n_sensors=8 y deserializar con n_sensors=8 debe funcionar.
    SensorCalib original[8];
    for (int i = 0; i < 8; ++i) {
        original[i].carpet = (uint16_t)(100 + i);
        original[i].white  = (uint16_t)(900 - i);
        original[i].threshold = 500;
    }
    uint8_t buf[CS_PAYLOAD_SIZE];
    cs_serialize(buf, CS_PAYLOAD_SIZE, original, 8);

    SensorCalib loaded[8] = {};
    TEST_ASSERT_TRUE(cs_deserialize(buf, CS_PAYLOAD_SIZE, loaded, 8));
    for (int i = 0; i < 8; ++i) {
        TEST_ASSERT_EQUAL_UINT16(original[i].carpet, loaded[i].carpet);
    }
}

// ============================================================================
// VALIDACION (la deserialización rechaza buffers malos)
// ============================================================================

void test_cs_deserialize_rejects_bad_magic(void) {
    SensorCalib calib[32] = {};
    uint8_t buf[CS_PAYLOAD_SIZE];
    cs_serialize(buf, CS_PAYLOAD_SIZE, calib, 32);
    buf[0] ^= 0xFF;  // corromper magic

    SensorCalib loaded[32] = {};
    TEST_ASSERT_FALSE(cs_deserialize(buf, CS_PAYLOAD_SIZE, loaded, 32));
}

void test_cs_deserialize_rejects_bad_version(void) {
    SensorCalib calib[32] = {};
    uint8_t buf[CS_PAYLOAD_SIZE];
    cs_serialize(buf, CS_PAYLOAD_SIZE, calib, 32);
    buf[4] = 99;  // versión inválida

    SensorCalib loaded[32] = {};
    TEST_ASSERT_FALSE(cs_deserialize(buf, CS_PAYLOAD_SIZE, loaded, 32));
}

void test_cs_deserialize_rejects_n_mismatch(void) {
    SensorCalib calib[32] = {};
    uint8_t buf[CS_PAYLOAD_SIZE];
    cs_serialize(buf, CS_PAYLOAD_SIZE, calib, 32);

    SensorCalib loaded[32] = {};
    // Buffer contiene n=32, pero el llamador pide n=16 → mismatch.
    TEST_ASSERT_FALSE(cs_deserialize(buf, CS_PAYLOAD_SIZE, loaded, 16));
}

void test_cs_deserialize_rejects_bad_crc(void) {
    SensorCalib calib[32];
    for (int i = 0; i < 32; ++i) { calib[i].carpet = i; calib[i].white = 800; }
    uint8_t buf[CS_PAYLOAD_SIZE];
    cs_serialize(buf, CS_PAYLOAD_SIZE, calib, 32);

    // Corromper un byte del payload — CRC almacenado ya no coincide.
    buf[20] ^= 0x01;

    SensorCalib loaded[32] = {};
    TEST_ASSERT_FALSE(cs_deserialize(buf, CS_PAYLOAD_SIZE, loaded, 32));
}

void test_cs_deserialize_rejects_all_zeros(void) {
    // EEPROM virgen está toda en 0 → magic = 0 → debe ser rechazada.
    uint8_t buf[CS_PAYLOAD_SIZE] = {0};
    SensorCalib loaded[32] = {};
    TEST_ASSERT_FALSE(cs_deserialize(buf, CS_PAYLOAD_SIZE, loaded, 32));
}

void test_cs_deserialize_no_modify_on_failure(void) {
    SensorCalib loaded[32];
    for (int i = 0; i < 32; ++i) {
        loaded[i].carpet = 111;
        loaded[i].white  = 999;
        loaded[i].threshold = 555;
    }
    // Buffer inválido (todo cero).
    uint8_t buf[CS_PAYLOAD_SIZE] = {0};
    cs_deserialize(buf, CS_PAYLOAD_SIZE, loaded, 32);
    // calib NO se debe haber modificado.
    for (int i = 0; i < 32; ++i) {
        TEST_ASSERT_EQUAL_UINT16(111, loaded[i].carpet);
        TEST_ASSERT_EQUAL_UINT16(999, loaded[i].white);
        TEST_ASSERT_EQUAL_UINT16(555, loaded[i].threshold);
    }
}

void test_cs_deserialize_null_buf_returns_false(void) {
    SensorCalib loaded[32] = {};
    TEST_ASSERT_FALSE(cs_deserialize(nullptr, CS_PAYLOAD_SIZE, loaded, 32));
}

void test_cs_deserialize_null_calib_returns_false(void) {
    uint8_t buf[CS_PAYLOAD_SIZE] = {0};
    TEST_ASSERT_FALSE(cs_deserialize(buf, CS_PAYLOAD_SIZE, nullptr, 32));
}

// ============================================================================
// MAIN
// ============================================================================

int main(int, char**) {
    UNITY_BEGIN();
    // CRC
    RUN_TEST(test_cs_crc16_known_value);
    RUN_TEST(test_cs_crc16_empty);
    RUN_TEST(test_cs_crc16_different_data_different_crc);
    // Serialización
    RUN_TEST(test_cs_serialize_returns_expected_size);
    RUN_TEST(test_cs_serialize_writes_magic_and_version);
    RUN_TEST(test_cs_serialize_null_buf_returns_error);
    RUN_TEST(test_cs_serialize_null_calib_returns_error);
    RUN_TEST(test_cs_serialize_buf_too_small_returns_error);
    RUN_TEST(test_cs_serialize_invalid_n_returns_error);
    // Roundtrip
    RUN_TEST(test_cs_roundtrip_preserves_values);
    RUN_TEST(test_cs_roundtrip_partial_n);
    // Validación
    RUN_TEST(test_cs_deserialize_rejects_bad_magic);
    RUN_TEST(test_cs_deserialize_rejects_bad_version);
    RUN_TEST(test_cs_deserialize_rejects_n_mismatch);
    RUN_TEST(test_cs_deserialize_rejects_bad_crc);
    RUN_TEST(test_cs_deserialize_rejects_all_zeros);
    RUN_TEST(test_cs_deserialize_no_modify_on_failure);
    RUN_TEST(test_cs_deserialize_null_buf_returns_false);
    RUN_TEST(test_cs_deserialize_null_calib_returns_false);
    return UNITY_END();
}
