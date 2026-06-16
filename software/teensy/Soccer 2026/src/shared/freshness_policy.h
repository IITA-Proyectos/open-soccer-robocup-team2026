// freshness_policy.h — La POLÍTICA de frescura→sentinela, en un solo lugar (PURO, host-testeable).
//
// Por qué existe (generalizar el idiom que ya repetimos por todos lados)
// ----------------------------------------------------------------------------------
// El robot tiene MUCHOS casilleros de sensor (ToF, BNO, cámaras, OTOS, línea) y todos
// se hacen la MISMA pregunta antes de usar un dato: "¿este dato es de AHORA, o el sensor
// se calló y quedó viejo?". Hoy esa decisión está copy-pasteada con micro-variantes:
// tof_fresh_or_no_reading() decide para un ToF, sensor_slot.h::slot_is_fresh() para la
// pizarra, y cada consumidor re-implementa el "si venció, colapsá a sentinela". Copiar
// la regla N veces es la receta para que UNA copia tenga el borde mal (>= en vez de >,
// o una resta con signo que el wrap de millis() rompe) y nadie lo note hasta la cancha.
//
// Este módulo destila esa regla a UNA función: slot_eval() mira (¿alguna vez hubo dato?,
// cuándo fue, qué hora es, cuánto aguanto) y devuelve un veredicto de tres estados
// {FRESCO, VIEJO, NUNCA}. Todo lo demás (gatear una confianza, saturar una edad para
// telemetría) se construye encima de ese veredicto, así el fail-safe vive en UN lugar.
//
// FAIL-SAFE: el default es DESCONFIAR
// ----------------------------------------------------------------------------------
//   • NUNCA tuvo dato (ever_ok=false) → NEVER, edad = UINT32_MAX (la más vieja posible).
//     No miramos last_ok_ms siquiera: sin un "alguna vez OK", el timestamp es basura
//     (un .bss en cero diría "publicado en t=0", un dato fantasma). El estado NUNCA es
//     distinto de VIEJO a propósito: "se cayó" y "nunca arrancó" se diagnostican distinto.
//   • El TECHO de frescura le gana a la confianza: conf_gate() devuelve 0 ante VIEJO o
//     NUNCA, sin importar qué tan alta venga la confianza del detector. Una cámara que
//     "está 99% segura" pero cuyo último frame es de hace 2 s NO vale 99 — vale 0. La
//     frescura es una condición necesaria; la confianza, una vez fresco, refina.
//   • Borde elapsed == timeout → todavía FRESCO (el corte es > timeout, NO >=), idéntico
//     a tof_fresh_or_no_reading: el instante límite todavía cuenta como vivo; recién el
//     ms siguiente vence. Un solo lugar para este borde = un solo lugar donde puede
//     estar mal, y está testeado.
//
// WRAP-SAFE (el reloj de millis() da la vuelta cada ~49,7 días, y a veces "retrocede")
// ----------------------------------------------------------------------------------
// La edad se calcula con resta UNSIGNED (now - last_ok): si now < last_ok (wrap del
// contador, o un reloj que pegó un saltito atrás), la resta unsigned NO hace underflow
// a un número gigante — da el elapsed correcto módulo 2^32. Mismo patrón que
// sensor_slot.h / tof_fresh_or_no_reading / state_timer.h. CASO especial defensivo: si
// el caller nos pasa now < last_ok (reloj raro), tratamos la edad como 0 → FRESCO, en
// vez de dejar que la aritmética de wrap reporte ~49 días de viejo por un saltito de 1 ms.
//
// PURO: sin Arduino, sin millis(), sin Serial. El tiempo entra como uint32 de
// millis()/micros() por parámetro (el caller elige la fuente: millis() en el Teensy, un
// contador falso en los tests). Compila con g++ host. Zero-init == "nunca hubo dato".

#pragma once
#include <stdint.h>

