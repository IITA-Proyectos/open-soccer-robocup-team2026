// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#include "sensors_tof.h"

#include <Arduino.h>

// ============================================================================
// STUB hasta confirmar modelo y lib (Q4: VL53L7CX disponible, VL53L5CX por llegar).
// Cuando se confirme, descomentar el bloque TODO_TOF_LIB y elegir entre:
//   • #include "vl53l5cx_class.h"  // STM32duino VL53L5CX
//   • #include "vl53l7cx_class.h"  // STM32duino VL53L7CX
//   • #include "VL53L1X.h"          // Pololu (más simple, sin array)
// ============================================================================

namespace iitasoccer {

namespace {

uint16_t g_distances_mm[NUM_TOF];
bool     g_ready[NUM_TOF];
uint16_t g_hcsr04_mm = TOF_NO_READING;
uint32_t g_tick_count = 0;

// HC-SR04 — lectura bloqueante, simple.
uint16_t read_hcsr04() {
    digitalWrite(PIN_HCSR04_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_HCSR04_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_HCSR04_TRIG, LOW);

    // pulseIn con timeout — si no llega echo, retorna 0.
    const uint32_t duration_us = pulseIn(PIN_HCSR04_ECHO, HIGH, 25000UL);  // 25ms = ~4m
    if (duration_us == 0) return TOF_NO_READING;
    // Velocidad del sonido: 343 m/s = 0.343 mm/µs. Duración es ida + vuelta.
    return static_cast<uint16_t>((duration_us * 343UL) / 2000UL);
}

}  // namespace

bool sensors_tof_init() {
    pinMode(PIN_HCSR04_TRIG, OUTPUT);
    pinMode(PIN_HCSR04_ECHO, INPUT);

    for (int i = 0; i < NUM_TOF; ++i) {
        g_distances_mm[i] = TOF_NO_READING;
        g_ready[i] = false;
        pinMode(PIN_TOF_XSHUT[i], OUTPUT);
        // Mantener todos los ToF apagados (XSHUT low) hasta enumerarlos.
        digitalWrite(PIN_TOF_XSHUT[i], LOW);
    }

    // TODO_TOF_LIB_BEGIN
    //
    // Procedimiento de enumeración estándar de ToF I2C:
    //   1. Encender ToF 0 (XSHUT[0] HIGH), inicializarlo, cambiar dirección a 0x52.
    //   2. Encender ToF 1, inicializarlo, dirección 0x54.
    //   3. ... idem para los 4.
    // El detalle exacto depende del modelo VL53L7CX vs VL53L5CX vs VL53L1X.
    //
    // for (int i = 0; i < NUM_TOF; ++i) {
    //     digitalWrite(PIN_TOF_XSHUT[i], HIGH);
    //     delay(10);
    //     // ... init lib, set address, start ranging ...
    //     g_ready[i] = true;
    // }
    //
    // TODO_TOF_LIB_END

    return true;  // siempre OK con stub (HC-SR04 funciona)
}

void sensors_tof_tick() {
    g_tick_count++;

    // TODO_TOF_LIB_BEGIN
    // for (int i = 0; i < NUM_TOF; ++i) {
    //     if (g_ready[i] && tof[i]->isReady()) {
    //         g_distances_mm[i] = tof[i]->getDistance();
    //     }
    // }
    // TODO_TOF_LIB_END

    // HC-SR04 — lectura bloqueante; corremos solo cada N ticks para no
    // saturar el loop (cada lectura puede tomar hasta 25ms).
    static uint32_t last_hc = 0;
    if (g_tick_count - last_hc >= 3) {
        g_hcsr04_mm = read_hcsr04();
        last_hc = g_tick_count;
    }
}

uint16_t sensors_tof_get_distance_mm(uint8_t idx) {
    if (idx >= NUM_TOF) return TOF_NO_READING;
    return g_distances_mm[idx];
}

uint16_t sensors_hcsr04_get_distance_mm() { return g_hcsr04_mm; }

uint16_t sensors_tof_get_min_distance_mm() {
    uint16_t min_d = TOF_NO_READING;
    for (int i = 0; i < NUM_TOF; ++i) {
        if (g_distances_mm[i] != TOF_NO_READING && g_distances_mm[i] < min_d) {
            min_d = g_distances_mm[i];
        }
    }
    if (g_hcsr04_mm != TOF_NO_READING && g_hcsr04_mm < min_d) {
        min_d = g_hcsr04_mm;
    }
    return min_d;
}

bool sensors_tof_is_ready(uint8_t idx) {
    return idx < NUM_TOF && g_ready[idx];
}

uint32_t sensors_tof_get_tick_count() { return g_tick_count; }

}  // namespace iitasoccer
