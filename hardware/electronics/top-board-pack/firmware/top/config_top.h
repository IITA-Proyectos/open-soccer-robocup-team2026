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
//   El 2º BNO055 está en los pines 24/25 del Teensy 4.0. Corrección 2026-06-09: el bus
//   físico de 24/25 es **Wire2 (LPI2C4)**, NO Wire1 — el repo confundía 24/25 con Wire1.
//   Wire1 real (LPI2C3) son los pines 16/17, ocupados por Serial4 → queda vacío.
//   El firmware DEBE fijar SDA/SCL del bus de 24/25 ANTES de begin().
//   Pendiente confirmar físicamente con TASK-003.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// ============================================================
// I2C buses
// ============================================================
// ⚠️ COPIA CONGELADA — corregido 2026-06-09 para consistencia con la fuente canónica
//    (hardware/electronics/top-board-pack/01-pinout-y-hardware.md). El firmware VIVO está
//    en software/teensy/Soccer 2026/src/. No flashear desde acá.
//
// CORRECCIÓN 2026-06-09 (i2c scan en banco, commit 9da8e9e): el Teensy 4.0 tiene 3 buses
// I²C — Wire (LPI2C1, 18/19) · Wire1 (LPI2C3, 16/17) · Wire2 (LPI2C4, 24/25). El bus físico
// de los pines 24/25 es **Wire2**, NO Wire1; el repo confundía 24/25 con Wire1. Los 2 BNO055
// van en buses SEPARADOS (0x28 ambos): el PRIMARIO (más confiable, sin ToF) está solo en
// Wire2 (24/25); el SECUNDARIO comparte Wire (18/19) con los ToF (se congela al chocar con
// ellos). Las constantes de abajo conservan el nombre histórico WIRE1_* (la instancia se
// llamaba Wire1 por el error) pero apuntan al bus físico Wire2 (pines 24/25).
//
// Wire   (LPI2C1) → pines default 18 (SDA0) / 19 (SCL0)
//                  Bus: BNO055 SECUNDARIO (U10) + los 4 ToF
//
// Wire2  (LPI2C4) → pines 24 (SCL2) / 25 (SDA2)  ← era llamado "Wire1" por error
//                  Bus: BNO055 PRIMARIO (U11), solo en su bus, sin ToF
constexpr int WIRE1_SCL_PIN = 24;   // Wire2 (LPI2C4) SCL — nombre histórico
constexpr int WIRE1_SDA_PIN = 25;   // Wire2 (LPI2C4) SDA — nombre histórico

// Direcciones I2C (ambos BNO055 comparten 0x28 — uno por bus.)
// LEFT = SECUNDARIO (Wire, 18/19, con los ToF) · RIGHT = PRIMARIO (Wire2, 24/25, solo).
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
//                            (Wire1 = LPI2C3) por default; al usarlos para Serial4,
//                            Wire1 queda vacío. El 2º BNO va en Wire2 (24/25), no Wire1.
// Serial5 (RX=21, TX=20)  → conector U9  "UART-CAMERA2"  → OpenMV cámara 2.
// Serial6 (RX=25, TX=24)  → BLOQUEADO por Wire2 remap (pines 24/25 = BNO055 primario;
//                            corrección 2026-06-09, antes decía Wire1).
// Serial7 (RX=28, TX=29)  → disponible para expansion futura.
constexpr long UART_FROM_DOWN_BAUD    = 230400;   // Serial1
constexpr long UART_TO_ZIRCON_BAUD    = 230400;   // Serial2 — TENTATIVO, confirmar U1 wiring
constexpr long UART_CAMERA1_BAUD      = 19200;    // Serial3 — protocolo viejo OpenMV
constexpr long UART_TO_COMM_BAUD      = 115200;   // Serial4 — placa ESP32-C6
constexpr long UART_CAMERA2_BAUD      = 19200;    // Serial5

// ============================================================
// Sensores ToF (4×)
// ============================================================
// U2, U3 en Wire (LPI2C1) — comparten dirección por default; usar XSHUT para enumerarlos.
// U5, U17 en Wire2 (LPI2C4, 24/25) — idem. (Layout viejo; en banco 2026-05-30 los 4 ToF se
//   movieron todos a Wire. El bus de 24/25 es Wire2, no Wire1 — corrección 2026-06-09.)
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
