// comm_down.h (CENTRAL) — recibe LINE_URGENT de la placa ABAJO (bus emergencia).
//
// Hardware: Serial1 del Teensy 4.1 (RX1 = pin 0, TX1 = pin 1), 230400 baud.
// Reasignado 2026-05-31: antes Serial2 (7/8), pero esos pines son del driver del
//    motor 2 (U17) → se movió a Serial1 (0/1, conector libre del Zircon). El viejo
//    conflicto 7/8 (TASK-036) quedó RESUELTO: 7/8 libre para el motor 2.
//    Ver hardware/electronics/MAPA-CONEXIONES-3-PLACAS.md.
// Frecuencia: 100-200 Hz.
// Watchdog: si no llega en 500 ms, world_model_line_is_fresh() = false.

#pragma once
#include <stdint.h>

namespace iitasoccer {

void comm_down_init();
void comm_down_tick();   // drena UART, aplica LineStatusV2 a world_model

// Comandos administrativos hacia ABAJO:
void comm_down_send_reset_otos();
void comm_down_send_calib_line(bool white);  // false=carpet, true=white

uint32_t comm_down_get_frames_received();
uint32_t comm_down_get_crc_errors();
uint32_t comm_down_get_frames_lost();    // SEQ gaps acumulados (frames perdidos en el enlace)
uint32_t comm_down_get_resync_events();  // DIAG: resyncs del decoder (byte-slip/ruido, #25)

}  // namespace iitasoccer
