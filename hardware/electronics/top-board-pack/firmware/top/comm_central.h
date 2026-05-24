// comm_central.h (TOP) — envía WORLD_SNAPSHOT al CENTRAL.
//
// Hardware: Serial2 del Teensy 4.0 (pines 7/8) — único UART libre en TOP
// según schematic 04-12.
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
