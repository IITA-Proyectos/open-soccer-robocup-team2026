// blackbox.h — CAJA NEGRA de la CENTRAL (banco/demo 2026-06-11, pedido Gustavo).
//
// Problema: el robot se mueve rápido y "para todos lados" — a ojo no se puede
// diagnosticar, y no siempre hay cancha. Solución: grabar TODO lo que la CENTRAL
// decide y aplica (estado FSM, snapshot que vio, comando vx/vy/ω, PWM real por
// motor, línea, heading) en un RING BUFFER EN RAM durante la corrida (cero I/O,
// cero impacto en el loop de 100 Hz) y VOLCARLO por USB como CSV al terminar
// (el USB del Teensy es ~1 MB/s: el volcado de 90 s tarda <1 s).
//
// Diseño v1 (RAM, sin SD): ~50 Hz × 90 s de historia circular (los ÚLTIMOS 90 s
// quedan siempre; lo viejo se pisa). La CENTRAL es Teensy 4.1: el buffer vive en
// DMAMEM (RAM2, 512 KB — el loop usa RAM1, no compiten). Fase 2 (post-demo): la
// 4.1 tiene ranura microSD integrada → SdFat para sesiones de horas/partidos.
//
// Uso (gateado por -DCENTRAL_BLACKBOX, envs *_bb):
//   - Graba solo (siempre armada).
//   - Al pasar de RUN → STOP (juez o 's'): AUTO-VOLCADO del CSV por USB.
//   - 'd' por el monitor = volcar a demanda · 'x' = borrar y re-armar.
//   - PC: tools/blackbox/leer_caja_negra.py COMx  (o `pio device monitor -f log2file`).
//
// Doc del experimento: docs/pruebas-banco/CAJA-NEGRA.md
#pragma once
#ifdef CENTRAL_BLACKBOX

#include <stdint.h>
#include "types.h"

namespace iitasoccer {

// Llamar UNA vez por pasada del lazo de control (100 Hz), DESPUÉS de aplicar el
// comando a los motores. Internamente decima a ~50 Hz y maneja el auto-volcado
// en el flanco RUN→STOP. cmd = lo que strategy_tick decidió este tick.
void blackbox_tick(const MotorCommand& cmd);

// Variante para el FRENO DE BORDE de main (EMERGENCY_LINE): graba el tick con
// cmd=0 y el flag de emergencia (columna 'emerg' del CSV). Sin esto la caja
// quedaba ciega justo durante el freno (auditoría 2026-06-11).
void blackbox_tick_emergency();

// Volcar el contenido como CSV por USB (bloqueante ~0,5-1 s; usar con el robot parado).
void blackbox_dump();

// Borrar la historia y re-armar (típico: antes de una corrida nueva, comando 'x').
void blackbox_reset();

}  // namespace iitasoccer

#endif  // CENTRAL_BLACKBOX
