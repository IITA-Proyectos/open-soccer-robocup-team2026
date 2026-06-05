// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
// sensors_imu.h — Wrapper de los 2 BNO055 de la placa TOP.
//
// Hardware (schematic 04-12):
//   U10 (BNO055 #1) → Wire   (I2C bus 0, pines 18/19 default del Teensy 4.0)
//   U11 (BNO055 #2) → Wire1  (I2C bus 1, REMAPEADO a pines 24/25 — ver config_top.h)
//
// Ambos BNO055 comparten dirección I2C 0x28 por default. Separados en 2 buses
// para evitar colisión.
//
// Lecciones aplicadas de docs/internal/giroscopo-bno055-analisis-tecnico.md:
//   • Modo IMUPLUS (acel + gyro, sin magnetómetro) → evita interferencia magnética
//     de motores.
//   • Espera estabilización 1000ms post-init.
//   • Espera calibración del gyro hasta 2000ms (con timeout, no bloquea forever).
//   • Promedio de 10 lecturas para captura del heading inicial.
//   • Degradación elegante: si un BNO055 falla, retorna false en su _ready() pero
//     el firmware sigue con el otro.
//   • NUNCA `while(1)` colgante.
//
// Estrategia dual:
//   Ambos sensores deberían reportar headings similares. Si difieren mucho, hay
//   sospecha de problema (impacto, interferencia, falla). El firmware reporta
//   el heading del IMU con mejor calibración, o el promedio si ambos están OK.

#pragma once
#include <stdint.h>

namespace iitasoccer {

bool sensors_imu_init();

// Actualiza lecturas de ambos IMUs. Llamar a 100 Hz desde el loop.
void sensors_imu_tick();

// Heading principal en [-180, +180] grados. Si el BNO055 LEFT está OK, usa ese.
// Si solo RIGHT está OK, usa RIGHT. Si ambos fallaron, retorna 0.
float sensors_imu_get_heading_deg();

// Diferencia entre los 2 IMUs (sanity check). > 5° indica problema.
float sensors_imu_get_disagreement_deg();

// Recalibra el heading inicial (zero offset) usando el IMU que esté OK.
void sensors_imu_recalibrate_zero();

// Diagnóstico:
bool sensors_imu_left_ready();
bool sensors_imu_right_ready();

}  // namespace iitasoccer
