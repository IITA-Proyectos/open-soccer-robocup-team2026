// mix_fsm_edge.h — Máquina de estados "CONDUCIR la pelota al arco" (centralconduce).
//
// ALTERNATIVA a la FSM 2025 de mix_fsm.cpp. Se elige con el flag -DMIX_ATTACK_EDGE. SIN el flag,
// la carpeta corre la FSM 2025 (mix_fsm.cpp) intacta → 100% aditivo.
//
// QUÉ HACE (NO es el rodeo de Edge — eso vive en centraledge): busca la pelota, la CENTRA al
// frente (para que al avanzar no se le escape), la LLEVA al arco (o al heading 0 si no ve el
// arco) manteniéndola adelante, y a <40 cm del arco APUNTA y PATEA (empuje por inercia). NO se
// posiciona detrás de la pelota: solo la centra y la conduce.
//
// REUSA lo probado de centralmix: mix_io (datos TOP/DOWN), las primitivas de mix_motors
// (girar/avanzar_patear/retroceder/kickoff + mix_mover_vector), el escape de línea y el árbitro.
//
// ESTADOS:
//   KICKOFF      → arranque (ve pelota→CENTRAR; no→medialuna→BUSCAR). 1 vez.
//   BUSCAR       → gira en el lugar buscando la pelota.
//   CENTRAR      → gira en el lugar hasta tener la pelota AL FRENTE.
//   CONDUCIR     → avanza hacia la pelota y, al tenerla cerca, la escolta al arco (o heading 0).
//   APUNTAR_ARCO → a <40 cm del arco: gira para apuntar al arco.
//   PATEAR       → empuje recto a fondo por inercia.
//   RETROCEDER   → se despega corto y vuelve a BUSCAR.
//   DETECTA_LINEA_1/2/3 → escape de línea (retroceder1/2/3 bench-tuneados de Elías).
//
// ⚠️ NO TESTEADO EN HARDWARE.

#pragma once

namespace iitasoccer {
namespace mix {

// Inicializa la FSM (estado = KICKOFF, sella timers). Llamar en setup() DESPUÉS de
// mix_comm_init() y mix_motors_init(). Análoga a mix_fsm_init().
void mix_fsm_edge_init();

// Un paso de la FSM: lee g_io, decide, actúa con las primitivas. Llamar cada loop()
// DESPUÉS de mix_comm_tick(). Respeta el árbitro RCJ (no mueve si !match_running).
void mix_fsm_edge_tick();

// Nombre del estado actual (para el debug por USB). No cuesta nada si no se usa.
const char* mix_fsm_edge_estado_nombre();

}  // namespace mix
}  // namespace iitasoccer
