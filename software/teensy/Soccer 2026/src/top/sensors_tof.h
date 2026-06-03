// sensors_tof.h — 4 slots ToF (VL53L7CX) + 1 HC-SR04 ultrasonido.
//
// Hardware (estado vivo, banco 2026-05-30 — supera el esquema viejo de abajo):
//   • Los 4 ToF VL53L7CX cuelgan del bus ÚNICO Wire (I2C0, 18/19) y enumeran a
//     0x2A..0x2D vía las patas LP {9,10,11,12} (bodge de Enzo; TOP_ENABLE_MULTI_TOF).
//   • Wire1 (24/25) quedó LIBRE para la placa DOWN.
//   • HC-SR04 → TRIG=pin 4, ECHO=pin 3 (NO 6/7; ver pinout_common.h).
//   ⚠️ Probar SIEMPRE con power-cycle (las direcciones I2C de los VL53L7CX persisten).
//   Lib usada: Adafruit_VL53L7CX (la STM32duino tiene bug en Teensy 4.0).
//
// (Histórico previo al bodge 2026-05-30: U2/U3 en Wire; U5/U17 en Wire1 24/25;
//  HC-SR04 en TRIG6/ECHO7; solo U2 instalado. Superado — no usar.)
//
// API publica estable: no cambio con la migracion stub -> Adafruit.

#pragma once
#include <stdint.h>
#include "config_top.h"

namespace iitasoccer {

constexpr uint16_t TOF_NO_READING = 0xFFFF;  // sentinel "no leído"
constexpr uint16_t TOF_MAX_RANGE_MM = 4000;  // ~4m según datasheet VL53L7CX

bool sensors_tof_init();

// Duerme los 4 ToF (LP low) para dejar el bus I2C limpio ANTES de iniciar el BNO
// (los VL53L7CX arrancan en 0x29 = misma dir que el BNO derecho). Llamar desde
// setup() ANTES de sensors_imu_init(). No-op si no esta TOP_ENABLE_MULTI_TOF.
void sensors_tof_predim_lp();

// DIAG (bring-up): escanea el bus Wire (0x08..0x77) e imprime las direcciones que
// responden. Llamar con los ToF DORMIDOS (post predim, pre enumeracion) para ver
// SOLO los BNO (responde la pregunta: hay algo en 0x29 = 2do BNO con puente?).
void sensors_tof_scan_wire();

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
