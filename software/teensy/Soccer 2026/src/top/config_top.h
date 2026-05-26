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
// ⚠️ TOP rev 1.0: SOLO 2 ToFs SOPORTADOS SIN REWORK
// Hallazgo forense 2026-05-25: los 4 pines XSHUT/LPn de los slots
// U2/U3/U5/U17 están INTENCIONALMENTE SIN CONECTAR en el PCB rev 1.0
// (NC flags explícitos en el schematic, sin nets en el PCB netlist).
// Sin XSHUT individual no se pueden enumerar 2 sensores en el mismo bus
// (ambos arrancan en 0x29 → colisión I²C).
//
// Configuración soportada SIN rework: 1 ToF por bus
//   - U2 en Wire (I²C0) — frontal, ya validado 2026-05-24.
//   - U5 en Wire1 (I²C1) — pendiente activar (ver TASK-033).
// Para los 4 ToFs hay que: (a) bodge soldando jumpers Xshut → GPIOs
// libres del Teensy, o (b) esperar TOP rev 1.1 con XSHUT ruteados.
//
// Ver:
//   - journal/2026-05-25-top-xshut-no-routed-hallazgo-forense.md
//   - research/in-progress/2026-05-25-top-board-rev-1.1-wishlist.md
//   - team-tasks/2026-05-25-task-033-decidir-cuantos-tofs-incheon.md
//
// La línea PIN_TOF_XSHUT[] de abajo queda como histórica/aspiracional.
// El código actual de sensors_tof.cpp (migrado a Adafruit 2026-05-24) ya
// no la usa — opera con 1 sensor en 0x29 default sin XSHUT.
// ============================================================
// U2, U3 en Wire (I2C0) — comparten dirección por default; usar XSHUT para enumerarlos.
// U5, U17 en Wire1 (I2C1) — idem.
// Hardware comprado (según coach Q4): VL53L7CX disponibles, VL53L5CX en pedido.
// Direcciones después de enumeración: 0x52, 0x54, 0x56, 0x58 (tentativas).
constexpr int NUM_TOF = 4;

// ⚠️ HISTÓRICO/ASPIRACIONAL — estos pines NO están ruteados en TOP rev 1.0.
// NO USAR como pinout de XSHUT real. Ver banner arriba.
// El firmware vivo (sensors_tof.cpp con lib Adafruit_VL53L7CX) no
// referencia este array — se conserva para trazabilidad del diseño
// original y para reuso en TOP rev 1.1 cuando los XSHUT estén ruteados.
constexpr int PIN_TOF_XSHUT[NUM_TOF] = { 2, 3, 4, 5 };  // ⚠️ NO USAR — ver banner arriba.

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

// ============================================================
// Localización en cancha RCJ Soccer Open 2026
// ============================================================
// Dimensiones interiores de la cancha (rulebook 2026).
constexpr uint16_t FIELD_WIDTH_MM  = 2430;   // eje X (largo)
constexpr uint16_t FIELD_HEIGHT_MM = 1820;   // eje Y (corto)

// Ángulos de montaje de los 4 TOFs respecto al frente del robot (grados).
// Convención: 0 = frente, 90 = izquierda, 180 = atrás, 270 = derecha.
// Mapeo a índices del array PIN_TOF_XSHUT / sensors_tof_get_distance_mm:
//   [0] = frontal, [1] = trasero, [2] = izquierdo, [3] = derecho
constexpr uint16_t TOF_MOUNT_ANGLE_DEG[NUM_TOF] = { 0, 180, 90, 270 };

// Umbral default para descarte de outliers por inconsistencia entre TOFs
// del mismo eje. Si 2 TOFs estiman X (o Y) y difieren más que este valor,
// se descarta el más lejano del pose anterior.
constexpr uint16_t LOCALIZATION_OUTLIER_THRESHOLD_MM = 300;  // 30 cm

}  // namespace iitasoccer
