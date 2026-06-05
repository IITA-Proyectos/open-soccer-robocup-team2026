// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
// imu_zircon.h — Wrapper del BNO055 con init robusto.
//
// Aplica las lecciones de `docs/internal/giroscopo-bno055-analisis-tecnico.md`:
//   - Modo IMUPLUS (acel + gyro, sin magnetómetro) → evita interferencia de motores.
//   - Espera estabilización 1000ms post-init.
//   - Espera calibración del gyro hasta 2000ms (con timeout, no bloquea forever).
//   - Promedio de 10 lecturas para captura del heading inicial.
//   - Degradación elegante: si BNO055 falla, retorna false y `imu_get_heading()`
//     devuelve 0. El motor server sigue funcionando sin corrección de heading.
//
// IMPORTANTE: el problema del análisis ("while(1) si BNO055 no responde") se
// resuelve acá — nunca bloqueamos el firmware del robot por falla del IMU.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Inicializa BNO055 en modo IMUPLUS con secuencia robusta. Retorna true si OK,
// false si el sensor no responde (degradación elegante).
bool imu_init();

// Indica si la inicialización fue exitosa.
bool imu_is_ready();

// Heading actual normalizado a [-180.0, +180.0] grados, relativo al heading
// inicial capturado en imu_init(). Retorna 0.0 si imu_is_ready() == false.
float imu_get_heading();

// Re-captura el heading inicial (para "ajusta tu apuntado al arco rival" del coach).
// Llamar cuando el robot esté estático apuntando a la dirección deseada como 0°.
void imu_recalibrate_heading_zero();

}  // namespace iitasoccer
