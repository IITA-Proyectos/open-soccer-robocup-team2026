// otos_position.h — controlador de posición/distancia por ODOMETRÍA OTOS (BUILD-OTOS).
//
// Módulo PURO host-testeable (solo <stdint.h>/<math.h>, SIN Arduino), vive en
// src/shared como pids.h / kinematics.h / drive_straight.h. Permite "andá N mm
// para adelante y para los costados y pará", usando la pose acumulada que el OTOS
// integra. A diferencia de drive_straight (corrección de deriva por velocidad),
// este módulo cierra el lazo de POSICIÓN: captura un objetivo y comanda velocidad
// hasta llegar, con rampa de frenado.
//
// ── Convención de signos (igual que kinematics.h / drive_straight.h) ──────────
//   • Marco ROBOT: +Y = frente del robot, +X = derecha del robot.
//   • Heading en grados, CCW+ (positivo = giro antihorario visto desde arriba).
//   • Marco MUNDO: el frame en que el OTOS acumula la pose (cur_x, cur_y).
//
// ── Captura de objetivo ──────────────────────────────────────────────────────
//   set_target convierte un desplazamiento (dx,dy) expresado en el MARCO ROBOT
//   actual a una posición ABSOLUTA en mundo y la guarda:
//       target_world = cur_pos + R(cur_heading) * (dx, dy)
//   con R(h) = [[cos h, -sin h],[sin h, cos h]] (rotación CCW estándar).
//
// ── Ley de control (update) ──────────────────────────────────────────────────
//       err_world  = target_world - cur_pos
//       remaining  = hypot(err_world)
//       err_robot  = R(-cur_heading) * err_world      (mundo -> robot)
//       si remaining < arrive_tol_mm -> arrived, vx = vy = 0.
//       si no:
//          speed = clamp(min_speed, max_speed, max_speed * remaining / decel_zone)
//          vx = speed * err_robot_x / remaining       (componente derecha)
//          vy = speed * err_robot_y / remaining       (componente adelante)
//   La rampa lineal de frenado dentro de decel_zone evita el sobrepaso; el clamp
//   a min_speed asegura que el robot no se quede "muerto" justo antes del tol.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Parámetros del lazo de posición (mm y mm/s).
struct OtosMoveParams {
    float max_speed_mm_s;   // velocidad de crucero máxima
    float decel_zone_mm;    // distancia dentro de la cual arranca la rampa de frenado
    float arrive_tol_mm;    // radio de "llegué" (remaining < tol -> arrived, v=0)
    float min_speed_mm_s;   // piso de velocidad mientras no se haya llegado
};

// Objetivo capturado en MARCO MUNDO (absoluto). active=false => sin objetivo.
struct OtosMoveTarget {
    float tx_world_mm;
    float ty_world_mm;
    bool  active;
};

// Comando de salida en MARCO ROBOT (+X derecha, +Y adelante).
struct OtosMoveCmd {
    float vx_mm_s;        // componente lateral (derecha+)
    float vy_mm_s;        // componente de avance (adelante+)
    bool  arrived;        // true cuando remaining < arrive_tol_mm
    float remaining_mm;   // distancia euclídea al objetivo (marco mundo)
};

// Parámetros por defecto: max=600, decel_zone=300, tol=15, min=60.
OtosMoveParams otos_move_default_params();

// Captura objetivo: desde la pose OTOS actual, moverse (dx,dy) en MARCO ROBOT.
// Convierte a marco mundo con cur_heading_deg y guarda el target absoluto.
void otos_move_set_target(OtosMoveTarget& t,
                          float cur_x_mm, float cur_y_mm, float cur_heading_deg,
                          float dx_robot_mm, float dy_robot_mm);

// Cancela el objetivo (active=false).
void otos_move_cancel(OtosMoveTarget& t);

// Comando de velocidad en MARCO ROBOT para acercarse al target (función PURA).
OtosMoveCmd otos_move_update(const OtosMoveTarget& t, const OtosMoveParams& p,
                             float cur_x_mm, float cur_y_mm, float cur_heading_deg);

}  // namespace iitasoccer
