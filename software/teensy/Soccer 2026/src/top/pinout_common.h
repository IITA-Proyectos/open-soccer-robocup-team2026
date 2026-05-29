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
// UARTs — pines fijos del Teensy 4.0
// ============================================================
constexpr long UART_FROM_DOWN_BAUD = 230400;   // Serial1
constexpr long UART_TO_ZIRCON_BAUD = 230400;   // Serial2 — TENTATIVO
constexpr long UART_CAMERA1_BAUD   = 19200;    // Serial3
constexpr long UART_TO_COMM_BAUD   = 115200;   // Serial4
constexpr long UART_CAMERA2_BAUD   = 19200;    // Serial5

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

// Ángulos de montaje físico de los 4 TOFs (mismo en ambos robots)
constexpr uint16_t TOF_MOUNT_ANGLE_DEG[4] = { 0, 180, 90, 270 };

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
// Cantidad de slots TOF físicos en la placa (hardware fijo)
// ============================================================
// NUM_TOF es el tamaño del array de slots TOF físicos. Cada slot puede
// estar populated o no según ROBOT_HAS_TOF_*. Para iterar solo los
// activos, usar NUM_TOF_ACTIVE del pinout_robotN.h.
constexpr int NUM_TOF = 4;

}  // namespace iitasoccer
