// pinout_robot2.h — Pinout específico del ROBOT 2 (delantero).
//
// ⚠️ CONFIG INCHEON 2026-06-17 (Gustavo): R2 = 2× BNO055 (primario en bus/dir I²C
//    propio a alta velocidad + centinela en el MISMO bus que los ToF a 1 Hz) + 4× ToF
//    + HC-SR04 + módulo de juez + 2× cámaras OpenMV + luz de la DOWN. R2 juega CON gyro.
//    R2 NO lleva OTOS (su DOWN va con NUM_OTOS=0). (R1 = igual pero SÍ con OTOS.)
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
// LP (XSHUT) de los 4 TOFs — bodge físico de Enzo
// ============================================================
// Mismo diseño que R1: 4 ToF en `Wire` (18/19) con LP individual; Wire2
// (24/25 — es Wire2/LPI2C4, NO "Wire1"; corrección 2026-06-09) ocupado por
// el BNO PRIMARIO de R2 (el enlace a DOWN es UART, no I²C).
// En R1 los pines LP {9,10,11,12}
// (activo-ALTO) quedaron CONFIRMADOS en banco (2026-05-30, ver journal).
// R2 es una construcción manual separada: se asumen los mismos pines, pero
// ⚠️ FALTA correr el banco en R2 (diag_top_tof_census, mismo procedimiento
// con POWER-CYCLE). Si en R2 dieran otros pines, CAMBIAR ACÁ y reflashear
// `pio run -e top_robot2 -t upload`.
// Posiciones (confirmadas en R1, esperadas iguales en R2 — confirmar en banco):
//   [0]=FRENTE [1]=ATRÁS [2]=DERECHA [3]=IZQUIERDA. Ángulos en pinout_common.h.
constexpr int PIN_TOF_XSHUT[4] = {
    9,   // TOF[0] FRENTE     (dir 0x2A)
    10,  // TOF[1] ATRÁS      (dir 0x2B)
    11,  // TOF[2] DERECHA    (dir 0x2C)
    12,  // TOF[3] IZQUIERDA  (dir 0x2D)
};

// ============================================================
// Direcciones I²C asignadas tras enumeración LP
// ============================================================
// Ninguno queda en 0x29 (el bodge controla los 4 LP). PROCEDIMIENTO al
// probar: power-cycle tras flashear (las direcciones persisten con 3V3).
constexpr uint8_t TOF_I2C_ADDR_ASSIGNED[4] = {
    0x2A, 0x2B, 0x2C, 0x2D,
};

// ============================================================
// Feature flags — qué sensores están físicamente instalados HOY en R2
// ============================================================
// 4 ToF fijos (mismo diseño que R1). En 1 asumiendo paridad con R1;
// confirmar enumeración + ranging en banco R2.
#define ROBOT_HAS_TOF_FRONT 1
#define ROBOT_HAS_TOF_BACK  1
#define ROBOT_HAS_TOF_LEFT  1
#define ROBOT_HAS_TOF_RIGHT 1
#define ROBOT_HAS_OTOS      0   // R2: su DOWN va SIN OTOS (no llegaron; env down_robot2 con OTOS=0)

constexpr int NUM_TOF_ACTIVE = 4;

// ⚠️ CONFLICTO DE PIN (ver R1): el pin 10 quedó como LP de ToF
// (PIN_TOF_XSHUT[1]=10) y acá también es el dipswitch de rol. No pueden
// coexistir; reubicar la lectura de rol a un pin libre. Decisión de
// hardware -> Enzo. Confirmar en el banco de R2.
constexpr int PIN_ROLE_DIPSWITCH = 10;

}  // namespace iitasoccer
