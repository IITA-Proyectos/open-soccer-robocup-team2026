// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
// comm_central.h — Bus de emergencia DOWN → CENTRAL via Serial1.
//
// Canal urgente para línea + imminent_exit + measurement. Latencia objetivo
// < 15 ms desde detección de blanco en sensores hasta PWM en motor.
//
// El payload es solo *measurement* físico crudo (ángulo + profundidad signed),
// SIN lógica de control. El PID lateral del arquero corre en CENTRAL, no acá
// (ver docs/ARQUITECTURA-3-PLACAS-2026.md sección "todos los PIDs en CENTRAL").
//
// Hardware: Serial1 (pines 0/1 del Teensy 4.0) → conector U11 del schematic DOWN
// (verificado en PCB JSON 04-12 — único otro UART cableado además de Serial5).
//
// También recibe comandos administrativos desde CENTRAL (calibrar línea,
// reset OTOS) en este mismo UART.

#pragma once
#include <stdint.h>

namespace iitasoccer {

void comm_central_init();

// Drena RX desde CENTRAL (comandos administrativos). Llamar cada loop.
int comm_central_tick();

// Envía LINE_URGENT con measurement crudo. Llamar a 100-200 Hz.
void comm_central_send_line_urgent();

// Estadísticas:
uint32_t comm_central_get_frames_received();
uint32_t comm_central_get_frames_sent();
uint32_t comm_central_get_crc_errors();

}  // namespace iitasoccer
