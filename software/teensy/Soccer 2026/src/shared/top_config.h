// top_config.h — Config persistente de sensores de la placa TOP (A2 del monitor
// de posicionamiento). Módulo PURO (solo <stdint.h>, sin Arduino): serializa/
// valida un blob de bytes con CRC. El glue con la EEPROM física vive en
// src/top/top_eeprom_config.cpp (Arduino-only) → este módulo es host-testeable.
//
// Hermano de calib_storage.h (DOWN). Mismo patrón de 3 capas: puro (acá) + glue
// EEPROM + carga al boot. Spec: research/in-progress/2026-06-13-diseno-monitor-
// general-top-config-persistente.md. Contrato EEPROM: docs/firmware/EEPROM-MAP.md.
//
// **Defaults = no-op EXACTO**: todo habilitado, bearings = el mapeo hardcodeado de
// hoy (TOF_MOUNT_ANGLE_DEG {0,180,270,90}), rotación/flip/máscara en identidad. Con
// EEPROM en blanco (0xFF) el CRC falla → defaults → competencia byte-idéntica.
//
// Formato del blob v1 (93 bytes):
//   off  bytes  campo
//   ---  -----  --------------------------------------------------
//     0      1  magic = TOP_CONFIG_MAGIC (0x7C)
//     1      1  version = TOP_CONFIG_VERSION (1)
//     2      1  cam_front_en            ← payload (offsets 2..90)
//     3      1  cam_back_en
//     4      1  bno_left_en
//     5      1  bno_right_en
//     6      1  ultrasonic_en
//     7   6*14  tof[6] = [enabled u8 | bearing i16 | rotation i16 | flip u8 | mask u64] LE
//    91      2  CRC16-CCITT del payload (offsets 2..90)
//
// ⚠️ Se serializa BYTE-A-BYTE little-endian (no memcpy del struct): el padding del
// struct en memoria puede diferir host(x86)↔Teensy(ARM). El blob es el contrato.

#pragma once
#include <stdint.h>

namespace iitasoccer {

constexpr uint8_t TOP_CONFIG_MAGIC   = 0x7C;
constexpr uint8_t TOP_CONFIG_VERSION = 1;
constexpr int     TOP_CFG_NUM_TOF    = 6;   // 4 fijos hoy + 2 futuros (alineado con TT_MAX_TOF)

// Config por ToF. enabled + bearing son A2.1 (lecturas escalares, no tocan el loop).
// rotation/flip/zone_mask están RESERVADOS para A2.2 (anular zonas / rotar la grilla
// 8x8): se serializan ya (mismo formato/versión), pero su apply llega en A2.2 →
// A2.2 NO es wire-breaking respecto a A2.1, solo activa apply-points hoy no-op.
struct TofZoneConfig {
    uint8_t  enabled;            // A2.1: 1 = participa; 0 = NO_READING siempre
    int16_t  mount_bearing_deg;  // A2.1: UBICACIÓN (0=frente,90=der,180=atrás,270=izq)
    int16_t  zone_rotation_deg;  // A2.2 reservado (0/90/180/270; default 0)
    uint8_t  flip;               // A2.2 reservado (bit0=invertir X, bit1=Y; default 0)
    uint64_t zone_mask;          // A2.2 reservado (1 bit/zona, 1=usar 0=anular; default ~0)
};

struct TopConfig {
    uint8_t cam_front_en;
    uint8_t cam_back_en;
    uint8_t bno_left_en;
    uint8_t bno_right_en;
    uint8_t ultrasonic_en;
    TofZoneConfig tof[TOP_CFG_NUM_TOF];
};

constexpr int TOP_CONFIG_TOF_BYTES = 14;  // enabled(1)+bearing(2)+rot(2)+flip(1)+mask(8)
constexpr int TOP_CONFIG_PAYLOAD   = 5 + TOP_CFG_NUM_TOF * TOP_CONFIG_TOF_BYTES;  // 89
constexpr int TOP_CONFIG_BLOB_SIZE = 2 + TOP_CONFIG_PAYLOAD + 2;                  // 93

// Defaults no-op: todo habilitado, bearings = {0,180,270,90,0,0}, rot/flip 0, mask ~0.
void top_config_defaults(TopConfig& cfg);

// Serializa cfg → buf (LE byte a byte + CRC). Retorna bytes escritos (=BLOB_SIZE)
// o -1 si buf chico / args inválidos.
int top_config_serialize(uint8_t* buf, int cap, const TopConfig& cfg);

// Deserializa buf → cfg. Valida magic + version (EXACTO) + CRC. Si TODO OK llena
// cfg y retorna true; si CUALQUIER validación falla NO toca cfg y retorna false
// (→ el llamador usa defaults). EEPROM en blanco (0xFF) → CRC falla → false.
bool top_config_deserialize(const uint8_t* buf, int len, TopConfig& cfg);

// CRC-16/CCITT-FALSE (0x1021, init 0xFFFF, sin reflexión, sin xor-out). Expuesta
// para tests; misma que cs_crc16_ccitt de DOWN.
uint16_t top_config_crc16(const uint8_t* data, int len);

}  // namespace iitasoccer
