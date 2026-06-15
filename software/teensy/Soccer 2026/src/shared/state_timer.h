// state_timer.h — Timeout/watchdog POR ESTADO, NO BLOQUEANTE (PURO, host-testeable).
//
// Por qué existe (lazo RT de la CENTRAL — pedido de Gustavo 2026-06-14)
// --------------------------------------------------------------------
// La FSM táctica de la CENTRAL espera que pasen cosas: "voy a buscar la pelota
// hasta tenerla", "centro al arquero hasta ver la línea". Pero un sensor puede no
// disparar nunca (la pelota se fue, el rival la tapó, el ToF se quedó stale). Si la
// FSM se queda esperando para SIEMPRE, el robot se planta en cancha. La regla dura
// del lazo es: NUNCA bloquear, JAMÁS un delay(). Entonces cada movimiento de la FSM
// necesita un RELOJ DE ARENA que diga "ya esperé demasiado, salgo y hago otra cosa"
// — sin frenar el lazo de control ni un microsegundo.
//
// Esto es ese reloj de arena. La FSM, al ENTRAR a un estado, lo "arma" con
// st_arm(). En cada vuelta del loop pregunta st_expired(): si todavía no venció,
// sigue esperando el sensor; si venció, hace la transición de escape. Preguntar es
// una resta de enteros — barato, no bloquea, se puede llamar a 100 Hz sin costo.
//
// Cómo encaja en las 4 capas del lazo RT:
//   • NO es un reflejo (capa 1): los reflejos —START/STOP, toque de línea— preemptan
//     y no esperan a nadie. Esto es para la capa 3 (FSM prolija), donde CADA
//     movimiento tiene su watchdog no bloqueante.
//   • El timeout (timeout_ms) vive en la TABLA de la jugada, separado del mecanismo:
//     el StateTimer es el mecanismo (cuándo se armó), el timeout_ms es el parámetro
//     tuneable que la FSM le pasa al preguntar. Así se titulan tiempos en banco sin
//     tocar la lógica.
//
// PURO: sin Arduino. POD + funciones libres → .bss zero-init y compila con g++ host.
// El tiempo entra como uint32 de millis() (el caller decide la fuente: millis() en
// el Teensy, un contador falso en los tests); el módulo solo hace aritmética
// WRAP-SAFE sobre ese contador (clave: millis() da la vuelta a los ~49,7 días, pero
// la resta unsigned igual da el intervalo real mientras la espera dure < 49,7 días,
// lo cual SIEMPRE se cumple para un timeout de jugada de unos segundos).

#pragma once
#include <stdint.h>

namespace iitasoccer {

// El reloj de arena de un estado. Guarda SOLO el instante en que se armó. El cuánto
// hay que esperar (timeout_ms) NO vive acá: se lo pasa la FSM al preguntar, porque
// es un parámetro de la jugada (tabla), no del mecanismo.
//
// Zero-init (.bss) == estado válido: armed=false → un timer que nunca se armó NUNCA
// se considera expirado (st_expired devuelve false). Eso es a propósito: un estado
// sin watchdog (timeout 0 / sin armar) simplemente no tiene escape por tiempo.
struct StateTimer {
    uint32_t arm_ms;   // millis() del momento en que se armó (st_arm)
    bool     armed;    // ¿se armó alguna vez? (false hasta el primer st_arm)
};

// Armar / re-armar el reloj de arena. Se llama al ENTRAR a un estado de la FSM (o
// para reiniciar la espera dentro del mismo estado, p.ej. cuando llegó un dato
// parcial y querés "resetear el reloj"). Re-armar pisa el instante anterior: la
// cuenta empieza de cero desde now.
inline void st_arm(StateTimer& t, uint32_t now) {
    t.arm_ms = now;
    t.armed  = true;
}

// Cuánto tiempo pasó (ms) desde que se armó. Resta UNSIGNED → wrap-safe.
// Si nunca se armó, devuelve 0 (todavía no empezó a contar nada).
inline uint32_t st_elapsed(const StateTimer& t, uint32_t now) {
    if (!t.armed) return 0u;
    return now - t.arm_ms;   // unsigned: maneja el wrap de millis() automáticamente
}

// ¿Ya venció? true SOLO si está armado Y pasó al menos timeout_ms desde que se armó.
// Esta es la pregunta que la FSM hace en CADA vuelta del loop para decidir si se
// queda esperando el sensor o dispara la transición de escape.
//
// Detalles que importan:
//   • Recién armado (elapsed 0) con timeout > 0 → NO expira (hay que esperar).
//   • Sin armar → NUNCA expira (un estado sin watchdog no escapa por tiempo).
//   • timeout_ms == 0 sobre un timer armado → expira de inmediato (escape instantáneo,
//     útil para estados de "paso fugaz"). Por eso el corte es >= y no >.
inline bool st_expired(const StateTimer& t, uint32_t now, uint32_t timeout_ms) {
    if (!t.armed) return false;
    return st_elapsed(t, now) >= timeout_ms;
}

}  // namespace iitasoccer
