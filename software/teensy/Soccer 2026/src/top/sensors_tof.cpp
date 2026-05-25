// sensors_tof.cpp — Implementacion VIVA del modulo ToF + HC-SR04 del TOP.
//
// ============================================================================
// MIGRACION 2026-05-24 — stub TODO_TOF_LIB -> Adafruit_VL53L7CX
// ----------------------------------------------------------------------------
// Hasta hoy este modulo era un stub: HC-SR04 funcionaba real pero los 4 ToF
// retornaban TOF_NO_READING en bloque (placeholder TODO_TOF_LIB).
//
// La lib STMicroelectronics (STM32duino_VL53L7CX) que probamos primero NO
// inicializa en Teensy 4.0 — bug raiz en `vl53l7cx_platform.h:49-60`,
// `DEFAULT_I2C_BUFFER_LEN = BUFFER_LENGTH (256)` desborda en 2 bytes el
// buffer del Wire al cargar el firmware blob del sensor (cada chunk son 2
// bytes header + 256 bytes payload). La lib Adafruit_VL53L7CX usa
// `maxBufferSize() - 2` (reserva el header) y funciona out of the box.
//
// Decision: migrar a Adafruit. Ver journal completo:
//   journal/2026-05-24-hardware-up-top-tof-frontal-resuelto.md
//
// Estado del hardware (al 2026-05-24):
//   • Solo el ToF frontal U2 esta fisicamente soldado (Wire / I2C0 / addr 0x29).
//   • U3, U5, U17 son slots vacios -> get_distance_mm(1..3) retorna
//     TOF_NO_READING permanente, is_ready(1..3) = false.
//   • HC-SR04 frontal (TRIG/ECHO) funciona como antes.
// ============================================================================

#include "sensors_tof.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VL53L7CX.h>

