// rx_calib_defer.h — Slot ÚLTIMO-GANA que difiere el calibrate bloqueante (PURO, host-testeable).
//
// Por qué existe (reingeniería RT de la placa DOWN — ARQUITECTURA-LAZO-DOWN-RT §8.1, F5)
// --------------------------------------------------------------------------------------
// La DOWN recibe comandos por UART desde la CENTRAL. Uno de esos comandos es "recalibrá
// los sensores de línea" (el piso blanco/verde cambió de cancha, hay que volver a tomar la
// referencia). PERO ese calibrate es un trabajo PESADO Y BLOQUEANTE: barre los 32 sensores,
// promedia muestras, escribe la referencia — del orden de ~320 ms. Tres décimas de segundo.
//
// El problema fino: ese calibrate NO puede correr DENTRO del handler de RX. El handler de RX
// se invoca al vuelo cuando llega un frame por el puerto serie; tiene que setear lo que haya
// que setear y RETORNAR en microsegundos. Si el handler se pone a calibrar 320 ms ahí mismo:
//   • el lazo de control de la DOWN (motores, línea) queda CONGELADO 320 ms → el robot se va
//     de mambo en pleno partido, sigue derecho contra la línea sin frenar;
//   • el buffer de RX se desborda mientras tanto → se pierden frames, el FrameDecoder pierde
//     sincronía;
//   • peor: calibrar mientras el robot se MUEVE toma una referencia basura (los sensores ven
//     manchas distintas en cada instante). El calibrate sólo sirve con el robot QUIETO.
//
// La solución: separar INTENCIÓN de EJECUCIÓN (este módulo)
// --------------------------------------------------------------------------------------
// Esto es un BUZÓN de un solo casillero. El handler de RX, cuando llega el comando, NO calibra:
// sólo deja una NOTA en el buzón ("alguien pidió calibrar, tipo K") con rxcd_request() y
// retorna en <5 µs. El trabajo pesado lo hace OTRO lugar del firmware, FUERA del handler de RX
// y FUERA del lazo crítico — en un punto del superloop donde ya se garantizó que el robot está
// quieto. Ese lugar pregunta rxcd_take(): si hay una nota la consume (y limpia el buzón) y
// recién AHÍ ejecuta el calibrate bloqueante; si no hay nota, sigue de largo sin costo.
//
// ÚLTIMO-GANA (last-wins), a propósito
// --------------------------------------------------------------------------------------
// El buzón tiene UN solo casillero. Si llegan DOS pedidos de calibrar antes de que el
// ejecutor los consuma, el segundo PISA al primero — gana el último `kind`. Es lo correcto
// para un comando idempotente como "calibrá": calibrar dos veces seguidas no tiene sentido,
// alcanza con hacerlo UNA vez con la intención más reciente. No encolamos pedidos (una cola
// crecería sin techo si el ejecutor se atrasa, y re-ejecutaría calibrates viejos sin sentido).
// El último pedido es el que refleja lo que la CENTRAL quiere AHORA.
//
// fail-safe: si nadie llama rxcd_take(), la peor consecuencia es que el calibrate no ocurre
// (pending queda seteado, inerte). NUNCA dispara un calibrate por sí solo; NUNCA bloquea. Un
// buzón sin vaciar no rompe nada — sólo significa "calibración pendiente todavía no atendida".
//
// PURO: sin Arduino, sin Wire, sin RTOS. POD + funciones libres → .bss zero-init == buzón
// vacío válido (pending=false). Compila con g++ host. El `kind` es un uint8 opaco para este
// módulo: lo interpreta el ejecutor (p.ej. 0 = calibrar todo, 1 = sólo anillo externo, etc.);
// acá sólo se transporta byte-idéntico, sin reinterpretarlo.
//
// LÍMITE HONESTO (un productor, un consumidor):
//   • Diseñado para UN escritor (el handler de RX) y UN lector (el ejecutor del superloop).
//     No es un seqlock: el `pending`/`kind` se asume escrito y leído fuera de preempción mutua
//     (el handler de RX y el ejecutor no corren a la vez en este firmware superloop). Si algún
//     día el RX pasa a ISR que preempta al ejecutor, `pending` debería ser volatile y la
//     toma/seteo ordenarse — eso es glue de integración, NO va acá (el módulo es PURO).

#pragma once
#include <stdint.h>

namespace iitasoccer {

// El buzón de un casillero. Guarda SI hay un pedido pendiente y de qué tipo.
//
// Zero-init (.bss) == buzón vacío válido: pending=false → rxcd_take() devuelve false (no hay
// nada que ejecutar). Un buzón recién construido nunca dispara un calibrate fantasma.
struct RxCalibDefer {
    bool    pending;   // ¿hay un pedido de calibrar esperando ser ejecutado?
    uint8_t kind;      // tipo de calibración pedida (opaco acá; lo interpreta el ejecutor)
};

// rxcd_request(slot, kind) — el HANDLER DE RX deja la nota y se va.
//
// Setea pending=true y guarda el `kind`. ÚLTIMO-GANA: si ya había un pedido sin consumir,
// este lo pisa (gana el kind más reciente). Es la operación que el handler de RX hace al
// vuelo: marca la intención en O(1), sin tocar hardware, sin bloquear, y retorna.
inline void rxcd_request(RxCalibDefer& s, uint8_t kind) {
    s.kind    = kind;     // gana el último: pisa cualquier kind anterior no consumido
    s.pending = true;
}

// rxcd_take(slot, &kind) — el EJECUTOR consume la nota (si la hay).
//
// Si había un pedido pendiente: copia su `kind` en *kind_out, LIMPIA el buzón (pending=false)
// y devuelve true → el caller ejecuta el calibrate bloqueante AHORA (con el robot quieto).
// Si no había nada: devuelve false y NO toca *kind_out → el caller sigue de largo.
//
// Limpiar al tomar (take-and-clear) garantiza que un mismo pedido se ejecuta UNA sola vez:
// la próxima vuelta del ejecutor verá el buzón vacío hasta que llegue un request nuevo.
inline bool rxcd_take(RxCalibDefer& s, uint8_t* kind_out) {
    if (!s.pending) {
        return false;            // buzón vacío: nada que ejecutar, no tocamos kind_out
    }
    if (kind_out != nullptr) {
        *kind_out = s.kind;      // entregamos el kind del pedido más reciente
    }
    s.pending = false;           // vaciar: el pedido queda consumido, no se re-ejecuta
    return true;
}

// rxcd_pending(slot) — ¿hay un pedido esperando? (consulta sin consumir).
// Útil para telemetría/diagnóstico ("hay un calibrate encolado") sin disparar la ejecución.
inline bool rxcd_pending(const RxCalibDefer& s) {
    return s.pending;
}

}  // namespace iitasoccer
