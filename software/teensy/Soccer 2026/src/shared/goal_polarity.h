// goal_polarity.h — Identifica qué COLOR de arco es el RIVAL vs el PROPIO.
//
// Módulo PURO host-testeable (solo <stdint.h>). En RCJ los arcos son de color
// (amarillo / azul) y, según el lado asignado, uno es el RIVAL (al que hay que
// hacer goles — lo ve la cámara de FRENTE) y el otro el PROPIO (el que defendemos
// — lo ve la cámara de ATRÁS).
//
// PROBLEMA QUE RESUELVE: hoy el mapeo color→propio/rival está HARDCODEADO en
// main_top.cpp (amarillo=rival, azul=propio). Si en la cancha tu arco es amarillo,
// queda INVERTIDO. Este módulo lo INFIERE de las cámaras: el arco que el robot
// tiene AL FRENTE (|ángulo| < 90°) es el rival.
//
// Convención de ángulo (igual que cámaras / sensor_geometry): 0° = frente (+Y),
// ±180° = atrás. |ángulo| < 90° = hemisferio delantero. El robot DEBE arrancar
// mirando a la cancha (frente al arco rival) — premisa estándar del kickoff.
//
// FAIL-SAFE: ante duda, el caller usa goal_polarity_effective(latch, DEFAULT)
// donde DEFAULT = el mapeo previo (YELLOW_IS_OPP) → comportamiento idéntico a hoy
// hasta que una inferencia CONFIABLE lo fije.

#pragma once
#include <stdint.h>

namespace iitasoccer {

enum class GoalPolarity : uint8_t {
    UNKNOWN = 0,    // no se pudo determinar (ningún arco al frente, o conflicto)
    YELLOW_IS_OPP,  // amarillo = rival (al frente); azul = propio
    BLUE_IS_OPP,    // azul = rival (al frente); amarillo = propio
};

// Infiere la polaridad de la visibilidad + ángulo (grados) de cada arco fusionado.
// Regla: el arco al FRENTE es el rival; corrobora con el otro (debería estar atrás).
// Devuelve UNKNOWN si no hay info clara, o si AMBOS parecen estar al frente (conflicto).
GoalPolarity goal_polarity_infer(bool yellow_visible, float yellow_angle_deg,
                                 bool blue_visible,   float blue_angle_deg);

// Cuántas lecturas consistentes confirman la polaridad (anti-rebote). A 100 Hz,
// 30 ≈ 0.3 s viendo el mismo resultado.
constexpr uint16_t GOAL_POLARITY_CONFIRM = 30;

// Latch anti-rebote: junta lecturas CONSISTENTES y, al confirmar, FIJA la polaridad
// para todo el partido (no cambia por una mala lectura puntual = estabilidad).
struct GoalPolarityLatch {
    GoalPolarity value;      // UNKNOWN hasta confirmar; luego FIJA
    GoalPolarity candidate;  // candidato en evaluación
    uint16_t     count;      // lecturas consecutivas consistentes del candidato
};

void goal_polarity_latch_init(GoalPolarityLatch& l);

// Alimenta una inferencia. Si la latch YA está fijada → NO cambia (estable todo el
// partido). Si no: acumula lecturas consistentes del mismo candidato; al llegar a
// GOAL_POLARITY_CONFIRM, fija. UNKNOWN resetea el candidato (no acumula).
// Devuelve l.value vigente (UNKNOWN si todavía no fijó).
GoalPolarity goal_polarity_latch_update(GoalPolarityLatch& l, GoalPolarity inferred);

// Polaridad EFECTIVA: la fijada si existe; si no, el `fallback` (fail-safe).
GoalPolarity goal_polarity_effective(const GoalPolarityLatch& l, GoalPolarity fallback);

}  // namespace iitasoccer
