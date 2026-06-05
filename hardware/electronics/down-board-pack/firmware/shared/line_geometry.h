// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#pragma once
#include <stdint.h>
#include "types.h"
namespace iitasoccer {

struct SensorPos2D;  // forward decl — definida en sensor_geometry.h

struct GeomResult {
    bool    line_present;
    int16_t line_angle_centideg;     // LSV2_NA_I16 si no hay línea
    int16_t escape_angle_centideg;   // LSV2_NA_I16 si no hay línea
    uint8_t sensors_on_line;
    bool    corner;
};

// Ángulo uniforme del sensor i en un anillo "ideal" de n sensores
// (asume anillo circular regular). 0=frente, horario+, grados.
// Para geometría REAL del PCB, usar sg_angle_deg() de sensor_geometry.h.
float lg_sensor_angle_deg(int i, int n);

// Centroide ANGULAR (solo direcciones, ignora distancia radial). escape = opuesto.
// Caller pasa el array `sensor_angle_deg[n]` con el ángulo de cada sensor.
// Para geometría real: llenar el array con sg_fill_angles_deg() antes de llamar.
GeomResult lg_compute(const bool* white, const float* sensor_angle_deg, int n);

// Centroide CARTESIANO usando las posiciones (x, y) reales de cada sensor.
// Es la versión "geométricamente correcta" cuando los sensores están en distintos
// radios (caso del PCB DOWN real: 3 anillos con R ≈ 37-87 mm, no equidistantes).
// `pos[i]` es la posición física del sensor i; convención del firmware
// (+X derecha, +Y adelante). El line_angle resultante usa la misma convención
// 0°=frente, horario+, rango (-180°, +180°], escape = opuesto.
//
// Diferencia con lg_compute(): un sensor cualquiera contribuye con su VECTOR
// (x, y) — no con un vector unitario en su dirección. Así un sensor del anillo
// externo "pesa" más que uno del interno (y eso es CORRECTO porque está más
// lejos del centro: ver la línea ahí significa que el borde está más cerca de
// la línea de fuga del robot).
//
// Esta versión NO computa corner detection (la lógica de "2 clusters separados
// ~90°" se puede combinar después). Si necesitás corner, llamá también a
// lg_compute() con sg_fill_angles_deg() en paralelo.
GeomResult lg_compute_xy(const bool* white, const SensorPos2D* pos, int n);

}  // namespace iitasoccer
