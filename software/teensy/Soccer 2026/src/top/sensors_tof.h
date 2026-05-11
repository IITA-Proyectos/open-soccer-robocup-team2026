// sensors_tof.h — 4 ToF (VL53L7CX o VL53L5CX) + 1 HC-SR04 ultrasonido.
//
// Hardware (schematic TOP):
//   • U2, U3 → Wire   (I2C bus 0, compartiendo bus con BNO055 LEFT)
//   • U5, U17 → Wire1 (I2C bus 1, compartiendo bus con BNO055 RIGHT, REMAP 24/25)
//   • U6 HC-SR04 → TRIG (pin 6) y ECHO (pin 7)
//
// Los ToF VL53L7CX y VL53L5CX entregan un array de distancias (zonas) además
// de un valor único promedio. Para simplificar el firmware inicial usamos solo
// el valor único central por ToF.
//
// Q4 del coach: el equipo compró VL53L5CX pero tardan en llegar; tienen
// VL53L7CX disponibles. Ambos usan la misma librería de ST (con flags por
// modelo). Hardware status pending — los ToF probablemente no están montados
// todavía en la placa que llegó.
//
// Stub: hasta confirmar qué modelo y que la lib está instalada, todas las
// lecturas retornan ToF_NO_READING.

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
