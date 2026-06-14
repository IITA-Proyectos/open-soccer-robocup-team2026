// comm_central.h (TOP) — envía WORLD_SNAPSHOT al CENTRAL.
//
// Hardware: Serial4 del Teensy 4.0 (RX4 = pin 16, TX4 = pin 17). FIX 2026-06-02:
// el 4.0 NO expone Serial7 (28/29) en el borde (back-pads); el enlace a CENTRAL va
// por Serial4. Cable: TOP pin 17 (TX4) -> CENTRAL pin 28 (RX7, Serial7 del 4.1).
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
// Frames presumiblemente perdidos en RX desde CENTRAL (gap de SEQ acumulado).
// SE SOLAPA con crc_errors del decoder (ver LinkSeqTracker en comm_arbiter.h).
// (P1-SEQ-LINK-HEALTH)
uint32_t comm_central_get_frames_lost();
// Snapshots dropeados en TX por buffer lleno (SI-02): 0 en operacion normal.
uint32_t comm_central_get_frames_tx_dropped();

#if defined(TOP_DEBUG_TELEMETRY) || defined(TOP_USB_MONITOR)
// Copia el ÚLTIMO WorldSnapshot difundido a la CENTRAL (para la telemetría/monitoreo
// USB). Retorna false si todavía no se envió ninguno.
bool comm_central_get_last_snapshot(WorldSnapshot& out);
#endif

}  // namespace iitasoccer
