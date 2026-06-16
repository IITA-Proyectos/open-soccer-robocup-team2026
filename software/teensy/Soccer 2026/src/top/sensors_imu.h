// sensors_imu.h — Wrapper de los 2 BNO055 de la placa TOP.
//
// Hardware — UNIFICADO PARA AMBOS ROBOTS (corrección 2026-06-15):
//   2 BNO055 en 0x28, en BUSES SEPARADOS (mismo esquema en R1 y R2):
//     idx 0 = PRIMARIO   → Wire2 (24/25 nativos, LPI2C4) — SOLO en su bus, sin ToF
//             → sin contención i2c → NO se congela. Es la fuente preferida.
//     idx 1 = SECUNDARIO → Wire (18/19) — comparte bus con los 4 ToF. Respaldo.
//   NO hay ningún BNO en 0x29: el viejo "RIGHT @ 0x29 (ADR a 3V3)" de robot1 fue un
//   ERROR de cableado, ya corregido en hardware. 0x29 es solo la dir de fábrica de los
//   ToF VL53L7CX, que se reasignan a 0x2A..0x2D al enumerar.
//   (R2 validado en banco 2026-06-09; R1 unificado en firmware 2026-06-15 → falta banco.)
//   (24/25 = Wire2, NO "Wire1"; Wire1 real = 16/17.)
//   En los getters/diag, "LEFT"=idx0=PRIMARIO y "RIGHT"=idx1=SECUNDARIO.
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
#ifdef TOP_ENABLE_HEADING_XVAL
#include "imu_cross_validate.h"   // a FILE SCOPE (el header tiene su propio namespace iitasoccer)
#endif

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

// ── Cross-validación de salud del heading (TASK-213) ──────────────────────────
// Getters de telemetría SIEMPRE presentes (firma sin tipos Arduino). Con el flag
// TOP_ENABLE_HEADING_XVAL OFF, el cuerpo es un return 0/false trivial; como ningún
// call-site los referencia (top_telemetry_serial los llama bajo el mismo #ifdef),
// --gc-sections los descarta → binario de competencia byte-idéntico.
uint8_t sensors_imu_xval_verdict();    // 0=SANO,1=SOSPECHA,2=MALO
uint8_t sensors_imu_xval_score();      // 0..100
uint8_t sensors_imu_xval_n_indep();    // refs independientes válidas
bool    sensors_imu_sentinel_ready();  // el 2º BNO (centinela) se inicializó OK
float   sensors_imu_sentinel_heading_deg();  // último yaw del centinela (2º BNO), CCW+ crudo, @1Hz
float   sensors_imu_get_gyro_z_dps();  // gyro_z del primario, ya leído este tick (CCW+)

#ifdef TOP_ENABLE_HEADING_XVAL
// Estado del cross-validador (lo alimentan el feed de main_top y el centinela de
// sensors_tof; xval_update corre 1x/tick en sensors_imu_tick). XvalState viene del
// include a file-scope de arriba.
XvalState& sensors_imu_xval_state();
// Rotación NETA acumulada del primario (GRADOS, NO deg/s) desde la última llamada;
// la consume el read del centinela @1Hz como gate de ventana-evaluable. Resetea al leer.
float sensors_imu_take_pri_net_rotation_deg();
#endif

#ifdef TOP_ENABLE_BNO_SENTINEL
// Paso del CENTINELA @1Hz: lee el 2º BNO (Wire) y alimenta xval. Lo ejecuta el LOOP
// en la MISMA ventana temporalmente aislada del read del primario (bus-quiet), para
// que el read del secundario NUNCA quede pegado a un getRangingData de los ToF (la
// contención es justo lo que congelaba el BNO). El read encapsula read_raw_yaw +
// g_bno_secondary (file-static de sensors_imu.cpp); el scheduler+timeout son puros
// (imu_cross_validate.h). No hace nada si todavía no toca la ventana 1Hz. ⚠️ BANCO:
// confirmar con analizador lógico que el read del secundario es <10 ms y queda
// aislado en el bus Wire (pre-req TASK-213). Requiere también TOP_ENABLE_HEADING_XVAL.
void sensors_imu_sentinel_step(uint32_t now_ms);
#endif

}  // namespace iitasoccer
