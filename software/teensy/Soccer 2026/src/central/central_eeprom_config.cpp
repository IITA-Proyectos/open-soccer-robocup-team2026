// central_eeprom_config.cpp — glue Arduino. Ver central_eeprom_config.h.
//
// Compila SOLO con -DCENTRAL_EEPROM_CALIB y en target Arduino (<EEPROM.h>). Sin el flag
// el TU queda vacío → 0 bytes al binario. Espejo de top_eeprom_config.cpp.

#include "central_eeprom_config.h"

#ifdef CENTRAL_EEPROM_CALIB

#include "config_central.h"   // MOTOR_MIN_PWM / MOTOR_EFF_X100 (defaults POR ROBOT)
#include <Arduino.h>
#include <EEPROM.h>

namespace iitasoccer {

CentralConfig g_central_cfg;
int g_cfg_min_pwm[3] = { 0, 0, 0 };
int g_cfg_eff[3]     = { 100, 100, 100 };

namespace {

// La CENTRAL nunca usó EEPROM hasta ahora → offset 0 está libre (Teensy 4.1). El blob
// son 26 bytes. Si en el futuro se agrega otra región, documentarla en EEPROM-MAP.md.
constexpr int CENTRAL_CONFIG_EEPROM_OFFSET = 0;

void read_buf(uint8_t* buf, int offset, int len) {
    for (int i = 0; i < len; ++i) buf[i] = EEPROM.read(offset + i);
}

bool write_buf(const uint8_t* buf, int offset, int len) {
    if (offset + len > (int)EEPROM.length()) return false;
    for (int i = 0; i < len; ++i) EEPROM.update(offset + i, buf[i]);  // solo escribe si cambió
    return true;
}

// Copia los campos que CONSUMEN los motores hoy (min_pwm/eff) a las variables efectivas.
// fwd_pwm/gyro quedan en g_central_cfg pero todavía NO se aplican (su cableado al mixer/PID
// es un incremento posterior; se guardan para no perder la calibración).
void apply_to_runtime() {
    for (int i = 0; i < 3; ++i) {
        g_cfg_min_pwm[i] = g_central_cfg.min_pwm[i];
        g_cfg_eff[i]     = g_central_cfg.eff[i];
    }
}

}  // namespace

void central_eeprom_init() {
    // 1) Defaults = los constexpr POR ROBOT (config_central.h). Si la EEPROM falla, el
    //    robot se comporta EXACTAMENTE como hoy.
    for (int i = 0; i < 3; ++i) {
        g_central_cfg.min_pwm[i] = MOTOR_MIN_PWM[i];
        g_central_cfg.eff[i]     = MOTOR_EFF_X100[i];
    }
    // fwd_pwm/gyro: 0 = "sin override" (el cableado futuro usa el valor del código si 0).
    g_central_cfg.fwd_pwm_l = 0;
    g_central_cfg.fwd_pwm_r = 0;
    g_central_cfg.gyro_kp   = 0;
    g_central_cfg.gyro_ki   = 0;
    g_central_cfg.gyro_kd   = 0;

    // 2) Override de EEPROM si el blob es válido (magic+version+CRC). Si no, g_central_cfg
    //    queda en los defaults de arriba (deserialize no toca out cuando falla).
    uint8_t buf[CENTRAL_CONFIG_BLOB_SIZE];
    read_buf(buf, CENTRAL_CONFIG_EEPROM_OFFSET, CENTRAL_CONFIG_BLOB_SIZE);
    central_config_deserialize(buf, CENTRAL_CONFIG_BLOB_SIZE, g_central_cfg);

    apply_to_runtime();
}

void central_eeprom_apply_set(CcParam param, int idx, int value) {
    switch (param) {
        case CcParam::MINPWM:
            if (idx >= 0 && idx < 3) { g_central_cfg.min_pwm[idx] = (int16_t)value; g_cfg_min_pwm[idx] = value; }
            break;
        case CcParam::EFF:
            if (idx >= 0 && idx < 3) { g_central_cfg.eff[idx] = (int16_t)value; g_cfg_eff[idx] = value; }
            break;
        case CcParam::FWDL: g_central_cfg.fwd_pwm_l = (int16_t)value; break;
        case CcParam::FWDR: g_central_cfg.fwd_pwm_r = (int16_t)value; break;
        case CcParam::GKP:  g_central_cfg.gyro_kp   = (int16_t)value; break;
        case CcParam::GKI:  g_central_cfg.gyro_ki   = (int16_t)value; break;
        case CcParam::GKD:  g_central_cfg.gyro_kd   = (int16_t)value; break;
        default: break;
    }
}

bool central_eeprom_save() {
    uint8_t buf[CENTRAL_CONFIG_BLOB_SIZE];
    if (central_config_serialize(buf, sizeof(buf), g_central_cfg) < 0) return false;
    return write_buf(buf, CENTRAL_CONFIG_EEPROM_OFFSET, CENTRAL_CONFIG_BLOB_SIZE);
}

}  // namespace iitasoccer

#endif  // CENTRAL_EEPROM_CALIB
