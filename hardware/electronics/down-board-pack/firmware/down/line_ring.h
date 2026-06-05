// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
// line_ring.h — Anillo de N sensores de luz multiplexados (CD4051 × 1-4)
//
// Lee los sensores ALS-PT19 con LEDs activos pareados a través de los muxes,
// detecta línea blanca, y entrega un dato procesado:
//   • ángulo de la línea (-180 .. +180 grados, 0 = frente)
//   • profundidad (cuántos sensores ven blanco simultáneamente)
//   • flag "salida inminente" (≥ N sensores blancos)
//
// Algoritmo del ángulo:
//   Cada sensor i tiene un ángulo físico θ_i = i × (360° / NUM_LINE_SENSORS).
//   Los sensores que ven blanco contribuyen un vector unitario en su ángulo.
//   El ángulo de la línea es atan2(sum_y, sum_x) sobre los vectores activos.
//
// Calibración:
//   • calibrate_carpet(): captura el promedio de cada sensor sobre carpet verde.
//   • calibrate_white():  captura el promedio sobre línea blanca.
//   • Umbral por sensor = (carpet[i] + white[i]) / 2.

#pragma once
#include <stdint.h>
#include "config_down.h"

namespace iitasoccer {

void line_ring_init();
void line_ring_tick();                  // muestrea los NUM_LINE_SENSORS sensores

// Datos procesados — válidos después de line_ring_tick().
float    line_ring_get_angle_deg();     // ángulo de la línea más fuerte detectada
uint8_t  line_ring_get_depth();         // cuántos sensores en blanco simultáneamente
bool     line_ring_get_imminent_exit(); // depth ≥ umbral
bool     line_ring_is_lifted();         // true si robot está en aire (datos no confiables)

// Acceso a lecturas crudas y calibración.
uint16_t line_ring_get_raw(uint8_t sensor_idx);    // 0..NUM_LINE_SENSORS-1
bool     line_ring_get_white(uint8_t sensor_idx);  // true si sensor i ve blanco
void     line_ring_calibrate_carpet();             // bloquea ~500 ms tomando muestras
void     line_ring_calibrate_white();              // idem

// Calibración por sensor — solo lectura, no modifica el muestreo.
uint16_t line_ring_get_carpet_avg(uint8_t sensor_idx); // 0..NUM_LINE_SENSORS-1
uint16_t line_ring_get_white_avg(uint8_t sensor_idx);  // 0..NUM_LINE_SENSORS-1

// Diagnóstico:
uint32_t line_ring_get_tick_count();
uint32_t line_ring_get_last_tick_us();

}  // namespace iitasoccer
