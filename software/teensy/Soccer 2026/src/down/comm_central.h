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

// Invalida la calib del DownModel → el próximo send re-deriva desde el
// line_ring (lazy). OBLIGATORIO tras cualquier CAL_* que toque la calib del
// line_ring por USB: sin esto, el LineStatusV2 hacia CENTRAL seguía con la
// calib VIEJA hasta el reboot (bug TASK-306, fix 2026-06-12).
void comm_central_invalidate_calib();

#ifdef DOWN_RX_HARDEN
// F5 (§8.1): ejecuta el calibrate DIFERIDO que encoló handle_frame (separar parseo de
// ejecución: el handler RX sólo deja la nota en <5µs). Llamar desde el loop FUERA del path
// RX, en un punto con el robot quieto/admin. Sin nota pendiente → retorna sin costo; con
// nota → corre el trabajo BLOQUEANTE (~320ms + EEPROM). Gateado off-by-default.
void comm_central_service_pending_calib();
#endif

// Estadísticas:
uint32_t comm_central_get_frames_received();
uint32_t comm_central_get_frames_sent();
uint32_t comm_central_get_frames_dropped();  // descartados por TX buffer lleno (P1.6)
uint32_t comm_central_get_crc_errors();

#ifdef DOWN_USB_MONITOR
struct LineStatusV2;  // fwd-decl (tipo completo en types.h, incluido en el .cpp)
// Copia el ÚLTIMO LineStatusV2 difundido a CENTRAL (para la telemetría USB del
// modo DEBUG/monitor). Retorna false si todavía no se envió ninguno.
bool comm_central_get_last_line_status(struct LineStatusV2& out);

// ── Sintonía fina desde la app (schema v3) ──────────────────────────────────
// Perillas: sensibilidad global al blanco, sensibilidad por sensor, y
// habilitar/deshabilitar sensores. Mutan el DownModel; dm_update las usa cada
// tick. pct se clampea a [-100,+100]; idx fuera de rango se ignora.
void comm_central_set_global_sens(int pct);
void comm_central_set_sensor_sens(int idx, int pct);
void comm_central_set_sensor_enabled(int idx, bool en);
// Llena los campos de sintonía fina para emitir por telemetría (umbral efectivo,
// bits de habilitado, sensibilidad global y por-sensor). Punteros opcionales (null OK).
void comm_central_get_tuning(uint16_t* threshold_eff, uint32_t* enabled_bits,
                             int8_t* global_sens, int8_t* persensor_sens, int n);
// Guarda calib + sintonía + global_sens a EEPROM (deriva carpet/white fresco si
// hace falta). Recarga de EEPROM en vivo (calib+sintonía → g_dm; carpet/white → line_ring).
bool comm_central_save_calib_and_tuning();
bool comm_central_reload_from_eeprom_live();
#endif

}  // namespace iitasoccer
