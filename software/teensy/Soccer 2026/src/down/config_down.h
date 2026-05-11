// config_down.h — Pinout y constantes del firmware de la placa DOWN
// (placa base con 32 sensores de luz + 2 SparkFun OTOS, Teensy 4.0 U7)
//
// El pinout se infiere del schematic 04-12 (PCB_PCB_Roboliga_2026_Futbol_2026-04-12.json).
// 7 de los pads del Teensy 4.0 en la placa DOWN son entradas analógicas (A0-A6) y
// se usan para las salidas O1-O4 de los CD4051. Las señales E1-E12 controlan A/B/C
// de los muxes y los INH (enable) individuales.
//
// ⚠️ HARDWARE STATUS (auditoria PCB 04-12, 2026-05-10):
//   En el PCB ruteado solo aparecen tracks para: O4, SDA2, SCL2, RX5, TX5, RX1, TX1.
//   FALTAN tracks para: O1, O2, O3, SDA1, SCL1, E1, E6, E7, E11.
//   Eso significa que SI las placas físicas que llegaron son del 04-12, solo el
//   mux U4 (8 sensores) y uno de los dos OTOS funcionan. Ver TASK-001 / TASK-009.
//
//   Para soportar ambas situaciones (placa actual con bug o placa fixeada),
//   usamos los siguientes flags de compilación:
//     -DDOWN_NUM_MUXES_CONNECTED=1  → modo degradado, solo mux U4 (8 sensores).
//     -DDOWN_NUM_MUXES_CONNECTED=4  → modo full, los 4 muxes (32 sensores).
//     -DDOWN_NUM_OTOS_CONNECTED=1   → solo el OTOS de SDA2/SCL2.
//     -DDOWN_NUM_OTOS_CONNECTED=2   → ambos OTOS (placa fixeada).
//   Defaults conservadores: 1 mux + 1 OTOS (lo que funciona seguro hoy).

#pragma once
#include <stdint.h>

namespace iitasoccer {

// ============================================================
// Compile-time configuration de la placa física
// ============================================================
#ifndef DOWN_NUM_MUXES_CONNECTED
    #define DOWN_NUM_MUXES_CONNECTED 1   // default: solo mux U4 (placa 04-12)
#endif

#ifndef DOWN_NUM_OTOS_CONNECTED
    #define DOWN_NUM_OTOS_CONNECTED 1    // default: solo 1 OTOS (placa 04-12)
#endif

constexpr int NUM_SENSORS_PER_MUX = 8;
constexpr int NUM_LINE_SENSORS    = DOWN_NUM_MUXES_CONNECTED * NUM_SENSORS_PER_MUX;
constexpr int NUM_OTOS            = DOWN_NUM_OTOS_CONNECTED;

// ============================================================
// Pinout del Teensy 4.0 en la placa DOWN
// (inferido del schematic 04-12 — confirmar contra fabricación real / TASK-009)
// ============================================================

// Entradas analógicas de los 4 muxes (O1..O4 del schematic).
// Solo O4 está ruteado en el PCB 04-12 confirmado. Los demás son tentativos.
constexpr int PIN_MUX_OUT[4] = { A0, A1, A2, A3 };

// Líneas de selección compartidas A, B, C entre los 4 muxes.
// Mapeo tentativo (a confirmar): E10=C, E11=B, E12=A según el schematic.
// Asignaciones específicas del Teensy: pendiente Q3-similar para DOWN.
constexpr int PIN_MUX_SEL_A = 2;
constexpr int PIN_MUX_SEL_B = 3;
constexpr int PIN_MUX_SEL_C = 4;

// Líneas de habilitación individuales por mux (INH activo bajo).
// E5, E6, E7, E8 del schematic.
constexpr int PIN_MUX_INH[4] = { 5, 6, 7, 8 };

// ============================================================
// SparkFun OTOS (I2C dual)
// ============================================================
// OTOS U5 → I2C bus 1 (Wire1 en Teensy 4.0)
// OTOS U6 → I2C bus 2 (Wire2 en Teensy 4.0)
// Ambos comparten dirección I2C por default (no se pueden poner en el mismo bus).
constexpr uint8_t OTOS_I2C_ADDR = 0x17;  // SparkFun default

// Separación física entre los 2 OTOS (mm). Q5 del usuario: "uno a cada costado".
// Confirmar con TASK-004 (montaje físico real).
constexpr float OTOS_SEPARATION_MM = 200.0f;  // tentativo: 10cm desde el centro a cada lado

// ============================================================
// UART hacia TOP
// ============================================================
// Serial5 (Teensy 4.0 pines 20=RX, 21=TX) — conector U10 "COMUNICATION" del schematic.
constexpr long UART_TOP_BAUD = 230400;

// ============================================================
// LED de estado
// ============================================================
constexpr int PIN_LED_STATUS = 13;  // LED_BUILTIN

// ============================================================
// Loop timing
// ============================================================
constexpr uint32_t LINE_TICK_INTERVAL_US   = 1000;   // 1 kHz — línea es lo más urgente
constexpr uint32_t OTOS_TICK_INTERVAL_MS   = 10;     // 100 Hz
constexpr uint32_t COMM_SEND_INTERVAL_MS   = 10;     // 100 Hz envío al TOP

// ============================================================
// Calibración línea (umbrales)
// ============================================================
// El ALS-PT19 entrega un valor analógico que sube cuando ve más luz reflejada.
// Carpet verde → bajo (~100-300 counts en 10-bit ADC).
// Línea blanca → alto (~600-900 counts).
constexpr uint16_t LINE_DEFAULT_THRESHOLD = 500;  // calibrar con calibrate_white / calibrate_carpet

}  // namespace iitasoccer
