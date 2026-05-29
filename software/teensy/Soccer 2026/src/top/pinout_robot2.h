// pinout_robot2.h — Pinout específico del ROBOT 2 (delantero).
//
// Hardware: placa TOP rev 1.0 + bodge XSHUT manual de Enzo. Los pines
// del bodge pueden ser DISTINTOS a R1 — cada robot tiene su construcción
// manual. Confirmar con Enzo qué pines usó.
//
// Spec: docs/superpowers/specs/2026-05-29-hal-robot-config-design.md

#pragma once
#include <stdint.h>

namespace iitasoccer {

// ============================================================
// XSHUT (LPn) de los 4 TOFs — bodge físico de Enzo
// ============================================================
// PLACEHOLDER (mismos que R1 hasta confirmación). Si Enzo soldó otros
// pines en R2, CAMBIAR ACÁ y reflashear `pio run -e top_robot2 -t upload`.
constexpr int PIN_TOF_XSHUT[4] = {
    9,   // PLACEHOLDER — TOF[0] frontal U2
    11,  // PLACEHOLDER — TOF[1] trasero U3
    12,  // PLACEHOLDER — TOF[2] izquierdo U5
    22,  // PLACEHOLDER — TOF[3] derecho U17
};

// ============================================================
// Direcciones I²C asignadas tras enumeración XSHUT
// ============================================================
constexpr uint8_t TOF_I2C_ADDR_ASSIGNED[4] = {
    0x29, 0x2A, 0x2B, 0x2C,
};

// ============================================================
// Feature flags — qué sensores están físicamente instalados HOY en R2
// ============================================================
#define ROBOT_HAS_TOF_FRONT 1
#define ROBOT_HAS_TOF_BACK  1
#define ROBOT_HAS_TOF_LEFT  0
#define ROBOT_HAS_TOF_RIGHT 0
#define ROBOT_HAS_OTOS      1   // R2 (delantero) con OTOS

constexpr int NUM_TOF_ACTIVE = 2;

constexpr int PIN_ROLE_DIPSWITCH = 10;

}  // namespace iitasoccer
