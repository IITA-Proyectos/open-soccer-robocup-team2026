// main_centralmix.cpp — entrypoint del programa AUTOCONTENIDO "centralmix".
//
// Programa estilo 2025 (FSM del delantero + primitivas de motor directas) pero
// alimentado por los datos de TOP/DOWN (SIN world_model). Estructura mínima:
//
//   setup(): inicializa comunicación (TOP/DOWN), motores y FSM.
//   loop():  drena comunicación → avanza FSM.
//
// CONTRATO / SKELETON. La lógica vive en mix_comm.cpp / mix_motors.cpp /
// mix_fsm.cpp (a implementar). Este archivo NO contiene lógica de juego.
//
// Para compilar como env propio en platformio.ini (a definir por el equipo, NO se
// toca acá): src_filter que incluya SOLO src/centralmix/ + src/shared/ (proto,
// types, line_view, pose_view) y build_flags -DROBOT... según corresponda. La
// fuente de heading se elige con -DMIX_HEADING_OTOS (default: BNO). Ver mix_config.h.
//
// ⚠️ NO TESTEADO EN HARDWARE.

#include <Arduino.h>

#include "mix_config.h"
#include "mix_io.h"
#include "mix_comm.h"
#include "mix_motors.h"
#include "mix_fsm.h"

#include <math.h>   // sqrtf (debug de distancia)

void setup() {
    iitasoccer::mix::mix_comm_init();
    iitasoccer::mix::mix_motors_init();
    iitasoccer::mix::mix_fsm_init(); 
}

void loop() {
    iitasoccer::mix::mix_comm_tick();
    iitasoccer::mix::mix_fsm_tick();
}
 