// comm_down.h (CENTRAL) — recibe LINE_URGENT de la placa ABAJO (bus emergencia).
//
// Hardware: Serial2 del Teensy 4.1 (pines 7/8), 230400 baud.
// ⚠️ Conflicto 7/8 PENDIENTE de aislar (TASK-036): los pines 7/8 son también el
//    driver del motor 2 (U17). Solo importa moviendo motores. Si se confirma 7/8
//    = motor Y se corren motores + comm juntos, migrar a Serial7 (28/29).
//    Ver docs/firmware/ANALISIS-COMM-DOWN-CENTRAL-2026-05-31.md.
// Frecuencia: 100-200 Hz.
// Watchdog: si no llega en 500 ms, world_model_line_is_fresh() = false.

#pragma once
#include <stdint.h>

namespace iitasoccer {

void comm_down_init();
int  comm_down_tick();   // drena UART, aplica LineStatusV2 a world_model

// Comandos administrativos hacia ABAJO:
void comm_down_send_reset_otos();
void comm_down_send_calib_line(bool white);  // false=carpet, true=white

uint32_t comm_down_get_frames_received();
uint32_t comm_down_get_crc_errors();
uint32_t comm_down_get_frames_lost();    // SEQ gaps acumulados (frames perdidos en el enlace)

}  // namespace iitasoccer