namespace iitasoccer {

// El veredicto de frescura, en tres estados (distinguir "se cayó" de "nunca arrancó"
// importa para el diagnóstico y la telemetría — no son lo mismo).
enum class SlotState : uint8_t {
    FRESH = 0,  // hubo dato OK y es reciente (elapsed <= timeout) → confiable.
    STALE = 1,  // hubo dato OK alguna vez, pero venció (elapsed > timeout) → no confiar.
    NEVER = 2,  // NUNCA hubo un dato OK → no hay nada de qué medir frescura.
};

// El resultado de evaluar un casillero: su edad (ms desde el último OK) y su estado.
// age_ms es para telemetría/decisiones finas; state es el veredicto fail-safe.
struct SlotFreshness {
    uint32_t age_ms;    // ms desde el último OK. UINT32_MAX si NEVER.
    SlotState state;
};

// slot_eval — el corazón: dado el estado de un casillero, devuelve {edad, veredicto}.
//
//   ever_ok     = ¿hubo ALGUNA vez una lectura/publicación buena? (sin esto no hay base)
//   last_ok_ms  = millis() de ese último OK (sólo significativo si ever_ok)
//   now_ms      = millis() actual
//   timeout_ms  = cuánto aguanta un dato antes de considerarse viejo
//
// Reglas (fail-safe, en orden):
//   1) !ever_ok → NEVER, age = UINT32_MAX. No miramos last_ok_ms (sería basura).
//   2) now < last_ok (reloj raro / saltito atrás) → age 0, FRESH (sin underflow espurio).
//   3) elapsed = now - last_ok (unsigned, wrap-safe). elapsed <= timeout → FRESH;
//      elapsed > timeout → STALE. (Borde == timeout = FRESH, igual que tof_fresh.)
inline SlotFreshness slot_eval(bool ever_ok,
                               uint32_t last_ok_ms,
                               uint32_t now_ms,
                               uint32_t timeout_ms) {
    SlotFreshness r;
    if (!ever_ok) {
        // Nunca hubo dato bueno: el timestamp es basura, no lo usamos. La edad "más
        // vieja posible" deja claro a cualquier consumidor que esto no se debe usar.
        r.age_ms = UINT32_MAX;
        r.state = SlotState::NEVER;
        return r;
    }
    // Reloj raro: now por detrás de last_ok (wrap del contador, o un saltito atrás).
    // Tratamos como recién publicado (edad 0) en vez de dejar que el wrap reporte ~49
    // días. Defensivo: el dato es como mucho de "ahora", nunca más viejo que eso.
    if (now_ms < last_ok_ms) {
        r.age_ms = 0u;
        r.state = SlotState::FRESH;
        return r;
    }
    const uint32_t elapsed = now_ms - last_ok_ms;   // unsigned → wrap-safe
    r.age_ms = elapsed;
    // Borde: elapsed == timeout TODAVÍA es fresco (corte es > timeout, no >=). Idéntico
    // a tof_fresh_or_no_reading — un solo lugar para este borde.
    r.state = (elapsed > timeout_ms) ? SlotState::STALE : SlotState::FRESH;
    return r;
}

// fresh_ok — atajo legible para "¿puedo confiar en este dato?". Sólo FRESH cuenta.
inline bool fresh_ok(const SlotFreshness& f) {
    return f.state == SlotState::FRESH;
}

// conf_gate — el TECHO de frescura le gana a la confianza del detector.
//
// Si el dato no está fresco (STALE o NEVER), la confianza colapsa a 0 sin importar qué
// tan alta venga base_conf: un detector "99% seguro" de un frame de hace 2 s vale 0.
// Si está FRESCO, la confianza pasa tal cual (la frescura es necesaria; la confianza,
// una vez fresca, refina). NO inventa confianza: nunca sube base_conf, sólo la deja
// pasar o la anula.
inline uint8_t conf_gate(uint8_t base_conf, const SlotFreshness& f) {
    return fresh_ok(f) ? base_conf : (uint8_t)0;
}

// age_to_u8_ms — empaqueta una edad (ms) en un byte para telemetría, SATURANDO a 255.
//
// La telemetría manda la edad en 1 byte (barato). Cualquier edad >= 255 ms ya es
// "vencidísimo" para nuestros timeouts (decenas a un par de cientos de ms), así que
// saturar en 255 no pierde información útil — y evita el wrap a un valor chico (un dato
// de hace 256 ms NO debe verse como "1 ms de fresco" en el monitor). Satura, no envuelve.
inline uint8_t age_to_u8_ms(uint32_t age_ms) {
    return (age_ms >= 255u) ? (uint8_t)255 : (uint8_t)age_ms;
}

}  // namespace iitasoccer
