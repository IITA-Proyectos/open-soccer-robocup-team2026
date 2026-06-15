// motor_slew.h — Control SUAVE de motores = limitador de slew/rampa por eje (PURO).
//
// QUÉ ES Y POR QUÉ EXISTE (Capa 2 del lazo RT de la CENTRAL, pedido Gustavo):
// El lazo principal de la CENTRAL recibe comandos de motor que pueden SALTAR de
// golpe cuando cambian los datos de TOP/DOWN (la pelota aparece, la FSM cambia de
// estado, el rumbo se corrige). Aplicar ese salto crudo a las 3 ruedas omni de
// una = pico de corriente, derrape, y pérdida de tracción (la rueda "patina" en
// vez de empujar). Este módulo ACERCA el comando previo al nuevo de a poco, como
// mucho `max_delta_per_tick` por eje y por tick → la potencia sube/baja en RAMPA
// en vez de en escalón. Es el equivalente firmware de "no pisar el acelerador a
// fondo de un saque": el robot acelera parejo y la dirección se mantiene limpia.
//
// QUÉ NO ES: NO es un PID ni un filtro de la PLANTA (eso es dinamica-omni-3-ruedas
// + control-pid-zona-muerta). Es puramente un LIMITADOR DE PENDIENTE sobre el
// comando, aguas arriba del mixer/piso de PWM. Trabaja en el espacio de EJES del
// robot (vx = strafe, vy = avance, omega = giro), ANTES de la cinemática que
// reparte a las 3 ruedas — así una rampa pareja en los ejes se traduce en un
// arranque parejo y SIN deriva parásita (rampar las ruedas por separado sí
// metería deriva). El orden de los 3 ejes lo fija quien llama; el módulo solo
// rampa cmd[i] contra prev[i], índice por índice, sin asumir significado.
//
// LOS REFLEJOS NO RAMPEAN (regla dura del lazo RT): la salida de línea blanca y
// el STOP del árbitro son REFLEJOS de alta prioridad que deben aplicarse YA, sin
// pasar por la rampa (si rampearan, el robot seguiría cruzando la línea mientras
// la potencia sube despacio = se sale de la cancha). Para eso está el BYPASS:
//   • slew_force(state, target) → fija prev = target de una (el próximo
//     slew_apply ya arranca desde el destino; el reflejo "teletransporta" la rampa).
//   • slew_reset(state)         → pone prev = 0 (motores quietos, p.ej. en STOP).
// Quien llama: en un reflejo, escribe el comando crudo a los motores Y llama a
// slew_force/slew_reset para que la rampa NO "tire para atrás" en el tick siguiente.
//
// PURO: sin Arduino, solo enteros (int16/int32). Host-testeable:
//   bash scripts/run-host-tests.sh test_motor_slew
// Determinista y no bloqueante: O(3), sin asignación, sin estado global, sin delay.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Cuántos ejes rampa este limitador. La CENTRAL omni usa 3: vx, vy, omega.
static const int SLEW_AXES = 3;

// Estado persistente entre ticks: el último comando ya rampado (lo que "salió"
// el tick anterior). Arranca en cero (motores quietos). Una instancia por robot.
struct SlewState {
    int16_t prev[SLEW_AXES];
};

// Configuración (tuneable en banco, NO toca el mecanismo): cuánto puede cambiar
// como MÁXIMO cada eje por tick. Más chico = rampa más suave (arranque más lento,
// menos derrape); más grande = más reactivo (al límite ≈ sin rampa). Por eje
// porque el giro (omega) y el strafe (vx/vy) toleran pendientes distintas.
// Se interpreta como MAGNITUD: el valor efectivo es |max_delta_per_tick[i]|.
struct SlewCfg {
    int16_t max_delta_per_tick[SLEW_AXES];
};

// Deja TODOS los ejes en cero (motores quietos). Usar al bootear y como reflejo
// de STOP del árbitro: el próximo slew_apply arranca rampando desde 0.
inline void slew_reset(SlewState& s) {
    for (int i = 0; i < SLEW_AXES; ++i) s.prev[i] = 0;
}

// BYPASS de la rampa: fija el estado interno directo en `target` (los 3 ejes).
// Tras esto, slew_apply(.., target, ..) devuelve `target` sin recortar (ya está
// "ahí"). Es como el reflejo SALTA la rampa: salida de línea / cambio inmediato
// sin la demora del slew. Pasar el MISMO comando crudo que se mandó a los motores.
inline void slew_force(SlewState& s, const int16_t target[SLEW_AXES]) {
    for (int i = 0; i < SLEW_AXES; ++i) s.prev[i] = target[i];
}

// UN TICK de rampa. Acerca el estado interno (prev) hacia `cmd`, como mucho
// max_delta por eje, y ESCRIBE el resultado de vuelta en cmd[] (in-place) además
// de dejarlo guardado en s.prev para el próximo tick.
//
// Garantías por eje i:
//   • |salida - prev_anterior| <= |cfg.max_delta_per_tick[i]|  (límite de pendiente)
//   • si el salto pedido es chico (<= max_delta) → salida == cmd[i] (llega en 1 tick)
//   • si max_delta <= 0 → ese eje queda CONGELADO en su prev (no se mueve);
//     elegí esto a propósito: una config en 0 NO debe "saltar" (sería lo opuesto
//     a un limitador). Para salto inmediato real está slew_force, no delta=0.
//   • nunca sobrepasa el objetivo (no oscila alrededor de cmd).
// No bloquea, no asume período fijo (la "velocidad" de rampa la fija max_delta,
// pensado para una tasa de tick FIJA del lazo de control).
inline void slew_apply(SlewState& s, int16_t cmd[SLEW_AXES], const SlewCfg& cfg) {
    for (int i = 0; i < SLEW_AXES; ++i) {
        // max_delta como magnitud (una config negativa no invierte el sentido).
        int32_t step = cfg.max_delta_per_tick[i];
        if (step < 0) step = -step;

        const int32_t target = cmd[i];
        const int32_t prev   = s.prev[i];
        const int32_t diff   = target - prev;

        int32_t out;
        if (step == 0) {
            // Eje congelado: sin autoridad de rampa, se queda donde estaba.
            out = prev;
        } else if (diff > step) {
            out = prev + step;          // sube, recortado al paso máximo
        } else if (diff < -step) {
            out = prev - step;          // baja, recortado al paso máximo
        } else {
            out = target;               // salto chico: llega exacto en este tick
        }

        s.prev[i] = static_cast<int16_t>(out);
        cmd[i]    = static_cast<int16_t>(out);
    }
}

}  // namespace iitasoccer
