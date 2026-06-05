// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
// comm_down.h — Lado TOP de la comunicación con la placa DOWN (slave de sensores).
//
// La placa DOWN envía cada 10 ms (100 Hz):
//   • LineStatus  (ángulo línea, profundidad, flag salida inminente)
//   • Pose2D      (pose fusionada de los 2 OTOS)
//   • Velocity2D  (velocidad + slip_estimate)
//
// El TOP recibe y mantiene el último valor de cada uno disponible para
// world_model. Si no recibe nada por DOWN_HEARTBEAT_TIMEOUT_MS, marca
// los datos como stale (confidence baja → strategy ignora).

#pragma once
#include <stdint.h>
#include "types.h"

namespace iitasoccer {

void comm_down_init();

// Drena bytes del UART desde DOWN, decodifica frames. Retorna cantidad de
// frames procesados (puede ser 0).
int comm_down_tick();

// Envía un comando al DOWN (reset OTOS, calibrar línea, etc).
void comm_down_send_reset_otos();
void comm_down_send_calib_line(bool white);  // false = carpet, true = white

// Accesores al último estado conocido del DOWN.
// is_*_fresh() retorna true si llegó dato hace menos de DOWN_HEARTBEAT_TIMEOUT_MS.
bool             comm_down_is_line_fresh();
const LineStatus& comm_down_get_line_status();

bool             comm_down_is_pose_fresh();
const Pose2D&    comm_down_get_pose();

bool             comm_down_is_vel_fresh();
const Velocity2D& comm_down_get_velocity();

// Estadísticas:
uint32_t comm_down_get_frames_received();
uint32_t comm_down_get_crc_errors();

}  // namespace iitasoccer
