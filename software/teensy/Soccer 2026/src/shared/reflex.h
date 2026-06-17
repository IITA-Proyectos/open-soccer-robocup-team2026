// reflex.h — Capa 1 del lazo RT de la CENTRAL: maquina de PRIORIDADES (PURO).
//
// QUE ES Y POR QUE EXISTE (Capa 1 del lazo RT, pedido Gustavo 2026-06-14)
// ----------------------------------------------------------------------
// El loop de la CENTRAL tiene 4 capas: plomeria (RX/pizarra) -> REFLEJOS ->
// control suave (slew) -> FSM. Los REFLEJOS son las respuestas de alta prioridad
// que PREEMPTAN al resto del loop: cuando el arbitro para el partido o el robot
// esta por salir de la cancha por una linea blanca, el robot NO puede esperar al
// tick de 100 Hz de la FSM ni pasar por la rampa de slew — tiene que actuar YA.
//
// Hoy esto vive HARDCODEADO en main_central.cpp:388-415 (freno de borde con
// anti-latch 350 ms, fix Maria 2026-06-14). Funciona bien, pero:
//   • El STOP del arbitro NO es un reflejo: corre por el camino LENTO dentro del
//     tick de strategy a 100 Hz (gate g_since_strategy_tick >= 10). Si el arbitro
//     para mientras el robot esta corriendo, hay 0-10 ms de demora extra.
//   • La maquina del borde esta acoplada al loop (static locals en main_central):
//     no se puede testear host, ni reusar en el rewrite del loop.
//
// Este modulo FORMALIZA esa maquina como un POD + 1 funcion pura, con TRES
// prioridades ESTRICTAS:
//
//   P-ALTA  STOP_MOTORS   arbitro paro (match_running=false): preempta TODO,
//                         incluso BRAKE_EDGE. Resetea el estado del borde.
//   P-MEDIA BRAKE_EDGE    borde de linea inminente: freno activo + return,
//                         con anti-latch de 350 ms (luego RELEASE_TO_FSM).
//                         RELEASE_TO_FSM no es "sin reflejo": es "el reflejo ya
//                         hizo su parte, ahora dejame correr la FSM ESCAPE para
//                         despegarme de la linea".
//   P-BAJA  NONE          sin reflejo: la FSM manda.
//
// REGLA DE ORO DEL HANDOFF (la leccion del deadlock de Maria, 2026-06-14):
// el reflejo es DUENO solo mientras BRAKE_EDGE, y ese dominio esta ACOTADO a
// edge_max_ms por construccion. Pasados los 350 ms emite RELEASE_TO_FSM, que el
// caller traduce a "NO hacer return": dejar correr strategy_tick para que el
// estado ESCAPE despegue al robot. El handoff es por el VALOR DE RETORNO
// (RELEASE vs BRAKE), no por una bandera que alguien tiene que acordarse de bajar.
// El estado se re-arma solo cuando edge_now baja.
//
// CAMBIO DE CONDUCTA vs HOY (banco lo valida — riesgo R5 del diseno): hoy el
// STOP NO preempta el borde (corre dentro de strategy). Con esto SI preempta:
// si el arbitro para mientras el robot esta frenando un borde, ahora corta a
// motors_stop() (coast) en vez de seguir el brake. Es lo correcto (partido
// parado = motores apagados, sin excepciones), pero hay que verificar en banco
// que el robot no se desliza hacia la linea por inercia tras el corte.
//
// PURO: solo <stdint.h>. POD + funciones libres. Zero-init = estado valido.
// El tiempo entra como uint32 de millis() (wrap-safe por resta unsigned, igual
// que state_timer.h). Host-testeable:
//   bash scripts/run-host-tests.sh test_reflex
//
// COMO CABLEAR (post-Incheon, NO incluido en esta entrega): el modulo es PURO
// y SIN cablear. El loop nuevo (rewrite post-mundial) llamara:
//   const ReflexInputs in = {world_model_match_running(),
//                            world_model_imminent_exit() && world_model_line_is_fresh()};
//   switch (reflex_arbitrate(g_reflex, in, millis())) {
//     case STOP_MOTORS:    motors_stop();  slew_reset(g_slew); return;
//     case BRAKE_EDGE:     motors_brake(); slew_reset(g_slew); return;
//     case RELEASE_TO_FSM: break;  // dejar correr la FSM (ESCAPE)
//     case NONE:           break;  // FSM normal
//   }
// El gate previsto es -DCENTRAL_REFLEX_LAYER (default OFF; lo activa el equipo
// cuando re-titula el cableado con la caja negra + CSV de Maria como regresion).
//
// PRECEDENTE: mismo molde que loop_monitor.h / state_timer.h / motor_slew.h.
// No bloquea, no asume periodo fijo, no asigna memoria, sin estado global.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Anti-latch del freno de borde: tiempo MAXIMO que puede mantener BRAKE_EDGE
// activo desde que se vio el primer edge_now. Pasado esto, el reflejo emite
// RELEASE_TO_FSM (el caller deja correr la FSM ESCAPE) y se mantiene en RELEASE
// hasta que edge_now baje (el momento del roll-out ya se freno; mantener el
// freno solo CONGELA al arquero sobre su linea).
//
// 350 ms es el valor del fix Maria 2026-06-14, validado en banco con CSV. NO
// cambiar sin re-validar contra el CSV de regresion (test_reflex lo cubre).
constexpr uint32_t REFLEX_EDGE_BRAKE_MAX_MS = 350u;

