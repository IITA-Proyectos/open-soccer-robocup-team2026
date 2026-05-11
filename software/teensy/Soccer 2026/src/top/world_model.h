// world_model.h — Estado del mundo del robot (pose propia, pelota, partner,
// línea, obstáculos).
//
// Es el "single source of truth" del TOP. Se actualiza desde:
//   • sensors_imu     → heading propio
//   • comm_down       → pose y velocidad desde OTOS + ángulo línea
//   • cameras (top/)  → posición de pelota y arcos
//   • sensors_tof     → obstáculos / paredes / oponentes
//   • comm_arbiter    → snapshot del partner
//
// La estrategia (strategy.h) consulta este modelo. Nada de lógica de FSM
// vive acá — solo fusión de datos y getters limpios.
//
// Versión inicial (Hito 2-3): fusión simple (last-writer-wins por sensor).
// Futuro: Kalman filter para la pelota + posicionamiento por arcos.

#pragma once
#include <stdint.h>
#include "types.h"

namespace iitasoccer {

void world_model_init();

// Actualiza desde todos los sensores conectados. Llamar a 100 Hz.
void world_model_update();

// === Pose propia ===
float world_model_get_my_x_mm();
float world_model_get_my_y_mm();
float world_model_get_my_heading_deg();
uint8_t world_model_get_my_confidence();

// === Pelota (de cámaras OpenMV) ===
bool  world_model_ball_visible();
float world_model_get_ball_x_mm();        // relativo al robot
float world_model_get_ball_y_mm();
uint32_t world_model_get_ball_age_ms();   // tiempo desde última detección

// === Arco rival (configurable por strategy: cyan o magenta) ===
bool  world_model_goal_opp_visible();
float world_model_get_goal_opp_angle_deg();   // ángulo relativo al frente
float world_model_get_goal_opp_distance_mm(); // estimado por tamaño en cámara

bool  world_model_goal_own_visible();

// === Línea blanca (del DOWN) ===
bool  world_model_line_detected();
float world_model_get_line_angle_deg();
bool  world_model_imminent_exit();

// === Obstáculo más cercano (de ToFs + HC-SR04) ===
uint16_t world_model_get_min_obstacle_mm();

// === Partner (de COMM ESP-NOW) ===
bool  world_model_partner_alive();
float world_model_get_partner_x_mm();
float world_model_get_partner_y_mm();
bool  world_model_partner_sees_ball();

// === Match state (de árbitros) ===
bool world_model_match_running();

// Reset (para nuevo partido).
void world_model_reset();

}  // namespace iitasoccer
