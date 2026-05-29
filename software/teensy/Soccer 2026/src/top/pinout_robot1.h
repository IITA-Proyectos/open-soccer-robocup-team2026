// pinout_robot1.h — Pinout específico del ROBOT 1 (arquero).
//
// Hardware: placa TOP rev 1.0 + bodge XSHUT manual de Enzo.
// Sensores instalados al 2026-05-29 (post-bodge): TOF frontal + trasero.
// Laterales pendientes de soldar.
//
// REGLA: si Enzo cambia el cableado físico, actualizar SOLO este archivo
// y reflashear con `pio run -e top_robot1 -t upload`. No tocar
// pinout_common.h ni los .cpp del firmware.
//
// Spec: docs/superpowers/specs/2026-05-29-hal-robot-config-design.md

#pragma once
#include <stdint.h>

namespace iitasoccer {

// ============================================================
// XSHUT (LPn) de los 4 TOFs — bodge físico de Enzo
// ============================================================
// PLACEHOLDER hasta confirmar con Enzo después del bodge. Estos pines
// están libres en rev 1.0 según `01-pinout-y-hardware.md` (NC en el
// schematic, sin conflicto con otros usos). Si Enzo soldó otros pines,
// CAMBIAR ACÁ y reflashear.
//
// Mapeo a slots físicos del PCB:
//   [0] = TOF frontal  (slot U2 del schematic)
//   [1] = TOF trasero  (slot U3 del schematic)
//   [2] = TOF izquierdo (slot U5 del schematic)
//   [3] = TOF derecho  (slot U17 del schematic)
constexpr int PIN_TOF_XSHUT[4] = {
    9,   // PLACEHOLDER — TOF[0] frontal U2
    11,  // PLACEHOLDER — TOF[1] trasero U3
    12,  // PLACEHOLDER — TOF[2] izquierdo U5
    22,  // PLACEHOLDER — TOF[3] derecho U17
};

// ============================================================
// Direcciones I²C asignadas tras enumeración XSHUT
// ============================================================
// Default del L7CX es 0x29. Cada TOF se levanta uno por uno con XSHUT y
// se le asigna una dirección distinta. El módulo sensors_tof.cpp (Sprint B
// futuro) hace esa enumeración usando este array.
constexpr uint8_t TOF_I2C_ADDR_ASSIGNED[4] = {
    0x29,  // TOF[0] frontal — mantiene default
    0x2A,  // TOF[1] trasero — reasignado
    0x2B,  // TOF[2] izquierdo — reasignado
    0x2C,  // TOF[3] derecho — reasignado
};

// ============================================================
// Feature flags — qué sensores están físicamente instalados HOY en R1
// ============================================================
// Actualizar cuando Enzo termine de soldar cada slot. Cambiar de 0 a 1
// el flag correspondiente cuando el sensor esté soldado + verificado.
#define ROBOT_HAS_TOF_FRONT 1
#define ROBOT_HAS_TOF_BACK  1
#define ROBOT_HAS_TOF_LEFT  0
#define ROBOT_HAS_TOF_RIGHT 0
#define ROBOT_HAS_OTOS      0   // R1 (arquero) sin OTOS

// Cantidad de TOFs activos (calcular a mano según los HAS_* arriba).
// Si cambian los flags, recalcular este valor.
constexpr int NUM_TOF_ACTIVE = 2;

// ============================================================
// Dipswitch de rol (idéntico ambos robots por hardware)
// ============================================================
constexpr int PIN_ROLE_DIPSWITCH = 10;

}  // namespace iitasoccer
