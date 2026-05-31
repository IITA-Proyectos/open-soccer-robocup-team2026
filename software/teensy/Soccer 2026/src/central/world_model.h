// world_model.h (CENTRAL) — Espejo del WorldSnapshot recibido del TOP.
//
// CENTRAL no fusiona sensores — recibe el WorldSnapshot armado por TOP y lo
// expone como estado del mundo para que strategy lo consulte. Si CENTRAL deja
// de recibir snapshots (TOP cuelga), expone confidence = 0 y `is_fresh()` = false
// para que strategy decida modo seguro.

#pragma once
#include <stdint.h>
#include "types.h"

namespace iitasoccer {

void world_model_init();

// Actualiza el world model con el último snapshot recibido del TOP.
// Llamar desde comm_top cuando llega un WORLD_SNAPSHOT.
void world_model_apply_snapshot(const WorldSnapshot& snap);

// Actualiza el mini-mundo de la línea con el último LINE_URGENT recibido de DOWN.
// Recibe el contrato v2 (LineStatusV2, 16 bytes). Ver docs/firmware/CONTRATO-DATOS-DOWN.md.
void world_model_apply_line(const LineStatusV2& line);

// Fresheness check — si no se recibe snapshot en N ms, datos son stale.
bool world_model_snapshot_is_fresh();
bool world_model_line_is_fresh();

// === Acceso a pose y entidades ===
float world_model_get_my_x_mm();
float world_model_get_my_y_mm();
float world_model_get_my_heading_deg();

bool  world_model_ball_visible();
float world_model_get_ball_x_mm();
float world_model_get_ball_y_mm();

bool  world_model_goal_opp_visible();
float world_model_get_goal_opp_angle_deg();
float world_model_get_goal_opp_distance_mm();

uint16_t world_model_get_min_obstacle_mm();

// === Línea (de DOWN bus emergencia) ===
bool     world_model_line_detected();
float    world_model_get_line_angle_deg();
uint8_t  world_model_get_line_depth();
bool     world_model_imminent_exit();
bool     world_model_line_data_valid();   // data_valid del último frame (compuerta maestra)
uint8_t  world_model_line_event_flags();  // EV_* del último frame (diagnóstico / observabilidad)

// === Estado táctico (de WorldSnapshot.flags) ===
bool world_model_match_running();
bool world_model_in_own_penalty_area();
bool world_model_partner_alive();
bool world_model_partner_sees_ball();

uint8_t world_model_referee_cmd();

}  // namespace iitasoccer
