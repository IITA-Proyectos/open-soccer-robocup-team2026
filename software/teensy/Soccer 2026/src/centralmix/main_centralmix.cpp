// main_centralmix.cpp — entrypoint del programa AUTOCONTENIDO "centralmix".
//
// Programa estilo 2025 (FSM del delantero + primitivas de motor directas) pero
// alimentado por los datos de TOP/DOWN (SIN world_model). Estructura mínima:
//
//   setup(): inicializa comunicación (TOP/DOWN), motores y FSM.
//   loop():  drena comunicación → avanza FSM.
//
// CONTRATO / SKELETON. La lógica vive en mix_comm.cpp / mix_motors.cpp /
// mix_fsm.cpp. Este archivo NO contiene lógica de juego (solo init + debug print).
//
// Para compilar como env propio en platformio.ini: src_filter que incluya SOLO
// src/centralmix/ + src/shared/ (proto, types, line_view, pose_view) y build_flags
// -DROBOT... La fuente de heading se elige con -DMIX_HEADING_SNAPSHOT (R1 con gyro)
// o -DMIX_HEADING_OTOS. Ver mix_config.h.
//
// DEBUG por USB Serial (115200): un print THROTTLEADO (MIX_DBG_PERIOD_MS) con los
// datos clave de g_io. Sirve para banco: verificar el heading del TOP (¿llega
// hdg_valid=1?, ¿heading_err se mueve al girar el robot?) y tunear la patada recta
// (¿otos_hdg se mueve?, ¿otos_conf>0?). Guardado con `if (Serial)`: si NO hay PC
// conectada NO cuesta nada (Teensy descarta el USB sin bloquear). Para apagarlo del
// todo, compilar con -DMIX_NO_DEBUG_SERIAL.
//
// ⚠️ NO TESTEADO EN HARDWARE.

#include <Arduino.h>

#include "mix_config.h"
#include "mix_io.h"
#include "mix_comm.h"
#include "mix_motors.h"

// Selección de FSM del delantero (mutuamente excluyentes):
//   DEFAULT (sin flag)      → mix_fsm.cpp     : port FIEL del delantero 2025 (apuntar→avanzar→orbitar).
//   con -DMIX_ATTACK_EDGE   → mix_fsm_edge.cpp: rodeo REACTIVO estilo Edge (1 estado, full velocidad).
// Es 100% aditivo: el flag SOLO cambia qué FSM se llama; el resto de centralmix es idéntico.
#ifdef MIX_ATTACK_EDGE
#include "mix_fsm_edge.h"
#else
#include "mix_fsm.h"
#endif

#include <math.h>   // sqrtf (distancia pelota en el debug)

#ifndef MIX_NO_DEBUG_SERIAL
static constexpr unsigned long MIX_DBG_PERIOD_MS = 150;  // ~6,7 Hz (no floodear ni frenar el loop)
static unsigned long s_dbg_prev_ms = 0;

// Imprime una línea con los datos clave de g_io. Solo si hay PC conectada (`if (Serial)`).
static void mix_debug_print() {
    using namespace iitasoccer::mix;
    const unsigned long now = millis();
    if (now - s_dbg_prev_ms < MIX_DBG_PERIOD_MS) return;
    s_dbg_prev_ms = now;
    if (!Serial) return;   // sin host USB → no imprimir (Teensy no bloquea, pero ahorramos)

    const float dist = sqrtf(g_io.ball_x_cm * g_io.ball_x_cm +
                             g_io.ball_y_cm * g_io.ball_y_cm);

    // Pelota
    Serial.print("ball vis="); Serial.print(g_io.ball_visible);
    Serial.print(" x=");       Serial.print(g_io.ball_x_cm, 1);
    Serial.print(" y=");       Serial.print(g_io.ball_y_cm, 1);
    Serial.print(" ang=");     Serial.print(g_io.angulo_pelota_deg, 0);
    Serial.print(" d=");       Serial.print(dist, 0);
    Serial.print(" vx=");      Serial.print(g_io.ball_vx_cm_s, 0);   // velocidad pelota (rodeo Edge)
    Serial.print(" vy=");      Serial.print(g_io.ball_vy_cm_s, 0);
    // Heading del TOP (snapshot) — lo que verifica el pendiente #2
    Serial.print(" | hdg=");      Serial.print(g_io.heading_deg, 1);
    Serial.print(" hvalid=");     Serial.print(g_io.heading_valid);
    Serial.print(" herr=");       Serial.print(g_io.heading_error_deg, 1);
    // OTOS (lo que usa la patada recta) + diagnóstico de la corrección de la patada
    Serial.print(" | otos_hdg="); Serial.print(g_io.otos_heading_deg, 1);
    Serial.print(" otos_conf=");  Serial.print(g_io.otos_confidence);
    Serial.print(" kick_err=");   Serial.print(g_io.kick_err_deg, 1);
    Serial.print(" kick_corr=");  Serial.print(g_io.kick_corr);
    Serial.print(" giro_atras="); Serial.print(g_io.giro_atras_dir);
    // Arco rival + línea + árbitro + enlaces
    Serial.print(" | goalOpp vis="); Serial.print(g_io.goal_opp_visible);
    Serial.print(" ang=");           Serial.print(g_io.goal_opp_angle, 0);
    Serial.print(" | line p=");      Serial.print(g_io.line_present);
    Serial.print(" ang=");           Serial.print(g_io.line_angle_deg, 0);
    Serial.print(" | match=");       Serial.print(g_io.match_running);
    Serial.print(" topFresh=");      Serial.print(g_io.top_link_fresh);
    Serial.print(" downFresh=");     Serial.print(g_io.down_link_fresh);
#ifdef MIX_ATTACK_EDGE
    Serial.print(" | EDGE=");        Serial.println(iitasoccer::mix::mix_fsm_edge_estado_nombre());
#else
    Serial.println();
#endif
}
#endif  // MIX_NO_DEBUG_SERIAL

void setup() {
#ifndef MIX_NO_DEBUG_SERIAL
    Serial.begin(115200);   // USB debug (no espera al host: arranca igual sin PC)
#endif
    iitasoccer::mix::mix_comm_init();
    iitasoccer::mix::mix_motors_init();
#ifdef MIX_ATTACK_EDGE
    iitasoccer::mix::mix_fsm_edge_init();
#else
    iitasoccer::mix::mix_fsm_init();
#endif
}

void loop() {
    iitasoccer::mix::mix_comm_tick();
#ifdef MIX_ATTACK_EDGE
    iitasoccer::mix::mix_fsm_edge_tick();
#else
    iitasoccer::mix::mix_fsm_tick();
#endif
#ifndef MIX_NO_DEBUG_SERIAL
    mix_debug_print();
#endif
}
