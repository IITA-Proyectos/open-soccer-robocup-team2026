// top_config.cpp — implementación. Ver top_config.h para el contrato.
// Espejo de calib_storage.cpp (mismo CRC16-CCITT + serialización LE byte a byte).

#include "top_config.h"
#include <cstring>

namespace iitasoccer {

uint16_t top_config_crc16(const uint8_t* data, int len) {
    if (data == nullptr || len <= 0) return 0xFFFF;
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; ++i) {
        crc ^= ((uint16_t)data[i]) << 8;
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000) crc = (uint16_t)((crc << 1) ^ 0x1021);
            else              crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

namespace {

inline void write_u16_le(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}
inline void write_u64_le(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = (uint8_t)((v >> (8 * i)) & 0xFF);
}
inline uint16_t read_u16_le(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0]) | (((uint16_t)p[1]) << 8));
}
inline uint64_t read_u64_le(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= ((uint64_t)p[i]) << (8 * i);
    return v;
}

// Bearings default = el mapeo HARDCODEADO de hoy (pinout_common.h
// TOF_MOUNT_ANGLE_DEG {0,180,270,90}); los 2 ToF futuros (móviles) en 0.
constexpr int16_t kDefaultBearing[TOP_CFG_NUM_TOF] = { 0, 180, 270, 90, 0, 0 };

}  // namespace

void top_config_defaults(TopConfig& cfg) {
    cfg.cam_front_en  = 1;
    cfg.cam_back_en   = 1;
    cfg.bno_left_en   = 1;
    cfg.bno_right_en  = 1;
    cfg.ultrasonic_en = 1;
    for (int i = 0; i < TOP_CFG_NUM_TOF; ++i) {
        cfg.tof[i].enabled           = 1;
        cfg.tof[i].mount_bearing_deg = kDefaultBearing[i];
        cfg.tof[i].zone_rotation_deg = 0;
        cfg.tof[i].flip              = 0;
        cfg.tof[i].zone_mask         = ~(uint64_t)0;   // todas las zonas activas (identidad)
    }
}

int top_config_serialize(uint8_t* buf, int cap, const TopConfig& cfg) {
    if (buf == nullptr || cap < TOP_CONFIG_BLOB_SIZE) return -1;

    std::memset(buf, 0, TOP_CONFIG_BLOB_SIZE);
    buf[0] = TOP_CONFIG_MAGIC;
    buf[1] = TOP_CONFIG_VERSION;
    // CRC va en los últimos 2 bytes; se escribe al final.

    // Payload (offset 2..90)
    buf[2] = (uint8_t)(cfg.cam_front_en  ? 1 : 0);
    buf[3] = (uint8_t)(cfg.cam_back_en   ? 1 : 0);
    buf[4] = (uint8_t)(cfg.bno_left_en   ? 1 : 0);
    buf[5] = (uint8_t)(cfg.bno_right_en  ? 1 : 0);
    buf[6] = (uint8_t)(cfg.ultrasonic_en ? 1 : 0);
    for (int i = 0; i < TOP_CFG_NUM_TOF; ++i) {
        uint8_t* p = buf + 7 + i * TOP_CONFIG_TOF_BYTES;
        p[0] = (uint8_t)(cfg.tof[i].enabled ? 1 : 0);
        write_u16_le(p + 1, (uint16_t)cfg.tof[i].mount_bearing_deg);
        write_u16_le(p + 3, (uint16_t)cfg.tof[i].zone_rotation_deg);
        p[5] = cfg.tof[i].flip;
        write_u64_le(p + 6, cfg.tof[i].zone_mask);
    }

    // CRC sobre el payload (offsets 2..90), no el header.
    const uint16_t crc = top_config_crc16(buf + 2, TOP_CONFIG_PAYLOAD);
    write_u16_le(buf + 2 + TOP_CONFIG_PAYLOAD, crc);

    return TOP_CONFIG_BLOB_SIZE;
}

bool top_config_deserialize(const uint8_t* buf, int len, TopConfig& cfg) {
    if (buf == nullptr || len < TOP_CONFIG_BLOB_SIZE) return false;
    if (buf[0] != TOP_CONFIG_MAGIC)   return false;
    if (buf[1] != TOP_CONFIG_VERSION) return false;

    const uint16_t stored   = read_u16_le(buf + 2 + TOP_CONFIG_PAYLOAD);
    const uint16_t computed = top_config_crc16(buf + 2, TOP_CONFIG_PAYLOAD);
    if (stored != computed) return false;

    // Todo OK → poblar cfg.
    cfg.cam_front_en  = (uint8_t)(buf[2] ? 1 : 0);
    cfg.cam_back_en   = (uint8_t)(buf[3] ? 1 : 0);
    cfg.bno_left_en   = (uint8_t)(buf[4] ? 1 : 0);
    cfg.bno_right_en  = (uint8_t)(buf[5] ? 1 : 0);
    cfg.ultrasonic_en = (uint8_t)(buf[6] ? 1 : 0);
    for (int i = 0; i < TOP_CFG_NUM_TOF; ++i) {
        const uint8_t* p = buf + 7 + i * TOP_CONFIG_TOF_BYTES;
        cfg.tof[i].enabled           = (uint8_t)(p[0] ? 1 : 0);
        cfg.tof[i].mount_bearing_deg = (int16_t)read_u16_le(p + 1);
        cfg.tof[i].zone_rotation_deg = (int16_t)read_u16_le(p + 3);
        cfg.tof[i].flip              = p[5];
        cfg.tof[i].zone_mask         = read_u64_le(p + 6);
    }
    return true;
}

}  // namespace iitasoccer
