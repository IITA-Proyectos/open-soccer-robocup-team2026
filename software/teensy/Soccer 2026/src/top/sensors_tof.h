// sensors_tof.h — 4 slots ToF (VL53L7CX) + 1 HC-SR04 ultrasonido.
//
// Hardware (schematic TOP):
//   • U2, U3 → Wire   (I2C bus 0, compartiendo bus con BNO055 LEFT)
//   • U5, U17 → Wire1 (I2C bus 1, compartiendo bus con BNO055 RIGHT, REMAP 24/25)
//   • U6 HC-SR04 → TRIG (pin 6) y ECHO (pin 7)
//
// Estado al 2026-05-24 (ver journal/2026-05-24-hardware-up-top-tof-frontal-resuelto.md):
//   • Solo U2 (frontal, Wire @ 0x29) esta fisicamente instalado y operativo.
//     Lib usada: Adafruit_VL53L7CX (la STM32duino tiene bug en Teensy 4.0).
//   • U3 / U5 / U17 son slots vacios — get_distance_mm(1..3) retorna
//     TOF_NO_READING permanente hasta que lleguen los modulos. Cuando lleguen,
//     toca agregar enumeracion XSHUT (cambio de address I2C) en sensors_tof.cpp.
//   • HC-SR04 frontal funciona desde el dia 1.
//
// API publica estable: no cambio con la migracion stub -> Adafruit.

#pragma once
#include <stdint.h>
#include "config_top.h"

namespace iitasoccer {

constexpr uint16_t TOF_NO_READING = 0xFFFF;  // sentinel "no leído"
constexpr uint16_t TOF_MAX_RANGE_MM = 4000;  // ~4m según datasheet VL53L7CX

bool sensors_tof_init();

// Lee ToFs disponibles (no bloqueante — algunas lecturas son asincrónicas).
// Llamar a ~30 Hz desde el loop.
void sensors_tof_tick();

// Distancia en mm del ToF idx (0..NUM_TOF-1), o TOF_NO_READING si no listo.
uint16_t sensors_tof_get_distance_mm(uint8_t idx);

// Distancia del HC-SR04 frontal en mm.
uint16_t sensors_hcsr04_get_distance_mm();

// Distancia mínima entre los 4 ToF + HC-SR04 (útil para evasión genérica).
uint16_t sensors_tof_get_min_distance_mm();

// Diagnóstico:
bool sensors_tof_is_ready(uint8_t idx);
uint32_t sensors_tof_get_tick_count();

}  // namespace iitasoccer