namespace iitasoccer {

namespace {

// ----- Estado del modulo -----
uint16_t g_distances_mm[NUM_TOF];
bool     g_ready[NUM_TOF];
uint16_t g_hcsr04_mm = TOF_NO_READING;
uint32_t g_tick_count = 0;

// ----- Sensor real (solo el frontal U2 esta instalado) -----
constexpr uint8_t TOF_FRONTAL_IDX = 0;  // indice U2 en la abstraccion
Adafruit_VL53L7CX     g_tof_frontal;
VL53L7CX_ResultsData  g_tof_results;
bool                  g_tof_init_logged = false;  // anti-spam del log de init

// Resolucion 4x4 = 16 zonas. Mas liviano que 8x8 y suficiente para un solo
// sensor que aporta "distancia frontal promedio" al firmware del TOP. Si en
// el futuro se quiere usar el array completo para evasion fina, subir a 64.
constexpr uint8_t TOF_RESOLUTION_ZONES = 16;  // 4x4
constexpr uint8_t TOF_RANGING_FREQ_HZ  = 15;

// HC-SR04 — lectura bloqueante, simple (sin cambios desde el stub).
uint16_t read_hcsr04() {
    digitalWrite(PIN_HCSR04_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_HCSR04_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_HCSR04_TRIG, LOW);

    // pulseIn con timeout — si no llega echo, retorna 0.
    const uint32_t duration_us = pulseIn(PIN_HCSR04_ECHO, HIGH, 25000UL);  // 25ms = ~4m
    if (duration_us == 0) return TOF_NO_READING;
    // Velocidad del sonido: 343 m/s = 0.343 mm/µs. Duracion es ida + vuelta.
    return static_cast<uint16_t>((duration_us * 343UL) / 2000UL);
}

// Promedia las zonas validas del frame 4x4 del L7CX. status==5/6/9 son
// "valid range" segun convencion ST. Devuelve TOF_NO_READING si NINGUNA
// zona del frame es valida (sensor mirando al vacio, fuera de rango, etc).
uint16_t mean_valid_zones(const VL53L7CX_ResultsData& r, uint8_t n_zones) {
    uint32_t sum = 0;
    uint16_t count = 0;
    for (uint8_t i = 0; i < n_zones; ++i) {
        const uint8_t s = r.target_status[i];
        const bool valid = (s == 5 || s == 6 || s == 9);
        const int16_t mm = r.distance_mm[i];
        if (valid && mm >= 0 && mm <= static_cast<int16_t>(TOF_MAX_RANGE_MM)) {
            sum += static_cast<uint32_t>(mm);
            ++count;
        }
    }
    if (count == 0) return TOF_NO_READING;
    return static_cast<uint16_t>(sum / count);
}

}  // namespace

bool sensors_tof_init() {
    // HC-SR04 frontal — igual que antes, funciona.
    pinMode(PIN_HCSR04_TRIG, OUTPUT);
    pinMode(PIN_HCSR04_ECHO, INPUT);

    for (int i = 0; i < NUM_TOF; ++i) {
        g_distances_mm[i] = TOF_NO_READING;
        g_ready[i] = false;
    }

    // NOTA: NO tocamos los pines XSHUT (PIN_TOF_XSHUT[]). Validado el
    // 2026-05-24: la lib Adafruit no los necesita (maneja el chip por I2C),
    // y en la placa actual esos pines probablemente ni siquiera estan
    // ruteados al modulo Pololu. Cuando lleguen los otros 3 ToF y haya que
    // enumerar 4 sensores en la misma direccion default, se agrega la
    // logica de XSHUT + setAddress() aca.

    // NOTA: NO tocamos `Wire.setClock()`. El main_top o cualquier otro
    // consumidor del bus puede setear el clock una vez al boot (default
    // 400 kHz = Adafruit default tambien). Si se setea aca y otro modulo
    // del TOP necesita otro clock, se pisan.

    // Init del ToF frontal U2 (unico instalado fisicamente al 2026-05-24).
    // begin() devuelve bool. Internamente carga ~85 KB de firmware blob por
    // I2C, puede tardar hasta ~10 s.
    if (!g_tof_frontal.begin(VL53L7CX_DEFAULT_ADDRESS, &Wire, 400000)) {
        if (!g_tof_init_logged) {
            Serial.println(F("[sensors_tof] WARN: VL53L7CX U2 begin() fallo; "
                             "se sigue sin ToF frontal."));
            g_tof_init_logged = true;
        }
        // No retornamos false — el HC-SR04 puede seguir funcionando y el
        // resto del firmware del TOP necesita seguir corriendo.
        return true;
    }
    if (!g_tof_frontal.setResolution(TOF_RESOLUTION_ZONES) ||
        !g_tof_frontal.setRangingFrequency(TOF_RANGING_FREQ_HZ) ||
        !g_tof_frontal.startRanging()) {
        if (!g_tof_init_logged) {
            Serial.println(F("[sensors_tof] WARN: VL53L7CX U2 config/start fallo; "
                             "se sigue sin ToF frontal."));
            g_tof_init_logged = true;
        }
        return true;
    }

    g_ready[TOF_FRONTAL_IDX] = true;
    Serial.println(F("[sensors_tof] OK: VL53L7CX U2 frontal listo "
                     "(4x4 @ 15 Hz, lib Adafruit)."));
    g_tof_init_logged = true;
    return true;
}

void sensors_tof_tick() {
    g_tick_count++;

    // ToF frontal U2: polling no bloqueante. isDataReady() es un read I2C
    // chico (~milisegundos en 400 kHz); si hay frame nuevo, getRangingData
    // lo trae y promediamos las zonas validas.
    if (g_ready[TOF_FRONTAL_IDX]) {
        if (g_tof_frontal.isDataReady()) {
            if (g_tof_frontal.getRangingData(&g_tof_results)) {
                g_distances_mm[TOF_FRONTAL_IDX] =
                    mean_valid_zones(g_tof_results, TOF_RESOLUTION_ZONES);
            }
            // Si getRangingData() devuelve false, dejamos el ultimo valor
            // valido cacheado — preferimos "ultimo dato bueno" a un sentinel
            // que dispare evasion espuria por un frame perdido.
        }
    }

    // HC-SR04 — lectura bloqueante. Corremos solo cada N ticks para no
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
