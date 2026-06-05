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
//
// ⚠️ NOTA sobre el PESO de cada sensor (modo degradado n<32 / mux muerto):
//   lg_compute() pondera cada sensor blanco por su VECTOR UNITARIO (cos/sin de su
//   ángulo) — todos los sensores pesan IGUAL, sin importar en qué anillo están.
//   En cambio lg_compute_xy() (camino n==32, geometría completa) pondera por la
//   POSICIÓN REAL (x,y), así que un sensor del anillo externo pesa más que uno
//   interno. Consecuencia: en el modo parcial/degradado el `line_angle` puede
//   SESGARSE hacia los anillos internos (su contribución relativa sube respecto
//   del cartesiano). El SIGNO y la CONVENCIÓN (0°=frente, horario+, escape=opuesto)
//   se CONSERVAN — sólo cambia la magnitud/exactitud del ángulo, no su sentido.
//   Es un fallback aceptable cuando la geometría completa no está disponible.
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
