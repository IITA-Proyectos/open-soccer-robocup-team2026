// config_down.h — Pinout y constantes del firmware de la placa DOWN
// (placa base con 32 sensores de luz + 2 SparkFun OTOS, Teensy 4.0 U7)
//
// Pinout EXTRAÍDO del schematic 04-12 (PCB_PCB_Roboliga_2026_Futbol_2026-04-12.json)
// y VALIDADO EMPÍRICAMENTE 2026-05-24 con la placa física en el banco — los 4 muxes
// leen los 32 sensores correctamente, verdict 0 muertos. Ver:
//   - hardware/electronics/down-board-pack/01-pinout-y-posiciones.md (doc canónico)
//   - journal/2026-05-24-hardware-up-down-anillo-linea.md (hardware-up con datos)
//
// Hallazgo importante: cada CD4051 tiene SUS PROPIOS 3 pines de selección A/B/C
// (NO compartidos como decía un doc viejo). Total: 12 pines SEL + 4 pines ADC.
// Los pines INH están atados a GND físico en el PCB — el firmware NO los controla.
//
// Flags de compilación (defaults conservadores para evitar lecturas al aire):
//   -DDOWN_NUM_MUXES_CONNECTED=1  → solo mux U4 (8 sensores, placa degradada).
//   -DDOWN_NUM_MUXES_CONNECTED=4  → modo full, los 4 muxes (32 sensores). [validado]
//   -DDOWN_NUM_OTOS_CONNECTED=1   → solo el OTOS U5 (Wire).
//   -DDOWN_NUM_OTOS_CONNECTED=2   → ambos OTOS (U5 + U6).

#pragma once
#include <stdint.h>
#include <Arduino.h>   // A0..A6 (macros de pin del core Teensy) usados abajo

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
// Pines correctos según schematic: A0/A1/A8/A9 (NO A2/A3 — esos van a SCL2/SDA2 del OTOS U6).
// Validado empíricamente 2026-05-24: con A2/A3 los muxes U3+U4 leían 1023 sólido (ADC flotante
// con pull-ups I²C). Con A8/A9 → ver test plan al final del archivo.
constexpr int PIN_MUX_OUT[4] = { A0, A1, A8, A9 };

// Selectores A/B/C de cada mux — NO compartidos. 3 pines por mux × 4 muxes = 12 pines.
// Pines correctos según schematic 2026-04-12 (extract_pinout_from_schematic.py).
// Validado empíricamente 2026-05-24: con SEL compartidos {2,3,4} los 8 canales de U3+U4
// daban todos el mismo valor (solo 1 de cada 8 sensores se leía). Con SEL por mux,
// los 8 canales rotan correctamente.
constexpr int PIN_MUX_A[4] = { 13, 4, 7, 10 };  // SEL_A de U1, U2, U3, U4
constexpr int PIN_MUX_B[4] = {  2, 5, 8, 11 };  // SEL_B de U1, U2, U3, U4
constexpr int PIN_MUX_C[4] = {  3, 6, 9, 12 };  // SEL_C de U1, U2, U3, U4

// Mapeo canal_del_mux → sensor_lógico (scrambling de Enzo, mismo patrón los 4 muxes).
// Para leer el sensor i (i=0..7) del mux m: ch = MUX_CH_FOR_SENSOR[i].
constexpr uint8_t MUX_CH_FOR_SENSOR[8] = { 3, 0, 1, 2, 5, 7, 6, 4 };

// INH (Enable) de los 4 muxes está atado a GND fijo en el PCB — el firmware NO
// lo controla. Por eso se eliminó el array PIN_MUX_INH[] anterior.

// ⚠️ PIN_LED_STATUS = 13 colisiona físicamente con PIN_MUX_A[0]. Es así en el PCB
// (Enzo compartió ambas funciones en el mismo pin). El LED parpadea con SEL_A de
// U1 — inevitable, no afecta la lectura del ADC.

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
