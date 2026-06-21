// mix_comm.h — capa de comunicación de centralmix: TOP/DOWN → variables planas.
//
// CONTRATO. ÚNICO punto que toca Serial. Decodifica los frames proto.h de:
//   - TOP  (Serial7, WORLD_SNAPSHOT 0x60): pelota, arcos, heading, referee/match.
//   - DOWN (Serial1, LineStatusV2 0x10 + Pose2D 0x11 / Velocity2D 0x12): línea + OTOS.
// y POBLA g_io (mix_io.h) con variables planas estilo 2025. NO contiene FSM ni motores.
//
// Reemplaza la lectura serial cruda del 2025 (Serial1.read() byte a byte con
// header 201/202/203 a 19200): ahora son frames CRC-protegidos de TOP/DOWN a 230400
// (ver comm_top.cpp / comm_down.cpp). NO usa world_model.
//
// Mapeo a g_io que mix_comm DEBE hacer (resumen para el implementador):
//   WorldSnapshot.ball_x_mm/ball_y_mm → g_io.ball_x_mm/ball_y_mm   (+X der, +Y adel)
//   WorldSnapshot.ball_visible        → g_io.ball_visible (+ sella t_last_ball_seen_ms)
//   angulo_pelota_deg = atan2(ball_x_mm, ball_y_mm)*180/PI
//   goal_opp/goal_own (centideg/mm)   → g_io.goal_opp_* / goal_own_* (copia directa, SIN color)
//   my_heading_centideg / flags bit4  → heading_deg / heading_valid (si fuente = BNO)
//   Pose2D.heading_centideg (DOWN)    → otos_heading_deg (y heading_deg si MIX_HEADING_OTOS)
//   heading_error_deg = wrap180(heading_deg - heading_inicial)
//   LineStatusV2                      → line_present / line_angle_deg / line_depth
//   flags bit3 (match_running)        → match_running
//
// ⚠️ NO TESTEADO EN HARDWARE.

#pragma once

namespace iitasoccer {
namespace mix {

// Abre Serial7 (TOP) y Serial1 (DOWN) a MIX_UART_BAUD, inicializa decoders, y
// (si la fuente es BNO) inicializa el BNO055 y captura el heading inicial. Llamar
// en setup() ANTES de mix_fsm_init().
void mix_comm_init();

// Drena ambos UARTs, decodifica frames y actualiza g_io. NO bloqueante. Llamar
// primero en cada loop(), antes de mix_fsm_tick(). También actualiza la frescura
// de enlace (top_link_fresh / down_link_fresh) y, si la fuente de heading es BNO,
// lee el BNO en este tick.
void mix_comm_tick();

}  // namespace mix
}  // namespace iitasoccer
