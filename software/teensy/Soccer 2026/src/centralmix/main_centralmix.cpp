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
    Serial.begin(115200);   // USB: debug de la pelota recibida (monitor serie de la CENTRAL)
    iitasoccer::mix::mix_comm_init();
    iitasoccer::mix::mix_motors_init();
    iitasoccer::mix::mix_fsm_init();
}

void loop() {
    iitasoccer::mix::mix_comm_tick();
    iitasoccer::mix::mix_fsm_tick();

    // --- DEBUG TEMPORAL: imprimir la pelota que se RECIBE del TOP, cada 200 ms ---
    // Para confirmar si llega bien el dato. Abrir el monitor serie USB de la CENTRAL.
    // (Se puede sacar/gatear después; no afecta la conducta, solo imprime por USB.)
    static unsigned long t_dbg = 0;
    if (millis() - t_dbg >= 200) {
        t_dbg = millis();
        const iitasoccer::mix::MixIO& io = iitasoccer::mix::g_io;
        const float dist_cm = sqrtf(io.ball_x_cm * io.ball_x_cm + io.ball_y_cm * io.ball_y_cm);
        Serial.print("TOPlink=");      Serial.print(io.top_link_fresh ? "OK " : "-- ");
        Serial.print("pelota vis=");   Serial.print(io.ball_visible ? 1 : 0);
        Serial.print(" x=");           Serial.print(io.ball_x_cm, 1);
        Serial.print(" y=");           Serial.print(io.ball_y_cm, 1);
        Serial.print(" dist=");        Serial.print(dist_cm, 1);
        Serial.println(" cm");
    }
}
 