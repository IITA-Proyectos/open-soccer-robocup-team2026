// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
// config_top.h — Pinout y constantes del firmware de la placa TOP (Teensy 4.0 master)
//
// Pinout inferido del schematic 04-12 (PCB_PCB_Roboliga2026_TOP_2026-04-12.json) y
// confirmado con el análisis de tracks (5/5/11/9 para SCL1/SDA1/RX4/TX4).
//
// ⚠️ Hardware decision Q3 (resuelta por análisis PCB, 2026-05-10):
//   Wire1 (I2C bus 1) está RUTEADA a los pines 24/25 del Teensy 4.0 (no a los
//   pines default 16/17, que están ocupados por Serial4). El firmware DEBE
//   llamar Wire1.setSCL(24); Wire1.setSDA(25); ANTES de Wire1.begin().
//   Pendiente confirmar físicamente con TASK-003.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// ============================================================
// I2C buses
// ============================================================
// Wire   (I2C0) → pines default 18 (SDA0) / 19 (SCL0)
//                  Bus 0: BNO055 U10 + ToF U2 + ToF U3
//
// Wire1  (I2C1) → REMAPEADO a pines 24 (SCL1) / 25 (SDA1)
//                  Bus 1: BNO055 U11 + ToF U5 + ToF U17
constexpr int WIRE1_SCL_PIN = 24;
constexpr int WIRE1_SDA_PIN = 25;

// Direcciones I2C (ambos BNO055 comparten 0x28 — uno por bus.)
constexpr uint8_t BNO055_LEFT_I2C_ADDR  = 0x28;
constexpr uint8_t BNO055_RIGHT_I2C_ADDR = 0x28;

// ============================================================
// UARTs (Teensy 4.0 hardware serials — pinout oficial PJRC)
// ============================================================
// Referencia: https://www.pjrc.com/teensy/td_uart.html
//
// Serial1 (RX=0,  TX=1 )  → conector U16 "UART_COMM_IN"  → desde placa DOWN.
// Serial2 (RX=7,  TX=8 )  → tentativo: hacia placa motores Zircon (conector U1).
//                            ⚠️ NO CONFIRMADO — el schematic del TOP muestra
//                            U1 con OUT1/OUT2/RX_OUT/TX_OUT sin nombre claro
//                            de UART. Falta validar con Enzo a qué pines del
//                            Teensy 4.0 van RX_OUT y TX_OUT del conector U1.
// Serial3 (RX=15, TX=14)  → conector U8  "UART-CAMERA1"  → OpenMV cámara 1.
// Serial4 (RX=16, TX=17)  → conector U15 "UART_COMM_OUT" → hacia placa COMM.
//                            ⚠️ Recordar Q3: pines 16/17 son también SCL1/SDA1
//                            por default; usamos Wire1 con REMAP a 24/25 para
//                            liberar Serial4 (ver sección Wire1 arriba).
// Serial5 (RX=21, TX=20)  → conector U9  "UART-CAMERA2"  → OpenMV cámara 2.
// Serial6 (RX=25, TX=24)  → BLOQUEADO por Wire1 remap (pines 24/25 usados).
// Serial7 (RX=28, TX=29)  → disponible para expansion futura.
constexpr long UART_FROM_DOWN_BAUD    = 230400;   // Serial1
constexpr long UART_TO_ZIRCON_BAUD    = 230400;   // Serial2 — TENTATIVO, confirmar U1 wiring
constexpr long UART_CAMERA1_BAUD      = 19200;    // Serial3 — protocolo viejo OpenMV
constexpr long UART_TO_COMM_BAUD      = 115200;   // Serial4 — placa ESP32-C6
constexpr long UART_CAMERA2_BAUD      = 19200;    // Serial5

// ============================================================
// Sensores ToF (4×)
// ============================================================
// U2, U3 en Wire (I2C0) — comparten dirección por default; usar XSHUT para enumerarlos.
// U5, U17 en Wire1 (I2C1) — idem.
// Hardware comprado (según coach Q4): VL53L7CX disponibles, VL53L5CX en pedido.
// Direcciones después de enumeración: 0x52, 0x54, 0x56, 0x58 (tentativas).
constexpr int NUM_TOF = 4;

// Pines XSHUT (Xshut del schematic — GPIOs para encender/apagar cada ToF
// individualmente durante enumeración I2C). Tentativos, confirmar con TASK-003 ext.
constexpr int PIN_TOF_XSHUT[NUM_TOF] = { 2, 3, 4, 5 };

// ============================================================
// HC-SR04 ultrasonido (frontal, fallback)
// ============================================================
constexpr int PIN_HCSR04_TRIG = 6;
constexpr int PIN_HCSR04_ECHO = 7;

// ============================================================
// LED de estado
// ============================================================
constexpr int PIN_LED_STATUS = 13;

// ============================================================
// Selección de rol del robot (Q7: dipswitch al boot)
// ============================================================
// Pin con pull-up: LOW = arquero, HIGH = delantero.
// Se lee 1 vez en setup() y se mantiene fijo (luego podrá cambiar
// dinámicamente vía mensaje del coach/COMM).
constexpr int PIN_ROLE_DIPSWITCH = 10;

// ============================================================
// Loop timing del TOP
// ============================================================
constexpr uint32_t IMU_TICK_INTERVAL_MS       = 10;   // 100 Hz
constexpr uint32_t TOF_TICK_INTERVAL_MS       = 30;   // ~30 Hz (ToF lentos)
constexpr uint32_t STRATEGY_TICK_INTERVAL_MS  = 10;   // 100 Hz
constexpr uint32_t MOTORS_SEND_INTERVAL_MS    = 10;   // 100 Hz al Zircon

// Watchdog: si no llega mensaje del Zircon en este tiempo, alerta degradación.
constexpr uint32_t ZIRCON_HEARTBEAT_TIMEOUT_MS = 500;
constexpr uint32_t DOWN_HEARTBEAT_TIMEOUT_MS   = 500;

}  // namespace iitasoccer
