// comm_top.h (CENTRAL) — recibe WORLD_SNAPSHOT de la placa ARRIBA.
//
// Hardware: Serial7 del Teensy 4.1 (RX7 = pin 28, TX7 = pin 29). Reasignado
// 2026-05-31 (antes Serial1/0-1, que pasó a recibir la línea del DOWN).
// Frecuencia: 100 Hz.
// Watchdog: si no llega snapshot en 500 ms, world_model marca stale y strategy
// degrada a modo seguro.

#pragma once
#include <stdint.h>

namespace iitasoccer {

void comm_top_init();
void comm_top_tick();   // drena UART, aplica snapshots a world_model

uint32_t comm_top_get_frames_received();
uint32_t comm_top_get_crc_errors();
uint32_t comm_top_get_bytes_received();  // DIAG: bytes crudos en Serial7 (link TOP)
uint32_t comm_top_get_resync_events();   // DIAG: resyncs del decoder (byte-slip/ruido, #25)
uint32_t comm_top_get_snapshot_size_rejects();  // CC-01 DIAG: WorldSnapshot con tamano != schema (deploy v2/v3)

}  // namespace iitasoccer
