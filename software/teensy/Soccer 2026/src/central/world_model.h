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
// #16 — OJO: varios de estos accessors EXISTEN pero la FSM (strategy.cpp) NO los consume
// hoy (son superficie de contrato para Nivel 2/3 futuro): get_my_x/y, min_obstacle,
// goal_opp_distance, in_own_penalty_area, partner_alive/sees_ball, get_otos_x/y, otos_omega,
// otos_slip, otos_pose_confidence. Que tengan accessor NO implica que el robot los use
// (p.ej. NO hay evasión de obstáculos aunque min_obstacle esté expuesto). Antes de asumir
// una conducta, verificar con grep en strategy.cpp. Ver auditoría 2026-06-03 #16.
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

// Distancia perpendicular FIRMADA del centro del robot a la recta de la línea,
// en mm (WP-3-DOWN / Capa 3; computada en down_model con geometría real).
// Convención de signo (ver down_model.cpp): + = línea ADELANTE del centro (+Y),
// - = ATRÁS (-Y). El arquero la usa como señal de error del PID lateral para
// mantener DISTANCIA PERPENDICULAR fija y desplazarse PARALELO a la línea
// lateral (strafe). Devuelve 0.0f cuando no es confiable (ver más abajo) —
// usar SIEMPRE junto con world_model_cross_track_valid() para distinguir
// "centrado sobre la línea" (0 mm válido) de "sin señal" (N/A).
float    world_model_get_cross_track_mm();

// ¿El cross_track del último frame es una medición real y usable?
// true sólo si data_valid==1 Y el campo no es el centinela N/A (LSV2_NA_I16).
// Es la compuerta que usa el arquero (strategy goalkeeper_tick) para elegir
// entre el control por cross_track (strafe paralelo, Capa 3) y el fallback
// por profundidad (world_model_get_line_depth). NO confundir con line_fresh:
// esto es validez del CAMPO dentro del frame; la frescura del frame es aparte.
bool     world_model_cross_track_valid();

// === Estado táctico (de WorldSnapshot.flags) ===
bool world_model_match_running();
// Override de arranque manual (F3, fail-safe de banco). Cuando es true,
// world_model_match_running() devuelve true aunque no haya START de COMM.
// Solo lo activa main_central bajo -DCENTRAL_ENABLE_MANUAL_START.
void world_model_set_force_match_running(bool on);
bool world_model_in_own_penalty_area();
bool world_model_partner_alive();
bool world_model_partner_sees_ball();

// #19: referee_cmd — RESERVADO / NO consumido por la FSM (0 callers en strategy.cpp).
// El contrato REAL del árbitro de este equipo es BINARIO: START/STOP llega como NIVEL
// GPIO (pines 5/6 del TOP) → flag bit3 MATCH_RUNNING del snapshot → world_model_match_running().
// halftime(2)/reset(3) NO se transmiten por este COMM. No construir lógica sobre este campo
// sin antes cablear su emisión en el TOP. Ver auditoría 2026-06-03 #19 + CONTRATO-DATOS-CENTRAL.
uint8_t world_model_referee_cmd();

// === OTOS directo de DOWN (Capa 1 broadcast) ===
// La pose de cancha AUTORITATIVA sigue siendo la del WorldSnapshot del TOP
// (world_model_get_my_*). El OTOS directo es SOLO para control de movimiento
// (drive-straight / patear derecho), que se cablea en Capa 2.
void world_model_apply_otos_pose(const Pose2D& pose);
void world_model_apply_otos_vel(const Velocity2D& vel);
bool world_model_otos_is_fresh();      // frescura de la POSE (0x11)
bool world_model_otos_vel_is_fresh();  // #21: frescura de la VEL (0x12), por separado de la pose

float   world_model_get_otos_x_mm();
float   world_model_get_otos_y_mm();
float   world_model_get_otos_heading_deg();
float   world_model_get_otos_vx_mm_s();
float   world_model_get_otos_vy_mm_s();
float   world_model_get_otos_omega_deg_s();
uint8_t world_model_get_otos_slip();
uint8_t world_model_otos_pose_confidence();

}  // namespace iitasoccer
