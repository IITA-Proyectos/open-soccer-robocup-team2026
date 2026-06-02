// down_tx.h — Capa de transmisión BROADCAST de la placa DOWN.
//
// Difunde cada mensaje a AMBOS enlaces de salida:
//   • Enlace 0 = CENTRAL (Serial1, TX1 = pin 1)
//   • Enlace 1 = TOP     (Serial5, TX5 = pin 20)
// Cada enlace lleva su PROPIO SEQ monótono (compartido entre los 3 tipos) para que
// la detección de pérdida por SEQ del receptor sea correcta al intercalar tipos.
// Backpressure por enlace (availableForWrite): si el buffer está lleno, dropea el
// frame y lo cuenta (no bloquea el line_ring de 1 kHz). El Serial.begin() de ambos
// UART lo hacen comm_central_init()/comm_top_init(); este módulo solo escribe.
#pragma once
#include <stdint.h>
#include "types.h"

namespace iitasoccer {

void down_tx_broadcast_line(const LineStatusV2& s);   // 0x10 a CENTRAL + TOP
void down_tx_broadcast_pose(const Pose2D& p);         // 0x11 a CENTRAL + TOP
void down_tx_broadcast_vel(const Velocity2D& v);      // 0x12 a CENTRAL + TOP

// Telemetría por enlace: 0 = CENTRAL (Serial1), 1 = TOP (Serial5).
uint32_t down_tx_get_sent(uint8_t link);
uint32_t down_tx_get_dropped(uint8_t link);

}  // namespace iitasoccer
