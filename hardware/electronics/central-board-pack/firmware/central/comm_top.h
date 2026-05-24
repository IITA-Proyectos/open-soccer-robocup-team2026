// comm_top.h (CENTRAL) — recibe WORLD_SNAPSHOT de la placa ARRIBA.
//
// Hardware: Serial1 del Teensy 4.1 (pines 0/1).
// Frecuencia: 100 Hz.
// Watchdog: si no llega snapshot en 500 ms, world_model marca stale y strategy
// degrada a modo seguro.

#pragma once
#include <stdint.h>

namespace iitasoccer {

void comm_top_init();
int  comm_top_tick();   // drena UART, aplica snapshots a world_model

uint32_t comm_top_get_frames_received();
uint32_t comm_top_get_crc_errors();

}  // namespace iitasoccer
