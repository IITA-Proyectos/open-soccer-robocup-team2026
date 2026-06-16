// rx_byte_budget.h — Presupuesto de bytes/tick que ACOTA el WCET del tick de RX (PURO, host-testeable).
//
// Por qué existe (reingeniería RT de la placa DOWN — ARQUITECTURA-LAZO-DOWN-RT §8.1, F5)
// --------------------------------------------------------------------------------------
// La DOWN drena el puerto serie de la CENTRAL dentro de su superloop: en cada vuelta lee los
// bytes que llegaron y se los pasa al FrameDecoder, que arma los frames. El problema de
// tiempo real: ¿CUÁNTOS bytes puede haber para leer en una vuelta? Si todo va bien, un puñado.
// PERO el peor caso (WCET) es feo:
//   • un cable de UART con ruido eléctrico (motores cerca, masa flotante) inyecta basura a
//     chorro — el buffer de RX se llena de bytes espurios;
//   • la CENTRAL, por un bug o un reset, escupe una ráfaga continua;
//   • el FrameDecoder, ante basura, descarta byte por byte buscando el header de sincronía.
// Si el tick de RX dice "leo TODO lo que haya en el buffer", entonces en el peor caso procesa
// CIENTOS de bytes en una sola vuelta → esa vuelta del superloop se estira sin techo → el lazo
// de control de la DOWN (motores, línea) pierde su slot de tiempo → jitter o congelamiento.
// Un cable ruidoso NO puede tener el poder de monopolizar el loop. Eso viola el determinismo:
// el WCET del tick de RX tiene que estar ACOTADO, pase lo que pase en el cable.
//
// La solución: un PRESUPUESTO de bytes por tick (este módulo)
// --------------------------------------------------------------------------------------
// Le ponemos un TECHO: "este tick procesás como mucho max_per_tick bytes, ni uno más". El
// drenador pregunta rxbb_allow() ANTES de tomar cada byte: mientras quede presupuesto devuelve
// true (y descuenta uno); cuando se agotó devuelve false y el drenador CORTA — los bytes que
// sobraron quedan en el buffer de hardware y se procesan el PRÓXIMO tick. Así el tick de RX
// tiene un WCET fijo (max_per_tick bytes × costo por byte) sin importar cuánta basura llegó.
//
// ¿Y si cortamos a mitad de un frame? No pasa nada — y ESE es el punto fino:
// --------------------------------------------------------------------------------------
// Si el presupuesto se agota justo cuando el FrameDecoder tenía medio frame, no se pierde el
// frame: los bytes restantes esperan en el buffer y el decoder RETOMA donde quedó en el tick
// siguiente (es una máquina de estados con memoria, no reinicia). Y si lo que se acumuló era
// BASURA de un cable ruidoso, el FrameDecoder RESINCRONIZA SOLO: descarta hasta reencontrar el
// header válido. O sea: acotar bytes/tick no rompe la recepción de frames legítimos y a la vez
// le quita al ruido el poder de robarse el loop. Throughput acotado, latencia acotada, sin
// dejar de recibir lo bueno.
//
// fail-safe: si max_per_tick está mal seteado (0), rxbb_allow() devuelve false SIEMPRE → el
// tick de RX no procesa nada (degrada a "no recibo", nunca a "monopolizo el loop"). Default-to-
// safe: ante un presupuesto inválido, el lazo de control sigue corriendo; la comunicación se
// resentirá (visible en telemetría), pero el robot no se congela. Es la degradación correcta.
//
// PURO: sin Arduino, sin Serial. POD + funciones libres → .bss zero-init. OJO con el zero-init:
// un RxByteBudget en .bss arranca con max_per_tick=0 (deniega todo) hasta que el caller lo
// inicializa con su techo real (rxbb_init / asignación directa). Eso es a propósito: un
// presupuesto sin configurar NO debe dejar pasar bytes sin control. Compila con g++ host.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// El contador de presupuesto. max_per_tick = techo de bytes por vuelta; used = cuántos se
// gastaron en el tick actual.
//
// Zero-init (.bss) == presupuesto CERRADO (max_per_tick=0 → no deja pasar nada). El caller
// debe fijar max_per_tick a su techo real antes de usarlo (rxbb_init o asignación directa).
// Esto es fail-safe: un presupuesto sin inicializar deniega, nunca abre el grifo sin control.
struct RxByteBudget {
    int max_per_tick;   // techo de bytes a procesar en un tick (lo fija el caller)
    int used;           // bytes ya consumidos en el tick actual (se reinicia con rxbb_reset)
};

// rxbb_init(b, max) — fija el techo y arranca el tick en cero. Conveniencia para inicializar
// un presupuesto recién creado (equivale a {max, 0}). Un max <= 0 deja el presupuesto cerrado
// (rxbb_allow devolverá false siempre) — fail-safe ante un techo inválido.
inline void rxbb_init(RxByteBudget& b, int max_per_tick) {
    b.max_per_tick = max_per_tick;
    b.used         = 0;
}

// rxbb_reset(b) — arranca un tick nuevo: pone el gasto en cero, conserva el techo.
// El drenador llama esto AL EMPEZAR cada vuelta del superloop, antes de leer del buffer.
inline void rxbb_reset(RxByteBudget& b) {
    b.used = 0;
}

// rxbb_allow(b) — ¿puedo procesar UN byte más en este tick?
//
// Devuelve true mientras used < max_per_tick (y DESCUENTA: incrementa used). Cuando used
// alcanzó el techo, devuelve false y NO incrementa (no desborda used más allá del techo).
// El drenador hace: while (hay_byte && rxbb_allow(b)) { procesar_un_byte(); }.
//
// Corte ESTRICTO en max_per_tick: con max=N se permiten exactamente N bytes (used pasa de 0
// a N), y la (N+1)-ésima llamada devuelve false. max_per_tick <= 0 → false de una (presupuesto
// cerrado: el comparativo used < max es 0 < 0 = false; fail-safe ante techo inválido).
inline bool rxbb_allow(RxByteBudget& b) {
    if (b.used < b.max_per_tick) {
        ++b.used;        // gastamos un byte del presupuesto del tick
        return true;
    }
    return false;        // techo alcanzado (o presupuesto cerrado): el drenador corta acá
}

// rxbb_remaining(b) — cuántos bytes quedan de presupuesto en este tick (para telemetría).
// Nunca negativo: si por algún camino used superó el techo, devuelve 0.
inline int rxbb_remaining(const RxByteBudget& b) {
    const int rem = b.max_per_tick - b.used;
    return rem > 0 ? rem : 0;
}

}  // namespace iitasoccer
