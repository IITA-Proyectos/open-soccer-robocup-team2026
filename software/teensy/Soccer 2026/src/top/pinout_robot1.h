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
// LP (XSHUT) de los 4 TOFs — bodge físico de Enzo (CONFIRMADO en banco)
// ============================================================
// Recableado 2026-05-30: los 4 ToF cuelgan del MISMO bus (Wire, 18/19) y
// la pata LP de cada uno está cableada por bodge a un pin del Teensy
// (reusando la traza que iba a INT, anulando INT). Esto liberó Wire1
// (24/25) para la placa DOWN.
//
// Pines CONFIRMADOS en banco (diag_top_tof_census, R1, 2026-05-30):
//   LP = {9, 10, 11, 12}, polaridad ACTIVO-ALTO (HIGH = ToF despierto).
//   Los 4 ToF enumeran a 0x2A/0x2B/0x2C/0x2D. (La hipótesis previa tenía
//   22 en vez de 10 — corregido.)
// Ver journal/2026-05-30-top-tof-4-en-bus-unico-enumeracion-ok.md.
//
// Mapeo pin -> posición física CONFIRMADO en banco (2026-05-30, Gustavo):
//   [0] = LP pin 9  → dir 0x2A → FRENTE
//   [1] = LP pin 10 → dir 0x2B → ATRÁS
//   [2] = LP pin 11 → dir 0x2C → DERECHA
//   [3] = LP pin 12 → dir 0x2D → IZQUIERDA
// El ángulo de montaje de cada índice está en pinout_common.h
// (TOF_MOUNT_ANGLE_DEG = {0,180,270,90}, ya alineado con este mapeo).
//
// ⚠️ PENDIENTE distinto (no es la posición): la ORIENTACIÓN INTERNA DE LAS ZONAS
// de cada sensor. El ToF IZQUIERDO es de otro fabricante y se montó mirando
// hacia abajo → puede tener arriba/abajo y/o izq/der de su grilla invertidos.
// Verificar con diag_top_tof_zonemap. Para el barrido tipo lidar-360 esto
// importa; para el promedio de zonas actual no. Ver journal 2026-05-31.
constexpr int PIN_TOF_XSHUT[4] = {
    9,   // TOF[0] FRENTE     (dir 0x2A)
    10,  // TOF[1] ATRÁS      (dir 0x2B)
    11,  // TOF[2] DERECHA    (dir 0x2C)
    12,  // TOF[3] IZQUIERDA  (dir 0x2D)
};

// ============================================================
// Direcciones I²C asignadas tras enumeración LP
// ============================================================
// Default del L7CX es 0x29. Al boot se duermen todos los LP, se despierta
// uno por uno y a cada ToF se le asigna una dirección distinta. NINGUNO
// queda en 0x29 (el bodge controla los 4 LP). El módulo sensors_tof.cpp
// (HAL Sprint B, pendiente) hará esa enumeración usando este array.
//
// PROCEDIMIENTO OBLIGATORIO al probar/enumerar: las direcciones I²C de los
// VL53L7CX PERSISTEN mientras tengan 3V3 (un reset del Teensy NO las borra).
// Hay que QUITAR ENERGÍA Y REPONERLA (power-cycle) tras flashear, o el bus
// arranca sucio. Ver el journal del 2026-05-30.
constexpr uint8_t TOF_I2C_ADDR_ASSIGNED[4] = {
    0x2A,  // TOF[0]
    0x2B,  // TOF[1]
    0x2C,  // TOF[2]
    0x2D,  // TOF[3]
};

// ============================================================
// Feature flags — qué sensores están físicamente instalados HOY en R1
// ============================================================
// Los 4 ToF fijos están soldados y ENUMERAN en banco (2026-05-30). Quedan
// en 1 hasta confirmar RANGING real con diag_top_tof_quad_live + mapear
// posición. (Ranging != solo responder I²C.)
#define ROBOT_HAS_TOF_FRONT 1
#define ROBOT_HAS_TOF_BACK  1
#define ROBOT_HAS_TOF_LEFT  1
#define ROBOT_HAS_TOF_RIGHT 1
#define ROBOT_HAS_OTOS      1   // R1: su DOWN lleva los 2 OTOS (env down; corrección 2026-06-10 — estaba invertido con R2)

// Cantidad de TOFs activos (calcular a mano según los HAS_* arriba).
// 4 ToF fijos confirmados enumerando. Ver NUM_TOF / NUM_TOF_MAX en
// pinout_common.h para el plan de escalado a 6 (4 fijos + 2 móviles).
constexpr int NUM_TOF_ACTIVE = 4;

// ============================================================
// Dipswitch de rol (idéntico ambos robots por hardware)
// ============================================================
// ⚠️ CONFLICTO DE PIN (detectado 2026-05-30): el banco confirmó que el pin
// 10 controla un LP de ToF (PIN_TOF_XSHUT[1]=10). Pero acá el dipswitch de
// rol también está en 10. NO pueden coexistir. Hay que reubicar la lectura
// de rol a otro pin libre (p.ej. 22/23, que quedaron libres al confirmarse
// que el LP NO usa el 22). Decisión de hardware -> Enzo. Hasta resolverlo,
// la lectura de rol por dipswitch es NO confiable en este pin.
constexpr int PIN_ROLE_DIPSWITCH = 10;

}  // namespace iitasoccer
