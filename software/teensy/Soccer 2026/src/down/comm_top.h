// comm_top.h — Comunicación de la placa DOWN con la placa TOP (master).
//
// Protocolo: el de src/shared/proto.h (frame con START/CRC-16/SEQ/END).
// Hardware: Serial5 del Teensy 4.0 (pines 20 RX5, 21 TX5) → conector U10 → TOP.
//
// Roles:
//   • Receptor: drena bytes de Serial5 cuando llegan, decodifica frames,
//     procesa comandos del TOP (reset OTOS, calibrar línea, etc).
//   • Emisor: a 100 Hz envía 3 frames con el estado del DOWN:
//       LINE_STATUS (ángulo línea + profundidad + flag salida inminente)
//       OTOS_POSE   (x, y, heading)
//       OTOS_VEL    (vx, vy, omega, slip estimate)

#pragma once
#include <stdint.h>
#include "types.h"

namespace iitasoccer {

void comm_top_init();

// Drena bytes del UART y procesa frames recibidos (no bloquea).
// Retorna número de frames procesados en esta llamada.
int  comm_top_tick();

// Envía los 3 frames de estado de DOWN al TOP. Llamar a 100 Hz.
void comm_top_send_status();

// Estadísticas para debug:
uint32_t comm_top_get_frames_received();
uint32_t comm_top_get_frames_sent();
uint32_t comm_top_get_frames_dropped();  // descartados por TX buffer lleno (P1.6)
uint32_t comm_top_get_crc_errors();

}  // namespace iitasoccer
