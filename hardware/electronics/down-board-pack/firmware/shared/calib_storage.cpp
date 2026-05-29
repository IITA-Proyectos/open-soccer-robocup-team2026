// calib_storage.cpp — implementación. Ver calib_storage.h para contrato.

#include "calib_storage.h"
#include <cstring>

namespace iitasoccer {

uint16_t cs_crc16_ccitt(const uint8_t* data, int len) {
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

inline void write_u32_le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}
inline void write_u16_le(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}
inline uint32_t read_u32_le(const uint8_t* p) {
    return ((uint32_t)p[0])       | (((uint32_t)p[1]) << 8) |
           (((uint32_t)p[2]) << 16) | (((uint32_t)p[3]) << 24);
}
inline uint16_t read_u16_le(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0]) | (((uint16_t)p[1]) << 8));
}

}  // namespace

int cs_serialize(uint8_t* buf, int buf_size,
                 const SensorCalib* calib, int n_sensors) {
    if (buf == nullptr || calib == nullptr) return -1;
    if (n_sensors <= 0 || n_sensors > CS_MAX_SENSORS) return -1;
    if (buf_size < CS_PAYLOAD_SIZE) return -1;

    // Limpiar todo el buffer para que los sensores no usados queden en 0.
    std::memset(buf, 0, CS_PAYLOAD_SIZE);

    write_u32_le(buf + 0, CS_MAGIC);
    buf[4] = CS_VERSION;
    buf[5] = (uint8_t)n_sensors;
    // CRC va en bytes 6-7; lo escribimos al final.

    // Payload: array de [carpet][white] por sensor a partir del offset 8.
    for (int i = 0; i < n_sensors; ++i) {
        write_u16_le(buf + 8 + i * 4 + 0, calib[i].carpet);
        write_u16_le(buf + 8 + i * 4 + 2, calib[i].white);
    }

    // CRC sobre el payload (no incluye el header de los primeros 8 bytes).
    const uint16_t crc = cs_crc16_ccitt(buf + 8, CS_PAYLOAD_SIZE - 8);
    write_u16_le(buf + 6, crc);

    return CS_PAYLOAD_SIZE;
}

bool cs_deserialize(const uint8_t* buf, int buf_size,
                    SensorCalib* calib, int n_sensors) {
    if (buf == nullptr || calib == nullptr) return false;
    if (n_sensors <= 0 || n_sensors > CS_MAX_SENSORS) return false;
    if (buf_size < CS_PAYLOAD_SIZE) return false;

    // Validar magic.
    if (read_u32_le(buf + 0) != CS_MAGIC) return false;
    // Validar versión exacta. Si en el futuro hay migración, agregar lógica
    // que decida si la versión vieja es upgradeable, pero por ahora rechaza.
    if (buf[4] != CS_VERSION) return false;
    // n_sensors debe coincidir EXACTO con lo esperado por el llamador.
    if (buf[5] != (uint8_t)n_sensors) return false;

    // Validar CRC del payload.
    const uint16_t stored_crc   = read_u16_le(buf + 6);
    const uint16_t computed_crc = cs_crc16_ccitt(buf + 8, CS_PAYLOAD_SIZE - 8);
    if (stored_crc != computed_crc) return false;

    // Todo OK → poblar calib[].
    for (int i = 0; i < n_sensors; ++i) {
        const uint16_t carpet = read_u16_le(buf + 8 + i * 4 + 0);
        const uint16_t white  = read_u16_le(buf + 8 + i * 4 + 2);
        calib[i].carpet    = carpet;
        calib[i].white     = white;
        // Threshold = punto medio (consistente con lc_set_static).
        // Si white == carpet (sensor inválido), threshold queda igual al
        // valor; el detector de suspect lo va a marcar con margin chico.
        calib[i].threshold = (uint16_t)((uint32_t)(carpet + white) / 2);
    }
    return true;
}

}  // namespace iitasoccer
