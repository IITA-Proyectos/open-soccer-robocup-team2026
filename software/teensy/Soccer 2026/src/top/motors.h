// motors.h — Lado TOP de la comunicación con el Zircon (motor server).
//
// El TOP envía MotorCommand al Zircon (Teensy 4.1 del Zircon Rev v15) por UART.
// El Zircon aplica cinemática inversa omni-3 y controla los 3 motores.
//
// El TOP NO ejecuta cinemática inversa — eso vive en motors_zircon.cpp del
// Zircon, que conoce los ángulos físicos de las ruedas. El TOP solo manda
// (vx, vy, omega) y el flag de kicker.
//
// Watchdog: si el TOP no envía un comando, el Zircon detiene los motores
// automáticamente (ver main_zircon.cpp, COMMAND_TIMEOUT_MS).

#pragma once
#include <stdint.h>
#include "types.h"

namespace iitasoccer {

void motors_init();

// Envía un MotorCommand al Zircon. Llamar a 100 Hz desde el loop del TOP.
//   vx_mm_s      : velocidad lateral deseada (+ = derecha)
//   vy_mm_s      : velocidad longitudinal deseada (+ = frente)
//   omega_centideg_s: velocidad angular (+ = CCW)
//   kicker       : true = activar kicker (solo delantero)
void motors_send_command(int16_t vx_mm_s, int16_t vy_mm_s,
                          int16_t omega_centideg_s, bool kicker_fire);

// Atajo conveniente: enviar comando STOP.
void motors_send_stop();

// Drena la UART del Zircon (recibe ZIRCON_STATUS si lo pedimos).
int motors_tick_rx();

// Estado del Zircon recibido más reciente. is_fresh = recibido en últimos 500 ms.
bool                  motors_zircon_is_fresh();
const ZirconStatus&  motors_zircon_get_status();

// Pide al Zircon un reporte de estado (responde con ZIRCON_STATUS).
void motors_request_status();

// Stats:
uint32_t motors_get_commands_sent();
uint32_t motors_get_status_received();

}  // namespace iitasoccer
