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
//   heading      = PROMEDIO CIRCULAR de los headings ABSOLUTOS de cada OTOS:
//                    atan2( sin(h_izq)+sin(h_der), cos(h_izq)+cos(h_der) )
//                  (cubre todo el rango ±180° y maneja el wrap; ver M2 en otos.cpp)
//   ⚠️ NO usar atan2(otos_der.y - otos_izq.y, OTOS_SEPARATION_MM) como heading:
//      satura a (-90,+90) y descarta el heading absoluto. Queda SOLO como
//      diagnóstico (otos_get_heading_diag_deg()).
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
//   Retorna la magnitud del slip lateral detectado, en mm/s: la diferencia de
//   velocidad lateral entre los 2 OTOS que NO se explica por la rotación del cuerpo:
//     slip = | (vx_der - vx_izq) - g_omega * OTOS_SEPARATION_MM |   [mm/s]
//   Se computa desde VELOCIDADES (no posición integrada), por lo que está ACOTADO
//   y NO es monótono: vuelve a ~0 cuando cesa el patinaje. 0 = sin slip. (Ver M3.)
float otos_get_slip_estimate();

// Reset de la pose acumulada a (0, 0, 0).
void  otos_reset();

// Diagnóstico:
bool     otos_is_left_ready();   // OTOS izquierdo (Wire / I2C bus 1) responde
bool     otos_is_right_ready();  // OTOS derecho (Wire1 / I2C bus 2) responde
uint32_t otos_get_tick_count();

// Diagnóstico (solo modo dual): heading inferido de atan2(dy, sep), saturado a
// (-90,+90). NO es el heading autoritativo (ese es otos_get_heading_deg()).
float    otos_get_heading_diag_deg();

}  // namespace iitasoccer
