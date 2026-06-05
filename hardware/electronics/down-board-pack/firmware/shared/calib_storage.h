// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
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
// Formato del buffer (136 bytes total para 32 sensores):
//
//   offset  bytes  campo
//   ------  -----  ----------------------------------------
//        0      4  magic = CS_MAGIC (0x494954A1 = "IITA¡")
//        4      1  version = CS_VERSION (1)
//        5      1  n_sensors (1..32)
//        6      2  CRC16-CCITT del payload (offsets 8..end)
//        8     N*4 array de [carpet_uint16][white_uint16] por sensor
//
// Si la versión sube en el futuro, cambiar CS_VERSION. La deserialización
// rechaza buffers con versión distinta — fuerza recalibración manual antes
// de aceptar EEPROM viejo con esquema incompatible.

#pragma once
#include <stdint.h>
#include "line_calib.h"

namespace iitasoccer {

constexpr int      CS_MAX_SENSORS  = 32;
constexpr uint32_t CS_MAGIC        = 0x494954A1u;  // "IITA" + signature byte
constexpr uint8_t  CS_VERSION      = 1;
// 4 (magic) + 1 (version) + 1 (n_sensors) + 2 (crc) + 4*N (carpet+white)
constexpr int      CS_PAYLOAD_SIZE = 8 + (CS_MAX_SENSORS * 4);  // 136 bytes

// Serializa calib[] al buffer `buf`. Calcula y embebe el CRC.
// Retorna la cantidad de bytes escritos, o -1 si el buffer es muy chico o
// los argumentos son inválidos.
int cs_serialize(uint8_t* buf, int buf_size,
                 const SensorCalib* calib, int n_sensors);

// Deserializa desde el buffer a calib[]. Valida magic + version + n_sensors
// (debe coincidir EXACTO con el n_sensors pasado) + CRC.
// Si TODO está OK, llena calib[] (carpet y white, threshold se calcula como
// el punto medio) y retorna true.
// Si CUALQUIER validación falla, calib[] no se modifica y retorna false.
bool cs_deserialize(const uint8_t* buf, int buf_size,
                    SensorCalib* calib, int n_sensors);

// CRC-16/CCITT-FALSE (polinomio 0x1021, init 0xFFFF, sin reflexión, no xor-out).
// Expuesta para tests.
uint16_t cs_crc16_ccitt(const uint8_t* data, int len);

}  // namespace iitasoccer
