// eeprom_calib.cpp — implementación del glue Arduino para EEPROM.
//
// Compila SOLO en target Arduino (donde está disponible <EEPROM.h>). En
// host-native, este .cpp queda fuera del build (build_src_filter del env
// `down` incluye este; el env `test_native` solo toma `shared/`).

#include "eeprom_calib.h"
#include "calib_storage.h"
#include <Arduino.h>
#include <EEPROM.h>

namespace iitasoccer {

namespace {

void read_buf_from_eeprom(uint8_t* buf, int offset, int len) {
    for (int i = 0; i < len; ++i) {
        buf[i] = EEPROM.read(offset + i);
    }
}

bool write_buf_to_eeprom(const uint8_t* buf, int offset, int len) {
    if (offset + len > (int)EEPROM.length()) return false;
    for (int i = 0; i < len; ++i) {
        // EEPROM.update solo escribe si el byte CAMBIÓ → minimiza desgaste
        // (write lifetime ~100k por celda en EEPROM emulada Teensy 4.0).
        EEPROM.update(offset + i, buf[i]);
    }
    return true;
}

}  // namespace

bool ec_load_calibration(SensorCalib* calib, int n_sensors,
                         int8_t* global_sens_out) {
    if (calib == nullptr || n_sensors <= 0 || n_sensors > CS_MAX_SENSORS) {
        return false;
    }
    uint8_t buf[CS_PAYLOAD_SIZE];
    read_buf_from_eeprom(buf, EC_EEPROM_OFFSET, CS_PAYLOAD_SIZE);
    return cs_deserialize(buf, CS_PAYLOAD_SIZE, calib, n_sensors, global_sens_out);
}

bool ec_save_calibration(const SensorCalib* calib, int n_sensors,
                         int8_t global_sens) {
    if (calib == nullptr || n_sensors <= 0 || n_sensors > CS_MAX_SENSORS) {
        return false;
    }
    uint8_t buf[CS_PAYLOAD_SIZE];
    const int written = cs_serialize(buf, CS_PAYLOAD_SIZE, calib, n_sensors,
                                     global_sens);
    if (written < 0) return false;
    return write_buf_to_eeprom(buf, EC_EEPROM_OFFSET, CS_PAYLOAD_SIZE);
}

void ec_erase_calibration() {
    for (int i = 0; i < CS_PAYLOAD_SIZE; ++i) {
        EEPROM.update(EC_EEPROM_OFFSET + i, 0);
    }
}

}  // namespace iitasoccer
