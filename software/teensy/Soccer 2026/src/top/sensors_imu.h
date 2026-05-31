// sensors_imu.h — Wrapper de los 2 BNO055 de la placa TOP.
//
// Hardware (recableado 2026-05-31, confirmado en banco con diag_bno_addr_check):
//   BNO055 LEFT  → Wire (I2C bus 0, pines 18/19) @ 0x28 (ADR a GND/flotante)
//   BNO055 RIGHT → Wire (I2C bus 0, pines 18/19) @ 0x29 (ADR puenteado a 3V3)
//
// AMBOS en el MISMO bus, en direcciones distintas. Esto LIBERA Wire1 (24/25)
// para la comunicación con la placa DOWN. (Antes RIGHT vivía en Wire1 @ 0x28;
// el recableado lo movió.) OJO: 0x29 es también la dir de fábrica de los ToF;
// por eso los ToF se enumeran a 0x2A..0x2D al boot y no colisionan con RIGHT.
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
// Estrategia dual (FUSIÓN):
//   El heading principal (get_heading_deg) es el promedio CIRCULAR de ambos
//   sensores cuando los 2 están OK y de acuerdo (< 30° de diferencia). Si
//   discrepan (impacto/falla), usa LEFT como referencia. Con 1 solo BNO, usa
//   ese. El desacuerdo queda visible vía get_disagreement_deg() para diagnóstico.

#pragma once
#include <stdint.h>

namespace iitasoccer {

bool sensors_imu_init();

// Actualiza lecturas de ambos IMUs. Llamar a 100 Hz desde el loop.
void sensors_imu_tick();

// Heading principal del robot en [-180, +180] grados, convención CCW-positiva
// (girar a la IZQUIERDA sube el heading). Es el promedio CIRCULAR de los 2 BNO
// cuando ambos están OK y de acuerdo; degrada a 1 BNO; 0 si ambos fallan.
float sensors_imu_get_heading_deg();

// Headings individuales de cada BNO (diagnóstico / fusión externa). Mismas
// unidades y convención que get_heading_deg(). 0 si el sensor no está listo.
float sensors_imu_get_left_heading_deg();
float sensors_imu_get_right_heading_deg();

// Mismo heading que sensors_imu_get_heading_deg() pero en centidegrees
// (centesimas de grado). Rango [-18000, +18000].
// Wrapper conveniente para modulos que prefieren enteros (ej. localization).
int16_t sensors_imu_get_heading_centideg();

// Diferencia entre los 2 IMUs (sanity check). > 5° indica problema.
float sensors_imu_get_disagreement_deg();

// Recalibra el heading inicial (zero offset) usando el IMU que esté OK.
void sensors_imu_recalibrate_zero();

// Diagnóstico:
bool sensors_imu_left_ready();
bool sensors_imu_right_ready();

}  // namespace iitasoccer
