// top_eeprom_config.cpp — glue Arduino. Ver top_eeprom_config.h.
//
// Compila SOLO en target Arduino (<EEPROM.h>). En host-native queda fuera del
// build (el env test_native solo toma src/shared/). Espejo de down/eeprom_calib.cpp.

#include "top_eeprom_config.h"
#include "top_config.h"
#include <Arduino.h>
#include <EEPROM.h>

namespace iitasoccer {

// Config viva (definición del extern de top_eeprom_config.h). Se puebla en
// top_config_load(); arranca en defaults por las dudas (static-init la pone en
// ceros = todo DESHABILITADO, así que main_top DEBE llamar load antes de leerla).
TopConfig g_top_cfg;

namespace {

void read_buf(uint8_t* buf, int offset, int len) {
    for (int i = 0; i < len; ++i) buf[i] = EEPROM.read(offset + i);
}

bool write_buf(const uint8_t* buf, int offset, int len) {
    if (offset + len > (int)EEPROM.length()) return false;
    for (int i = 0; i < len; ++i) EEPROM.update(offset + i, buf[i]);  // solo si cambió
    return true;
}

}  // namespace

bool top_config_load(TopConfig* out) {
    if (out == nullptr) return false;
    top_config_defaults(*out);   // base: si la EEPROM falla, quedan defaults (no-op)
    uint8_t buf[TOP_CONFIG_BLOB_SIZE];
    read_buf(buf, TOP_CONFIG_EEPROM_OFFSET, TOP_CONFIG_BLOB_SIZE);
    // deserialize sobreescribe *out solo si el blob es válido; si no, *out queda
    // en los defaults que acabamos de poner.
    return top_config_deserialize(buf, TOP_CONFIG_BLOB_SIZE, *out);
}

bool top_config_save(const TopConfig& cfg) {
    uint8_t buf[TOP_CONFIG_BLOB_SIZE];
    if (top_config_serialize(buf, sizeof(buf), cfg) < 0) return false;
    return write_buf(buf, TOP_CONFIG_EEPROM_OFFSET, TOP_CONFIG_BLOB_SIZE);
}

void top_config_erase() {
    for (int i = 0; i < TOP_CONFIG_BLOB_SIZE; ++i) {
        EEPROM.update(TOP_CONFIG_EEPROM_OFFSET + i, 0);
    }
}

}  // namespace iitasoccer
