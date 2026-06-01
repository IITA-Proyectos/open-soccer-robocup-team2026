// pinout_common.h — Constantes idénticas en ambos robots (R1 arquero, R2
// delantero). Incluido por hardware_profile.h antes del pinout específico
// del robot. NO incluir directamente desde código del firmware — usar
// hardware_profile.h o el wrapper legacy config_top.h.
//
// Spec: docs/superpowers/specs/2026-05-29-hal-robot-config-design.md

#pragma once
#include <stdint.h>

namespace iitasoccer {

// ============================================================
// I2C buses (TOP placa rev 1.0 — hardware fijo, ambos robots)
// ============================================================
constexpr int WIRE1_SCL_PIN = 24;
constexpr int WIRE1_SDA_PIN = 25;

constexpr uint8_t BNO055_LEFT_I2C_ADDR  = 0x28;
constexpr uint8_t BNO055_RIGHT_I2C_ADDR = 0x28;
constexpr uint8_t VL53L7CX_DEFAULT_I2C_ADDR = 0x29;

// ============================================================
// Topología ToF (recableado 2026-05-30, confirmado en banco)
// ============================================================
// Los ToF VL53L7CX cuelgan TODOS del bus principal `Wire` (18/19), junto a
// un BNO055 (0x28). Cada ToF tiene su pata LP cableada a un pin del Teensy
// (bodge de Enzo, reusando las trazas de INT). Al boot se enumeran: dormir
// todos los LP -> despertar uno por uno -> asignar dirección distinta.
// Esto dejó `Wire1` (24/25) LIBRE para comunicación con la placa DOWN.
//
// ⚠️ PROCEDIMIENTO OBLIGATORIO al probar/enumerar ToF: las direcciones I²C
// de los VL53L7CX PERSISTEN mientras el módulo tenga 3V3. Un reset del
// Teensy NO las borra. Hay que QUITAR ENERGÍA Y REPONERLA (power-cycle)
// tras flashear, o el bus arranca sucio (ToF pegados en direcciones de
// corridas previas) y la enumeración no parte de fábrica (0x29).
// Ver journal/2026-05-30-top-tof-4-en-bus-unico-enumeracion-ok.md.

// ============================================================
// UARTs — pines fijos del Teensy 4.0
// ============================================================
constexpr long UART_FROM_DOWN_BAUD = 230400;   // Serial1
constexpr long UART_TO_ZIRCON_BAUD = 230400;   // Serial7 (TX29/RX28) → CENTRAL/Zircon
constexpr long UART_CAMERA1_BAUD   = 19200;    // Serial3
constexpr long UART_TO_COMM_BAUD   = 115200;   // Serial4
constexpr long UART_CAMERA2_BAUD   = 19200;    // Serial5 (cámara trasera, soldada pin 21)

// ============================================================
// HC-SR04 ultrasonido frontal (idéntico ambos robots)
// ============================================================
constexpr int PIN_HCSR04_TRIG = 6;
constexpr int PIN_HCSR04_ECHO = 7;

// ============================================================
// LED de estado (LED_BUILTIN del Teensy)
// ============================================================
constexpr int PIN_LED_STATUS = 13;

// ============================================================
// Cancha RCJ Soccer Open 2026 — convención de ejes canónica
// (X long axis derecha, Y short axis al arco rival)
// ============================================================
constexpr uint16_t FIELD_WIDTH_MM  = 2430;   // eje X (largo)
constexpr uint16_t FIELD_HEIGHT_MM = 1820;   // eje Y (corto)

// Ángulos de montaje físico de los TOFs fijos (mismo en ambos robots).
// CONVENCIÓN (ver localization.cpp::classify_wall): heading 0 = robot mira al
// arco rival (+Y). El ángulo de montaje se SUMA al heading; +90° = izquierda
// del robot (pared -X / WEST), +270° = derecha del robot (pared +X / EAST).
//
// Mapeo CONFIRMADO en banco (2026-05-30, Gustavo):
//   índice [0] = FRENTE     → 0°    (mira +Y, arco rival)
//   índice [1] = ATRÁS      → 180°  (mira -Y)
//   índice [2] = DERECHA    → 270°  (mira +X, pared EAST)
//   índice [3] = IZQUIERDA  → 90°   (mira -X, pared WEST)
//
// ⚠️ CORREGIDO 2026-05-30: antes era {0,180,90,270}, que asignaba el índice 2 a
// izquierda y el 3 a derecha — invertido respecto al hardware real. Con el valor
// viejo, localization cruzaba las lecturas der/izq. Ahora coincide con el mapeo
// físico confirmado (TOF2=derecha, TOF3=izquierda).
//
// Cuando se agreguen los 2 ToF móviles (ver NUM_TOF_MAX abajo), su ángulo es
// DINÁMICO (lo da el mecanismo que los orienta hacia la pelota), así que NO va
// en este array estático.
constexpr uint16_t TOF_MOUNT_ANGLE_DEG[4] = { 0, 180, 270, 90 };

// Umbral default para descarte de outliers en localización
constexpr uint16_t LOCALIZATION_OUTLIER_THRESHOLD_MM = 300;

// ============================================================
// Loop timing del TOP
// ============================================================
constexpr uint32_t IMU_TICK_INTERVAL_MS       = 10;
constexpr uint32_t TOF_TICK_INTERVAL_MS       = 30;
constexpr uint32_t STRATEGY_TICK_INTERVAL_MS  = 10;
constexpr uint32_t MOTORS_SEND_INTERVAL_MS    = 10;

// Watchdogs
constexpr uint32_t ZIRCON_HEARTBEAT_TIMEOUT_MS = 500;
constexpr uint32_t DOWN_HEARTBEAT_TIMEOUT_MS   = 500;

// ============================================================
// Cantidad de slots TOF — actual (4 fijos) y plan de escalado (6)
// ============================================================
// NUM_TOF es el tamaño del array de slots TOF físicos HOY: 4 ToF FIJOS,
// uno por orientación (frontal/trasero/izquierdo/derecho), todos en `Wire`
// con LP individual (confirmados enumerando en banco 2026-05-30). Cada
// slot puede estar populated o no según ROBOT_HAS_TOF_*; para iterar solo
// los activos, usar NUM_TOF_ACTIVE del pinout_robotN.h.
constexpr int NUM_TOF = 4;

// PLAN DE ESCALADO A 6 ToF (diseño objetivo, NO cableado todavía):
//   • 4 ToF FIJOS (los actuales) -> localización 2D por trilateración.
//   • 2 ToF MÓVILES adicionales -> montados sobre un mecanismo que los
//     orienta para UBICAR LA PELOTA (complemento de la cámara). Su ángulo
//     es dinámico (no entra en TOF_MOUNT_ANGLE_DEG[]).
// Cuando se agreguen, los 2 móviles cuelgan del MISMO bus `Wire` con su
// propio pin LP cada uno y se enumeran a 0x2E/0x2F (siguen la secuencia de
// TOF_I2C_ADDR_ASSIGNED). Para activarlos: subir NUM_TOF a 6, extender
// PIN_TOF_XSHUT[] y TOF_I2C_ADDR_ASSIGNED[] a 6 en pinout_robotN.h, y
// agregar el manejo del mecanismo móvil. El bus tiene direcciones de sobra
// (0x29..0x77 libres salvo 0x28 del BNO).
constexpr int NUM_TOF_MAX = 6;   // objetivo: 4 fijos + 2 móviles (pelota)

}  // namespace iitasoccer
