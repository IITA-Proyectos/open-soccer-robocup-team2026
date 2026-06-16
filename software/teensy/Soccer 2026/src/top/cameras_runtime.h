// cameras_runtime.h — Wiring de los 2 CameraParser sobre Serial3 + Serial5.
//
// Responsabilidad:
//   • cameras_init()  : abre Serial3 (cam frontal) y Serial5 (cam trasera).
//   • cameras_tick()  : drena ambos UARTs sin bloquear, alimenta los parsers,
//                       y al cerrar un packet actualiza el snapshot fusionado.
//   • Getters         : exponen el último valor fusionado (pelota + 2 arcos).
//   • Watchdog        : si una cámara no manda un packet en CAMERA_TIMEOUT_MS,
//                       se marca como caída y sus datos se ignoran al fusionar.
//
// La lógica de fusión pura (rotación 180° + ponderado por confianza) vive en
// `src/shared/cameras_fusion.{h,cpp}` y se testea host-native.
//
// Pelota y arcos vienen en coords RELATIVAS AL ROBOT (mm):
//   +y = frente,  +x = lateral derecho.

#pragma once
#include <stdint.h>
#ifdef TOP_ENABLE_HEADING_XVAL
#include "goal_rate_tracker.h"   // a FILE SCOPE (el header tiene su propio namespace iitasoccer)
#endif

namespace iitasoccer {

void cameras_init();
void cameras_tick();

// === Pelota (fusión front + back) ===
bool    cameras_ball_visible();
int16_t cameras_get_ball_x_mm();
int16_t cameras_get_ball_y_mm();
uint8_t cameras_get_ball_confidence();

// Velocidad de la pelota (mm/s, marco robot) derivada de la posición fusionada.
// 0 cuando no hay estimación válida (pelota recién aparecida, perdida, o stall).
// Lógica pura host-testeada en src/shared/ball_velocity.{h,cpp}.
int16_t cameras_get_ball_vx_mm_s();
int16_t cameras_get_ball_vy_mm_s();

// === Arco amarillo ===
bool    cameras_goal_yellow_visible();
int16_t cameras_get_goal_yellow_angle_centideg();
int16_t cameras_get_goal_yellow_distance_mm();

// === Arco azul ===
bool    cameras_goal_blue_visible();
int16_t cameras_get_goal_blue_angle_centideg();
int16_t cameras_get_goal_blue_distance_mm();

// === Detecciones POR CÁMARA (pre-fusión) — A1 monitor de posicionamiento ===
// Lo que ve CADA óptica por separado (frontal / trasera), antes de fusionar.
// Permite ver el desacuerdo entre cámaras (pelota fantasma del promedio fusionado)
// y decidir cuál apagar. Combinar con cameras_front_alive()/back_alive() para
// distinguir "no ve" de "cámara caída". Pelota en x/y (mm); arcos en polar.
bool    cameras_get_ball_front_visible();
int16_t cameras_get_ball_front_x_mm();
int16_t cameras_get_ball_front_y_mm();
bool    cameras_get_ball_back_visible();
int16_t cameras_get_ball_back_x_mm();
int16_t cameras_get_ball_back_y_mm();

bool    cameras_get_goal_yellow_front_visible();
int16_t cameras_get_goal_yellow_front_angle_centideg();
int16_t cameras_get_goal_yellow_front_distance_mm();
bool    cameras_get_goal_yellow_back_visible();
int16_t cameras_get_goal_yellow_back_angle_centideg();
int16_t cameras_get_goal_yellow_back_distance_mm();

bool    cameras_get_goal_blue_front_visible();
int16_t cameras_get_goal_blue_front_angle_centideg();
int16_t cameras_get_goal_blue_front_distance_mm();
bool    cameras_get_goal_blue_back_visible();
int16_t cameras_get_goal_blue_back_angle_centideg();
int16_t cameras_get_goal_blue_back_distance_mm();

// === Estado / watchdog ===
bool     cameras_front_alive();
bool     cameras_back_alive();
bool     cameras_any_alive();         // al menos 1 cámara reportó en último timeout
bool     cameras_both_dead();         // ninguna reportó → DEGRADED_NO_CAMERAS

// === Diagnóstico ===
uint32_t cameras_packets_front();
uint32_t cameras_packets_back();
uint32_t cameras_resyncs_total();        // pérdida de framing AGREGADA (front+back)

// Telemetría de integridad del enlace cámara→TOP (TASK-015 / P0-CAM-CRC).
// AGREGADOS front+back de los contadores que ya expone el parser v2 (cameras.h):
//   • crc_errors  : frames de 11 B completos pero con CRC8 malo (bit-flip).
//   • resync      : alias explícito de cameras_resyncs_total() (framing perdido).
// Aditivos, solo leen el parser; NO tocan el contrato de wire ni cameras.cpp/.h.
uint32_t cameras_get_crc_errors_total();
uint32_t cameras_resync_total();         // == cameras_resyncs_total()

#ifdef TOP_ENABLE_HEADING_XVAL
// Cross-validación del heading (TASK-213): tasa de rotación inferida del bearing del
// arco. main_top lo llama con el bearing del arco RIVAL ya resuelto (la polaridad
// opp/own se decide en main_top, no acá). El estado del tracker vive en el .cpp.
// GoalRateResult viene del include a file-scope de arriba.
GoalRateResult cameras_goal_rate_update(int16_t bearing_centideg, bool visible, uint32_t now_ms);
#endif

}  // namespace iitasoccer
