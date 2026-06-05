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
// Flags de compilación. Los #define de abajo son sólo FALLBACK por si un env no
// pasa -D; la PLACA DE COMPETENCIA compila con 4 muxes + 2 OTOS (ver más abajo y
// platformio.ini [env:down] / [env:down_debug] / [env:diag_down], que setean
// -DDOWN_NUM_MUXES_CONNECTED=4 y -DDOWN_NUM_OTOS_CONNECTED=2 desde 2026-05-31):
//   -DDOWN_NUM_MUXES_CONNECTED=1  → fallback: solo mux U1 (8 sensores, placa degradada).
//   -DDOWN_NUM_MUXES_CONNECTED=4  → PRODUCCIÓN: los 4 muxes (32 sensores). [validado 2026-05-24]
//   -DDOWN_NUM_OTOS_CONNECTED=1   → fallback: solo el OTOS U5 (Wire).
//   -DDOWN_NUM_OTOS_CONNECTED=2   → PRODUCCIÓN: ambos OTOS (U5 + U6).

#pragma once
#include <stdint.h>
#include <Arduino.h>   // A0..A6 (macros de pin del core Teensy) usados abajo

namespace iitasoccer {

// ============================================================
// Compile-time configuration de la placa física
// ============================================================
#ifndef DOWN_NUM_MUXES_CONNECTED
    // FALLBACK si ningún env pasa -D. PRODUCCIÓN compila =4 (ver platformio.ini [env:down]).
    #define DOWN_NUM_MUXES_CONNECTED 1
#endif

#ifndef DOWN_NUM_OTOS_CONNECTED
    // FALLBACK si ningún env pasa -D. PRODUCCIÓN compila =2 (ver platformio.ini [env:down]).
    #define DOWN_NUM_OTOS_CONNECTED 1
#endif

constexpr int NUM_SENSORS_PER_MUX = 8;
constexpr int NUM_LINE_SENSORS    = DOWN_NUM_MUXES_CONNECTED * NUM_SENSORS_PER_MUX;
constexpr int NUM_OTOS            = DOWN_NUM_OTOS_CONNECTED;

// ============================================================
// Pipeline de filtros de line_ring (gate de CPU)
// ============================================================
// El pipeline procesado de line_ring_tick() (temporal ×32, hysteresis ×32,
// spatial, centroid, lifted) computa g_angle_deg / g_depth / g_imminent_exit /
// g_lifted / g_sensor_white_validated. En COMPETENCIA esas salidas NADIE las
// consume: el LineStatusV2 que va a CENTRAL se computa entero en dm_update()
// (DownModel) desde la lectura CRUDA (line_ring_get_raw). Los únicos lectores de
// las salidas procesadas son los diags de banco (main_diag_down.cpp,
// diag_down_calibracion.cpp).
//
// LINE_RING_PROCESS gobierna si line_ring_tick() corre ese pipeline. Lo dejamos
// DEFAULT ON para que TODOS los envs actuales (competencia Y diags) sean
// BYTE-IDÉNTICOS al binario previo — sin riesgo de romper los diags, que NO
// pasan -DLINE_RING_PROCESS pero igual lo reciben por este default.
//
// Para realizar el ahorro de CPU en competencia (saltar el pipeline muerto),
// pasar -DDOWN_LEAN_LINE_PIPELINE en [env:down]/[env:down_debug] de
// platformio.ini. Es un OPT-OUT explícito (DEFAULT NO PASADO): mientras no se
// agregue, el binario de competencia no cambia. Los envs de diag NUNCA deben
// pasarlo (siguen leyendo las salidas procesadas). El MUESTREO CRUDO y los
// timestamps (g_last_sample_us / g_tick_count) corren SIEMPRE, con o sin gate.
#if !defined(LINE_RING_PROCESS) && !defined(DOWN_LEAN_LINE_PIPELINE)
    #define LINE_RING_PROCESS
#endif

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
// OTOS U5 → Wire  (I2C bus 0, SDA=18 / SCL=19)
// OTOS U6 → Wire1 (I2C bus 1, SDA=17 / SCL=16)
// (Pinout validado en otos.cpp — corregido 2026-05-24: antes decía "Wire2/I2C2",
//  era un typo viejo; el Teensy 4.0 ni siquiera expone Wire2 en estos pines.)
// Ambos comparten dirección I2C por default (no se pueden poner en el mismo bus).
constexpr uint8_t OTOS_I2C_ADDR = 0x17;  // SparkFun default

// Separación física entre los 2 OTOS (mm). Q5 del usuario: "uno a cada costado".
// ⚠️ SIN VALIDAR (número tentativo) — medir el real y cerrar TASK-004 / TASK-029 test2.
// El HEADING dual NO depende de esta separación: g_heading_deg es el promedio
// VECTORIAL de los headings ABSOLUTOS de las 2 IMUs (fuse_dual_heading_deg en
// otos.cpp ~l.142), no una derivada geométrica de Δposición/separación. La
// separación entra SOLO en el slip_estimate (otos_slip_estimate, otos.cpp ~l.152,
// usa OTOS_SEPARATION_MM*0.5 como radio para descontar la componente de rotación).
// Por lo tanto, un error en este valor afecta el slip, NO el heading. Con la
// config de 1 OTOS este valor no se usa (slip = 0).
constexpr float OTOS_SEPARATION_MM = 200.0f;  // tentativo: 10cm desde el centro a cada lado

// ============================================================
// UART hacia TOP
// ============================================================
// Serial5 (Teensy 4.0 pines 20=TX5, 21=RX5) — conector U10 "COMUNICATION" del schematic.
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
