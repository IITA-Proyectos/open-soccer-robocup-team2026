// sensors_imu.h — Wrapper de los 2 BNO055 de la placa TOP.
//
// Hardware — POR ROBOT (la impl está gateada con #if defined(ROBOT2)):
//   ROBOT1 (recableado 2026-05-31, banco diag_bno_addr_check):
//     BNO055 LEFT  → Wire (18/19) @ 0x28 (ADR flotante)
//     BNO055 RIGHT → Wire (18/19) @ 0x29 (ADR a 3V3) — unidad FALLADA, se saltea.
//     OJO: 0x29 es también la dir de fábrica de los ToF; se enumeran a 0x2A..0x2D.
//   ROBOT2 (banco 2026-06-09): 2 BNO en BUSES SEPARADOS, ambos @ 0x28:
//     idx 0 = PRIMARIO   → Wire2 (24/25 nativos, LPI2C4) — SOLO en su bus, sin ToF
//             → sin contención i2c → NO se congela. Es la fuente preferida.
//     idx 1 = SECUNDARIO → Wire (18/19) — comparte bus con los 4 ToF. Respaldo.
//     (Corrección 2026-06-09: 24/25 = Wire2, NO "Wire1"; Wire1 real = 16/17.)
//     En los getters/diag, "LEFT"=idx0=PRIMARIO y "RIGHT"=idx1=SECUNDARIO en R2.
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

// Fuerza guardar el perfil de calibración de cada BNO en EEPROM (Capa 2).
// Solo guarda el de un chip si ya está fully-calibrated. Devuelve true si
// guardó al menos uno. El tick también auto-guarda solo al detectar calib.
bool sensors_imu_save_calibration();

// Diagnóstico:
bool sensors_imu_left_ready();
bool sensors_imu_right_ready();

// Validez del heading EN VIVO para el flag heading_valid del WorldSnapshot (bit4).
// Refleja la fusión (false si ningún sensor utilizable en runtime), no el readiness
// al boot. Byte-idéntico a (_left_ready||_right_ready) en operación normal. (Audit R1.)
bool sensors_imu_get_heading_valid();

}  // namespace iitasoccer
