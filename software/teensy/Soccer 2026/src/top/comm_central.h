// comm_central.h (TOP) — envía WORLD_SNAPSHOT al CENTRAL.
//
// Hardware: Serial7 del Teensy 4.0 (TX7 = pin 29, RX7 = pin 28). SWAP 2026-05-31
// (TASK-204): antes Serial5, pero ahí quedó soldada la cámara trasera.
// Frecuencia: 100 Hz.

#pragma once
#include <stdint.h>
#include "types.h"

namespace iitasoccer {

void comm_central_init();
int  comm_central_tick();           // drena RX (comandos desde CENTRAL)
void comm_central_send_snapshot(const WorldSnapshot& snap);

uint32_t comm_central_get_frames_sent();
uint32_t comm_central_get_frames_received();

}  // namespace iitasoccer
