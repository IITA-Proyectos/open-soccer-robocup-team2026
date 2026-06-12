// otos.h — Wrapper de los 2 SparkFun OTOS (Optical Tracking Odometry Sensor)
//
// Los 2 OTOS están montados en los costados del robot (según Q5 del coach,
// 2026-05-10): "uno a cada costado, para que cuando patean puedan hacerse
// analisis diferencial y que avance derecho hacia adelante".
//
// Análisis diferencial:
//   Si ambos OTOS ven el mismo Δx, Δy → traslación pura (sin rotación, sin slip).
//   Si difieren en X → diferencia de velocidad lateral entre los lados del robot,
//     puede ser:
//       • rotación del robot (esperable, OK).
//       • slip de ruedas al patear (no deseable — flag).
//   Si difieren en Y → diferencia de velocidad longitudinal, idem.
//
// Fusión central (pose del robot en el centro):
//   x_robot      = (x_otos_izq + x_otos_der) / 2
//   y_robot      = (y_otos_izq + y_otos_der) / 2
//   heading      = promedio VECTORIAL de los headings absolutos de las 2 IMUs
//                  (atan2(senL+senR, cosL+cosR), ver otos_fusion.h — audit 2026-06-03 #7).
//                  (El cálculo viejo atan2(Δy, separación) saturaba en ±90° y sólo
//                   valía para giros chicos; descartaba el heading absoluto del OTOS.)
//
// Modo single-OTOS (degradación, p.ej. placa 04-12 con solo SDA2/SCL2 ruteado):
//   Se usa el OTOS disponible como única fuente de pose. No hay diferencial.

#pragma once
#include <stdint.h>
#include "types.h"

namespace iitasoccer {

bool  otos_init();             // retorna true si al menos un OTOS responde
void  otos_tick();             // lee de los OTOS conectados y fusiona

// Pose fusionada del robot (centro geométrico).
float otos_get_x_mm();
float otos_get_y_mm();
float otos_get_heading_deg();

// Velocidades.
float otos_get_vx_mm_s();
float otos_get_vy_mm_s();
float otos_get_omega_rad_s();

// Análisis diferencial:
//   Retorna la magnitud del slip detectado (mm/s de diferencia entre los OTOS
//   menos lo esperable por la rotación pura). 0 = sin slip.
//   Implementación pura en otos_fusion.h (otos_slip_estimate): trabaja sobre
//   VELOCIDADES y resta |ω|·separación. Sólo se computa en modo 2-OTOS.
//   ⚠️ Todavía NO debe usarse para decisiones de control sin validar en HW que
//   los OTOS reales se comportan así (audit 2026-06-03 #20 / TASK-004 separación).
float otos_get_slip_estimate();

// Reset de la pose acumulada a (0, 0, 0).
void  otos_reset();

// Salud / diagnóstico:
//   Los flags ready ahora reflejan la SALUD ACTUAL del OTOS (lecturas I²C OK),
//   no sólo la detección de boot (audit 2026-06-03 #6/#15). Si un OTOS deja de
//   responder N ticks seguidos, su flag cae (latch) y la confidence emitida baja.
//   Re-armar un OTOS caído requiere otos_reset() (CENTRAL_RESET_OTOS) o reboot.
bool     otos_is_left_ready();   // OTOS izquierdo (U5 → Wire / I2C bus 0) sano (lecturas OK)
bool     otos_is_right_ready();  // OTOS derecho  (U6 → Wire1 / I2C bus 1) sano (lecturas OK)
uint32_t otos_get_tick_count();

#if defined(DOWN_DEBUG_TELEMETRY) || defined(DOWN_USB_MONITOR)
// Lecturas por-OTOS SIN fusionar (la pose de cada sensor, ya en mm/deg). Para la
// telemetría de banco del arquero / el monitor USB de competencia: ver el
// diferencial izq/der en vivo. GATEADO → sin estos flags no se compilan.
// Devuelven el último valor cacheado por otos_tick(); 0 si ese OTOS no está
// conectado/vivo.
float otos_get_left_x_mm();
float otos_get_left_y_mm();
float otos_get_left_heading_deg();
float otos_get_right_x_mm();
float otos_get_right_y_mm();
float otos_get_right_heading_deg();
#endif

}  // namespace iitasoccer
