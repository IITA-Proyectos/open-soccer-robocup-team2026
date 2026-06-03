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

// === Estado / watchdog ===
bool     cameras_front_alive();
bool     cameras_back_alive();
bool     cameras_any_alive();         // al menos 1 cámara reportó en último timeout
bool     cameras_both_dead();         // ninguna reportó → DEGRADED_NO_CAMERAS

// === Diagnóstico ===
uint32_t cameras_packets_front();
uint32_t cameras_packets_back();
uint32_t cameras_resyncs_total();

}  // namespace iitasoccer
