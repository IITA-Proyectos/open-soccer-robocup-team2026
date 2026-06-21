// main_arqueromix.cpp — entrypoint del programa AUTOCONTENIDO "arqueromix".
//
// Hermano ARQUERO de centralmix: FSM del arquero 2025 + primitivas de motor directas,
// alimentado por los datos de TOP/DOWN por serie (SIN world_model). Estructura mínima:
//
//   setup(): inicializa comunicación (TOP/DOWN), motores y FSM.
//   loop():  drena comunicación → avanza FSM.
//
// La lógica vive en amix_comm.cpp / amix_motors.cpp / amix_fsm.cpp. Este archivo NO
// contiene lógica de juego.
//
// Build AISLADO (platformio): env central_robot2_arqueromix con
//   build_src_filter = +<arqueromix/> +<shared/>  → NO compila src/central/.
// Heading: del SNAPSHOT del TOP por default; -DARQMIX_HEADING_OTOS usa el OTOS.
//
// ⚠️ NO TESTEADO EN HARDWARE.

#include <Arduino.h>

#include "amix_config.h"
#include "amix_io.h"
#include "amix_comm.h"
#include "amix_motors.h"
#include "amix_fsm.h"

void setup() {
    iitasoccer::arqmix::amix_comm_init();
    iitasoccer::arqmix::amix_motors_init();
    iitasoccer::arqmix::amix_fsm_init();
}

void loop() {
    iitasoccer::arqmix::amix_comm_tick();
    iitasoccer::arqmix::amix_fsm_tick();
}
