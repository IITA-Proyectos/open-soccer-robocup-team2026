// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
// sensor_geometry.h — Posiciones físicas reales de los 32 sensores ALS-PT19
// del anillo de la placa DOWN, extraídas del schematic + PCB layout JSON
// (2026-04-12) validado empíricamente 2026-05-24.
//
// Fuente canónica: hardware/electronics/down-board-pack/01-pinout-y-posiciones.md §5b
//
// Sistema de referencia:
//   - Origen = centro del PCB DOWN (asumido = centro del robot, ver §5b para
//     validación pendiente del montaje físico en el chasis — TASK-027).
//   - +X = derecha del robot (mirando al robot desde arriba con el frente "alejándose").
//   - +Y = adelante del robot (lado del logo IITA del silkscreen TOP).
//
// Convención angular del firmware (compatible con line_geometry.cpp):
//   0° = frente (+Y),  +90° = derecha (+X),  ±180° = atrás (-Y),  -90° = izquierda (-X).
//   Sentido horario positivo. Rango (-180°, +180°].

#pragma once
#include <stdint.h>

namespace iitasoccer {

constexpr int SENSOR_COUNT = 32;

struct SensorPos2D {
    float x_mm;   // +X = derecha del robot
    float y_mm;   // +Y = adelante del robot
};

// LUT de posiciones físicas de los 32 sensores.
// Índice 0-based: SENSOR_POS[0] = S1 del PCB = primer canal del mux U1.
// (El diag_down imprime "S0..S31" en serial, que mapea 1:1 con índices 0..31 de aquí.)
extern const SensorPos2D SENSOR_POS[SENSOR_COUNT];

// ============================================================
// Agrupación lógica por anillo (útil para algoritmos que ponderan
// distinto los sensores internos vs externos, o que necesitan saber
// qué cuadrante cubre cada mux).
// ============================================================
// Anillo externo frontal (mux U1, R≈80mm, Y≈+75-82mm, X∈[-36,+36])
constexpr int RING_OUTER_FRONT_FIRST = 0;
constexpr int RING_OUTER_FRONT_COUNT = 8;

// Anillo externo izquierdo + trasero-izq (mux U2, R≈85mm, X≈-81-86, Y∈[+12,-77])
constexpr int RING_OUTER_LEFT_FIRST  = 8;
constexpr int RING_OUTER_LEFT_COUNT  = 8;

// Anillo externo derecho + trasero-der (mux U3, R≈86mm, X∈[+36,+87], Y∈[-77,+12])
constexpr int RING_OUTER_RIGHT_FIRST = 16;
constexpr int RING_OUTER_RIGHT_COUNT = 8;

// Anillo interno (mux U4): 4 frontales R≈54mm + 4 centrales R≈37mm
constexpr int RING_INNER_FIRST       = 24;
constexpr int RING_INNER_COUNT       = 8;

// ============================================================
// Helpers calculados a partir de SENSOR_POS[i].
// Cheap (sólo cálculos float), pero NO marcados constexpr porque
// usan std::atan2/sqrt. Para uso en loops puede convenir cachear.
// ============================================================

// Ángulo polar del sensor i, desde el centro del robot.
// Convención: 0° = frente (+Y), horario positivo, rango (-180°, +180°].
// Ej: S0 está en (-36, +82) → ángulo ≈ -23.8° (frente-izquierda).
//     S20 está en (+82, -26) → ángulo ≈ +107° (atrás-derecha).
// Devuelve 0.0f si i está fuera de rango.
float sg_angle_deg(int i);

// Distancia (radio) del sensor i desde el centro del robot, en mm.
// Para los anillos externos da ~80-87mm; para los internos ~37-54mm.
// Devuelve 0.0f si i está fuera de rango.
float sg_radius_mm(int i);

// Llena un array out[n] con los ángulos físicos reales de los primeros n
// sensores. Útil para alimentar lg_compute() (la función vieja, que toma
// ángulos) con geometría REAL en lugar de la aproximación uniforme que
// devuelve lg_sensor_angle_deg(). Si n > SENSOR_COUNT los extras quedan en 0.
void sg_fill_angles_deg(float* out, int n);

}  // namespace iitasoccer
