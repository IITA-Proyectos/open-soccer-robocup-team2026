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

// Recableado 2026-05-31 (confirmado en banco con diag_bno_addr_check):
// los 2 BNO055 quedaron en el MISMO bus `Wire` (18/19), en direcciones
// distintas. RIGHT tiene su pad ADR puenteado a 3V3 -> 0x29. Esto LIBERA
// `Wire1` (24/25) para la comunicación con la placa DOWN.
// OJO: 0x29 lo comparten el BNO RIGHT y la dirección de fábrica de los ToF;
// por eso los ToF se enumeran a 0x2A..0x2D al boot (ver topología ToF abajo)
// y nunca quedan en 0x29 una vez enumerados.
constexpr uint8_t BNO055_LEFT_I2C_ADDR  = 0x28;
constexpr uint8_t BNO055_RIGHT_I2C_ADDR = 0x29;  // ADR puenteado a 3V3
constexpr uint8_t VL53L7CX_DEFAULT_I2C_ADDR = 0x29;  // de fábrica; se reasigna al enumerar

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
constexpr long UART_TO_ZIRCON_BAUD = 230400;   // Serial4 (RX16/TX17) → CENTRAL/Zircon
                                               // (FIX 2026-06-02: el Teensy 4.0 NO expone
                                               // Serial7 28/29 en el borde — back-pads. El enlace
                                               // a CENTRAL va por Serial4 (Serial2 7/8 = COMM).
                                               // Cable: TOP pin 17 TX4 → CENTRAL pin 28 RX7 del 4.1.)
constexpr long UART_CAMERA1_BAUD   = 19200;    // Serial3
constexpr long UART_TO_COMM_BAUD   = 115200;   // Serial2 (RX7/TX8) ↔ COMM (ESP32-C6)
constexpr long UART_CAMERA2_BAUD   = 19200;    // Serial5 (cámara trasera, soldada pin 21)

// ============================================================
// HC-SR04 ultrasonido frontal (idéntico ambos robots)
// Cableado en banco 2026-05-31: TRIG=pin 4, ECHO=pin 3 (pines ex-XSHUT ToF, hoy
// libres). Antes 6/7. Los pines 3/4 NO son UART → sin conflicto con ningún Serial.
// ============================================================
constexpr int PIN_HCSR04_TRIG = 4;
constexpr int PIN_HCSR04_ECHO = 3;

// ============================================================
// LED de estado (LED_BUILTIN del Teensy)
// ============================================================
constexpr int PIN_LED_STATUS = 13;

// ============================================================
// Cancha RCJ Soccer Open 2026 — convención de ejes canónica
// (ver docs/CONVENCION-EJES-ROBOT.md §2). Medidas PARED-A-PARED (las que ven
// los ToF): cancha de juego 2190 x 1580 mm (línea blanca) + 12 cm de outer area
// por lado = 2430 x 1820 pared-a-pared. +Y va al arco rival = lado LARGO
// (arco-a-arco) = 2430; +X = lateral = lado CORTO = 1820.
// ⚠️ CORREGIDO 2026-06-03: antes WIDTH=2430/HEIGHT=1820 (eje +Y al arco con el
// valor del lado corto) — INVERTIDO respecto al reglamento. classify_wall usa
// field_height para la pared del arco rival (+Y), que es el LARGO (2430).
// ============================================================
constexpr uint16_t FIELD_WIDTH_MM  = 1820;   // eje X (lateral, lado CORTO)
constexpr uint16_t FIELD_HEIGHT_MM = 2430;   // eje Y (arco-a-arco, lado LARGO)

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

// Radio del robot para F1a: distancia del plano de los sensores ToF al CENTRO
// geométrico del robot, en mm. Se SUMA a la lectura cruda del ToF antes de
// proyectar (los sensores están en el borde, no en el centro). Con 1 solo
// sensor por eje esta corrección es crítica (no hay opuesto que la cancele).
// ⚠️ PLACEHOLDER: medir en HW con cinta (plano-sensor → centro) y validar en
// banco (TASK del equipo) antes de dar este valor por bueno. Típico ~90-100 mm.
constexpr uint16_t TOF_OFFSET_MM = 95;

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
// Guarda (TOPHAL-01, 2026-06-04): los arrays de trilateración en localization.h
// son de tamaño fijo [4] (tof_distance_mm[4], tof_valid[4], tof_mount_angle_deg[4]).
// Subir NUM_TOF sin ampliar esos arrays sería un out-of-bounds silencioso.
static_assert(NUM_TOF == 4,
              "NUM_TOF != 4: ampliá los arrays [4] de localization.h antes de subirlo");

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
