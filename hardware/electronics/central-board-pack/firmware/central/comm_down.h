// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
// comm_down.h (CENTRAL) — recibe LINE_URGENT de la placa ABAJO (bus emergencia).
//
// Hardware: Serial2 del Teensy 4.1 (pines 7/8) — UART secundario rápido.
// Frecuencia: 100-200 Hz.
// Watchdog: si no llega en 500 ms, world_model_line_is_fresh() = false.

#pragma once
#include <stdint.h>

namespace iitasoccer {

void comm_down_init();
int  comm_down_tick();   // drena UART, aplica LineStatus a world_model

// Comandos administrativos hacia ABAJO:
void comm_down_send_reset_otos();
void comm_down_send_calib_line(bool white);  // false=carpet, true=white

uint32_t comm_down_get_frames_received();
uint32_t comm_down_get_crc_errors();

}  // namespace iitasoccer
