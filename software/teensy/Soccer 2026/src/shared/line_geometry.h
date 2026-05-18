#pragma once
#include <stdint.h>
#include "types.h"
namespace iitasoccer {
struct GeomResult {
    bool    line_present;
    int16_t line_angle_centideg;     // LSV2_NA_I16 si no hay línea
    int16_t escape_angle_centideg;   // LSV2_NA_I16 si no hay línea
    uint8_t sensors_on_line;
    bool    corner;                  // (se completa en Task 3; aquí false)
};
// Ángulo físico del sensor i en un anillo de n: 0=frente, horario+, grados.
float lg_sensor_angle_deg(int i, int n);
// Centroide angular de los sensores en blanco. escape = opuesto al centroide.
GeomResult lg_compute(const bool* white, const float* sensor_angle_deg, int n);
}  // namespace iitasoccer
