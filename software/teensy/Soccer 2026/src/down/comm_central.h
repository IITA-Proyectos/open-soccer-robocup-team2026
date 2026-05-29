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

// Carga calibración persistida desde EEPROM hacia el DownModel interno. Llamar
// UNA vez en setup(), después de line_ring_calibrate_carpet(). Si hay calib
// válida, la usa y bloquea el lazy-init (EEPROM gana). Retorna true si cargó.
bool comm_central_load_persisted_calib();

// Estadísticas:
uint32_t comm_central_get_frames_received();
uint32_t comm_central_get_frames_sent();
uint32_t comm_central_get_frames_dropped();  // descartados por TX buffer lleno (P1.6)
uint32_t comm_central_get_crc_errors();

}  // namespace iitasoccer
