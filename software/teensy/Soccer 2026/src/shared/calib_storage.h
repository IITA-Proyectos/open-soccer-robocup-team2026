// calib_storage.h — Serialización + validación de calibración persistente.
//
// Tema 2 P1 del research doc 2026-05-24-deteccion-linea-down.
//
// Este módulo NO toca EEPROM directamente. Su único trabajo es:
//   1. Serializar SensorCalib[N] a un buffer de bytes con formato chequeable.
//   2. Deserializar y validar (magic, versión, CRC) un buffer recibido.
//
// El glue con EEPROM física vive en `src/down/eeprom_calib.cpp` (Arduino-only)
// para mantener este módulo 100% host-testeable.
//
// Formato del buffer v2 (201 bytes total para 32 sensores):
//
//   offset  bytes  campo
//   ------  -----  ----------------------------------------
//        0      4  magic = CS_MAGIC (0x494954A1 = "IITA¡")
//        4      1  version = CS_VERSION (2)
//        5      1  n_sensors (1..32)
//        6      2  CRC16-CCITT del payload (offsets 8..end)
//        8      1  global_sens (int8, [-100,+100])           ← payload
//        9     N*6 array de [carpet_u16][white_u16][enabled_u8][sensitivity_i8]
//
// v1 (136 B: solo carpet/white, sin global_sens/enabled/sensitivity) quedó
// OBSOLETO. La deserialización rechaza buffers con versión distinta → fuerza
// recalibración (la EEPROM v1 vieja se descarta y el robot arranca con defaults:
// todos los sensores habilitados, sensibilidad 0 = comportamiento histórico).

#pragma once
#include <stdint.h>
#include "line_calib.h"

namespace iitasoccer {

constexpr int      CS_MAX_SENSORS  = 32;
constexpr uint32_t CS_MAGIC        = 0x494954A1u;  // "IITA" + signature byte
constexpr uint8_t  CS_VERSION      = 2;            // v2: +global_sens +enabled +sensitivity
// 4 (magic) + 1 (version) + 1 (n_sensors) + 2 (crc) + 1 (global_sens) + 6*N
constexpr int      CS_PAYLOAD_SIZE = 9 + (CS_MAX_SENSORS * 6);  // 201 bytes

// Serializa calib[] + global_sens al buffer `buf`. Calcula y embebe el CRC.
// Persiste por sensor: carpet, white, enabled, sensitivity (NO threshold: se
// recomputa como punto medio al cargar). Retorna bytes escritos, o -1 si el
// buffer es muy chico / args inválidos.
int cs_serialize(uint8_t* buf, int buf_size,
                 const SensorCalib* calib, int n_sensors, int8_t global_sens = 0);

// Deserializa desde el buffer a calib[] + *global_sens_out. Valida magic +
// version + n_sensors (EXACTO) + CRC. Si TODO OK, llena calib[] (carpet, white,
// enabled, sensitivity; threshold = punto medio) y *global_sens_out (si no es
// null), y retorna true. Si CUALQUIER validación falla, no modifica nada y
// retorna false (→ el llamador usa defaults).
bool cs_deserialize(const uint8_t* buf, int buf_size,
                    SensorCalib* calib, int n_sensors, int8_t* global_sens_out = nullptr);

// CRC-16/CCITT-FALSE (polinomio 0x1021, init 0xFFFF, sin reflexión, no xor-out).
// Expuesta para tests.
uint16_t cs_crc16_ccitt(const uint8_t* data, int len);

}  // namespace iitasoccer
