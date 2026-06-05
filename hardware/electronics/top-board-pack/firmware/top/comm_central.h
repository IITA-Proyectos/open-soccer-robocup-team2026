// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
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