// Que tiene que hacer el caller en esta vuelta. El reflex_arbitrate devuelve
// UNO de estos; el caller hace el switch y aplica la accion fisica.
enum class ReflexAction : uint8_t {
    NONE = 0,         // sin reflejo: dejar correr la FSM normal
    STOP_MOTORS,      // arbitro paro: motors_stop() y return
    BRAKE_EDGE,       // borde inminente: motors_brake() y return (anti-latch activo)
    RELEASE_TO_FSM,   // borde sigue inminente PERO el anti-latch ya vencio:
                      // NO frenar mas, NO return; dejar correr la FSM ESCAPE
                      // para despegar al robot. El reflejo "solto" el control.
};

// Estado persistente del reflejo del borde entre vueltas. Zero-init = sin armar.
// edge_since_ms == 0 es el centinela "borde no detectado en este tramo"; el
// re-arme natural (edge_now baja -> reset a 0) lo restablece automaticamente.
struct ReflexState {
    uint32_t edge_since_ms;   // millis() del primer edge_now del tramo actual; 0 = sin armar
    bool     edge_released;   // true tras agotar el anti-latch: mantener RELEASE hasta edge_now=false
};

// Entradas del reflejo en esta vuelta. El caller pre-ANDea line_fresh con
// imminent_exit para edge_now (un borde stale NO es un borde — si el DOWN se
// cayo, no queremos frenar para siempre).
struct ReflexInputs {
    bool match_running;   // arbitro en PLAY (false = STOP / pausa / pre-match)
    bool edge_now;        // borde inminente Y fresco (caller ya lo pre-ANDeo con line_fresh)
};

// Decide la accion de esta vuelta. Pura, O(1), sin asignacion, sin delay.
//
// Prioridad estricta:
//   1. !match_running -> STOP_MOTORS (resetea estado del borde, gana SIEMPRE)
//   2. edge_now y dentro de los edge_max_ms -> BRAKE_EDGE
//   3. edge_now pero ya vencio el anti-latch -> RELEASE_TO_FSM (sostenido hasta edge_now=false)
//   4. !edge_now -> NONE (y resetea el estado del borde para el proximo tramo)
//
// edge_max_ms permite override para tests; en produccion usar el default
// REFLEX_EDGE_BRAKE_MAX_MS (350 ms, fix Maria validado en banco).
//
// WRAP-SAFE: la resta (now_ms - edge_since_ms) es unsigned -> el wrap de
// millis() (cada ~49.7 dias) NO rompe el anti-latch siempre que la espera dure
// menos que el wrap (always true: el anti-latch son ms, no dias).
inline ReflexAction reflex_arbitrate(ReflexState& s, const ReflexInputs& in,
                                     uint32_t now_ms,
                                     uint32_t edge_max_ms = REFLEX_EDGE_BRAKE_MAX_MS) {
    // P-ALTA: STOP del arbitro. Gana sobre TODO. Resetea el estado del borde
    // para que al volver a RUN el reflejo arranque limpio (sin "memoria" del
    // borde que estaba viendo cuando se paro el partido).
    if (!in.match_running) {
        s.edge_since_ms = 0u;
        s.edge_released = false;
        return ReflexAction::STOP_MOTORS;
    }

    // P-MEDIA: borde de linea con anti-latch (replica EXACTA de la maquina viva
    // en main_central.cpp:388-415, fix Maria 2026-06-14).
    if (in.edge_now) {
        if (s.edge_since_ms == 0u) {
            // Primer edge_now del tramo: arrancar la cuenta del anti-latch.
            s.edge_since_ms = now_ms;
            s.edge_released = false;
            // OJO: si now_ms == 0 (boot exacto), edge_since queda en 0 = centinela.
            // El reflejo dispara igual (BRAKE_EDGE abajo) y el proximo tick re-arma
            // con un millis() > 0. No es un bug observable (es 1 tick de latencia
            // en el peor caso al borde exacto del overflow del timer).
        }
        if (!s.edge_released && (now_ms - s.edge_since_ms) >= edge_max_ms) {
            // Anti-latch vencio: el momento del roll-out ya se freno. Soltar.
            s.edge_released = true;
        }
        return s.edge_released ? ReflexAction::RELEASE_TO_FSM
                               : ReflexAction::BRAKE_EDGE;
    }

    // !edge_now: el borde bajo. Re-armar el reflejo para el proximo tramo.
    s.edge_since_ms = 0u;
    s.edge_released = false;
    return ReflexAction::NONE;
}

}  // namespace iitasoccer
