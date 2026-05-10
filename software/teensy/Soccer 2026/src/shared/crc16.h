// crc16.h — CRC-16/CCITT-FALSE para integridad del protocolo UART entre placas
//
// Polinomio: 0x1021
// Init:      0xFFFF
// RefIn:     false
// RefOut:    false
// XorOut:    0x0000
//
// Implementación sin tabla (más lenta pero portable). En Teensy 4.0 @ 600MHz
// procesar 16 bytes toma ~1µs — despreciable.

#pragma once
#include <stdint.h>
#include <stddef.h>

namespace iitasoccer {

uint16_t crc16_ccitt(const uint8_t* data, size_t len);

}  // namespace iitasoccer
