// mix_fsm_edge.h — Máquina de estados DELANTERO estilo "Edge" (回り込み / rodeo reactivo).
//
// ALTERNATIVA a la FSM 2025 de mix_fsm.cpp. Se elige en main_centralmix.cpp con el flag
// -DMIX_ATTACK_EDGE. SIN el flag, centralmix corre EXACTAMENTE como hoy (mix_fsm.cpp); este
// archivo ni se llama → 100% aditivo, no toca lo que ya funciona.
//
// POR QUÉ EXISTE: la FSM 2025 (apuntar→avanzar→orbitar lento, sin anticipar) es lenta para
// posicionarse y no llega a una pelota en movimiento. Edge (campeón mundial Lightweight 2024)
// se pone detrás de la pelota con UNA fórmula reactiva a full velocidad. Esta FSM porta ese
// enfoque, adaptado SIN pateador (empuje por inercia).
//
// REUSA todo lo que ya está probado de centralmix: mix_io (datos TOP/DOWN), las primitivas de
// mix_motors (girar/avanzar_patear/retroceder/kickoff + la nueva mix_mover_vector), el escape
// de línea (retroceder1/2/3) y el gate del árbitro RCJ. SOLO cambia el "cómo me posiciono".
//
// ESTADOS:
//   KICKOFF    → arranque del partido (ve pelota→RODEAR; no→medialuna→BUSCAR). 1 vez.
//   BUSCAR     → gira en el lugar buscando la pelota.
//   RODEAR     → ★ EL CORAZÓN ★ rodeo reactivo (mix_edge) + mira al arco. A full velocidad.
//   EMPUJAR    → empuje recto al arco por inercia (avanzar_patear), tiempo fijo.
//   RETROCEDER → se despega corto post-empuje y vuelve a BUSCAR.
//   LINEA_1/2/3→ escape de línea blanca (retroceder1/2/3 bench-tuneados de Elías).
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
