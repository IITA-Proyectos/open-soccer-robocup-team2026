// sensor_slot.h — La PIZARRA: un casillero genérico productor/consumidor (PURO, host-testeable).
//
// Por qué existe (rediseño sensorial de la placa TOP — pedido de Gustavo 2026-06-14)
// ----------------------------------------------------------------------------------
// La placa TOP tiene que mandarle un WorldSnapshot al CENTRAL a 100 Hz SIN FALTA. Hoy
// el loop es un superloop cooperativo: si la lectura de un sensor se demora (un read
// I2C del BNO, un ToF que tarda, el pulseIn del HC-SR04), el envío del snapshot se
// atrasa con ella. El plan acordado por el equipo es que las lecturas pasen a correr
// por su lado (interrupciones + DMA), dejen el dato fresco en algún lado, y el loop
// del snapshot SOLO lea ese "algún lado" — nunca espere al sensor.
//
// Ese "algún lado" es esta PIZARRA. Imaginá una pizarra de turnos en una guardia: el
// que tiene el dato nuevo (el PRODUCTOR — la rutina de lectura del sensor) la escribe;
// el que necesita el último dato (el CONSUMIDOR — el armado del snapshot) la lee de un
// vistazo y sigue. El que lee NUNCA espera a que el que escribe termine, y el que
// escribe NUNCA espera al que lee. Cada uno hace lo suyo a su ritmo.
//
// El problema fino: TORN READ (lectura partida)
// ----------------------------------------------------------------------------------
// Si el productor está a mitad de escribir un dato grande (p.ej. una struct con x, y,
// distancia) y JUSTO ahí el consumidor lee, el consumidor podría agarrar la mitad
// vieja y la mitad nueva — un Frankenstein que nunca existió. En un superloop eso no
// pasa porque todo corre secuencial, pero en cuanto el productor es una INTERRUPCIÓN
// (que preempta al loop en cualquier instante) o un DMA, sí puede pasar.
//
// La solución: DOBLE BUFFER + SEQLOCK (sin locks, sin deshabilitar interrupciones)
// ----------------------------------------------------------------------------------
// Tenemos DOS casilleros (buffer[0] y buffer[1]) y un contador de secuencia `seq`:
//   • El PRODUCTOR escribe SIEMPRE en el buffer que el consumidor NO está mirando, y
//     recién cuando terminó de escribirlo entero, incrementa `seq` para "publicar".
//     El bit más bajo de `seq` dice cuál buffer es el bueno → al publicar, el lector
//     pasa a mirar el recién escrito, y el productor pasa a usar el otro la próxima.
//   • El CONSUMIDOR lee `seq`, copia el buffer que `seq` indica, vuelve a leer `seq`:
//     si cambió en el medio, el productor publicó justo ahí → la copia puede estar
//     partida → reintenta. Si `seq` quedó igual, la copia es de UNA sola publicación,
//     coherente. (Esto es un "seqlock" de libro, en su forma de un solo productor.)
//
// Resultado: read_latest() SIEMPRE devuelve un valor que existió de verdad (entero,
// nunca partido), y si nadie escribió todavía devuelve el valor inicial. Ni el lector
// ni el escritor se bloquean jamás → el snapshot a CENTRAL nunca se frena por un sensor.
//
// FRESCURA (¿el dato es de ahora o quedó viejo?)
// ----------------------------------------------------------------------------------
// La pizarra también sella CUÁNDO se publicó (timestamp del productor, en ms de
// millis()). El consumidor pregunta is_fresh(now, timeout): si pasó más del timeout
// desde la última publicación, el sensor se calló (se colgó, se desenchufó) y el
// CENTRAL no debe confiar en ese dato. Mismo criterio wrap-safe que state_timer.h /
// tof_fresh_or_no_reading: resta UNSIGNED, así el wrap de millis() (~49,7 días) no
// dispara un "viejo" espurio.
//
// PURO: sin Arduino, sin RTOS, sin threads. Plantilla + POD. El tiempo entra como
// uint32 de millis() (el caller elige la fuente: millis() en el Teensy, un contador
// falso en los tests). Compila con g++ host. Zero-init (.bss) == estado válido vacío.
//
// LÍMITE HONESTO (qué garantiza y qué NO):
//   • Diseñado para UN productor y UNO o varios consumidores (patrón de esta pizarra:
//     una rutina de lectura por sensor publica; el armado del snapshot lee). DOS
//     productores sobre el mismo slot NO está soportado (se pisarían el buffer).
//   • En el Teensy real, `seq` debería ser `volatile` y las publicaciones/lecturas
//     ordenarse con una barrera de memoria para que el compilador/CPU no reordene los
//     stores. Acá el tipo de `seq` se deja como plain uint32 para que el módulo sea
//     PURO y host-testeable; el glue Arduino (marcar volatile, barreras, correr el
//     publish dentro de la ISR) se agrega en la integración de banco, NO acá. El test
//     SIMULA la concurrencia intercalando publish/read a mano (no hay hilos de verdad
//     host), que es lo que se puede verificar de la LÓGICA sin hardware.

#pragma once
#include <stdint.h>

// ----------------------------------------------------------------------------
// BARRERA DE MEMORIA DEL SEQLOCK (IITA_SEQLOCK_FENCE)
// ----------------------------------------------------------------------------
// El seqlock es correcto por SINGLE-WRITER + lector wait-free, PERO en el
// Cortex-M7 (Teensy 4.x) el store buffer / la ejecución fuera de orden pueden
// hacer visible el store de `seq` (publicar) ANTES de que el store del buffer
// termine → torn read. Hace falta una barrera de memoria entre el escribir el
// buffer y el publicar `seq` (y, en el lector, entre leer `seq` y copiar el
// buffer). En el M7 la barrera correcta es `dmb` con clobber `:::"memory"` (el
// equivalente a CMSIS __DMB(), full-system 0xF) — NO el `dsb` pelado del
// watchdog: el clobber también prohíbe que el compilador reordene/elimine los
// accesos. En host (x86, tests single-thread) basta una barrera de compilador.
// Pedido explícito del review adversarial del repo (HANDOFF-INTEGRACION-RT.md).
#if defined(__arm__) || defined(__thumb__) || defined(__ARM_ARCH)
#define IITA_SEQLOCK_FENCE() __asm__ __volatile__("dmb 0xF" ::: "memory")
#else
#define IITA_SEQLOCK_FENCE() __asm__ __volatile__("" ::: "memory")
#endif

namespace iitasoccer {

// SensorSlot<T> — la pizarra para un dato de tipo T.
//
// T debe ser un POD copiable trivialmente (una struct de números, p.ej. una distancia,
// una pose, un avistaje de pelota). El slot guarda DOS copias de T (doble buffer) + el
// contador de secuencia + el timestamp de la última publicación.
//
// Zero-init (.bss) == pizarra vacía y coherente:
//   seq=0  → bit bajo 0 → el lector mira buffer[0]; nunca se publicó (published=false)
//   → read_latest() devuelve buffer[0] (que es T value-inicializado) y is_fresh()
//   da false (nunca hubo publicación de la cual medir frescura).
template <typename T>
struct SensorSlot {
    // Los dos casilleros físicos. El productor escribe en uno mientras el consumidor
    // puede estar leyendo el otro. value-init (= ceros para un POD) al construir.
    T buffer[2]{};

    // Contador de secuencia (el corazón del seqlock).
    //   • PAR  → no hay una escritura a medio hacer; el dato está quieto y publicado.
    //   • IMPAR→ el productor está EN MEDIO de una publicación (escribiendo); el lector
    //            que vea impar debe reintentar.
    //   • El valor / 2 (cuántas veces se publicó) no importa; lo que importa es si
    //     CAMBIÓ entre el antes y el después de la copia del lector. El bit 1 (seq>>1
    //     &1) selecciona cuál buffer es el recién publicado.
    // VOLATILE: el lector (la ISR del emisor) y el escritor (el loop) tocan `seq`
    // desde contextos distintos; sin volatile el compilador puede cachear `seq` en
    // un registro y plegar `before == after` a siempre-true (matando la detección
    // de torn-read). volatile fuerza las relecturas reales; las barreras
    // IITA_SEQLOCK_FENCE() ordenan los accesos al buffer respecto de `seq`.
    volatile uint32_t seq{0};

    // millis() de la última publicación exitosa. Base de la frescura.
    uint32_t last_publish_ms{0};

    // ¿Hubo al menos una publicación? Antes de la primera, read_latest() devuelve el
    // valor inicial y is_fresh() es false (no hay de qué medir frescura).
    bool published{false};
};

// publish(slot, value, now) — el PRODUCTOR deja un dato nuevo en la pizarra.
//
// Secuencia seqlock de un solo productor:
//   1) seq pasa a IMPAR (++seq): avisa "estoy escribiendo, no me leas entero todavía".
//   2) escribe el value en el buffer que NO se estaba publicando (el "de atrás").
//   3) seq pasa a PAR otra vez (++seq): publica. Ahora el bit de selección apunta al
//      buffer recién escrito, y un lector que copie después de esto verá el dato nuevo.
// Sella el timestamp y marca published=true.
//
// (En el Teensy, entre el paso 2 y el ++seq final iría una barrera de memoria para
// asegurar que el store del buffer "se vea" antes que el del seq. Eso es glue de
// integración; la LÓGICA de alternancia y publicación es la que se testea acá.)
template <typename T>
inline void slot_publish(SensorSlot<T>& s, const T& value, uint32_t now) {
    const uint32_t start = s.seq;
    s.seq = start + 1;                 // → IMPAR: "escritura en curso"
    IITA_SEQLOCK_FENCE();              // el store IMPAR se ve ANTES de tocar el buffer
    // Buffer a escribir = el OPUESTO al que quedará seleccionado tras publicar.
    // Tras ++seq final, seq = start+2; el buffer seleccionado por el lector será
    // ((start+2) >> 1) & 1. Para que ESE sea el que escribimos ahora, escribimos en
    // ((start+2) >> 1) & 1.
    const uint32_t write_idx = ((start + 2u) >> 1) & 1u;
    s.buffer[write_idx] = value;       // copia entera del dato nuevo
    IITA_SEQLOCK_FENCE();              // el buffer ENTERO se ve ANTES del store PAR (publicar)
    s.seq = start + 2;                 // → PAR de nuevo: publicado
    s.last_publish_ms = now;
    s.published = true;
}

// read_latest(slot) — el CONSUMIDOR se lleva el último dato publicado, ENTERO.
//
// Bucle seqlock del lector: leé seq, copiá el buffer que indica, releé seq. Si seq no
// cambió y era PAR, la copia es de una sola publicación → coherente, la devolvés. Si
// cambió (o estaba impar), el productor publicó justo en el medio → reintentá.
//
// Sin escritor activo (caso normal del consumidor en el loop), entra una vez y sale.
// Antes de la primera publicación, devuelve buffer[0] (el valor inicial de T).
//
// LÍMITE host-test: con un solo hilo real no hay reintento de verdad (el productor no
// corre "a la vez"); el bucle termina en la primera pasada. El test cubre la lógica de
// ALTERNANCIA de buffers y el caso de seq impar simulando el estado intermedio a mano.
template <typename T>
inline T slot_read_latest(const SensorSlot<T>& s) {
    for (;;) {
        const uint32_t before = s.seq;
        // Si está impar, hay una escritura en curso: el buffer seleccionado todavía no
        // es coherente con `seq`. Reintentar (en hardware, el productor terminará).
        if (before & 1u) {
            continue;
        }
        IITA_SEQLOCK_FENCE();           // leer `before` ANTES de copiar el buffer
        const uint32_t read_idx = (before >> 1) & 1u;
        T out = s.buffer[read_idx];     // copia entera del dato
        IITA_SEQLOCK_FENCE();           // copiar el buffer ANTES de releer `seq`
        const uint32_t after = s.seq;
        if (before == after) {
            return out;                 // nadie publicó en el medio → coherente
        }
        // seq cambió → el productor publicó mientras copiábamos → reintentar.
    }
}

// slot_read_latest_capped(slot, out, max_retries) — read ACOTADO (con tope de reintentos).
//
// Por qué existe (fail-safe del consumidor en tiempo real)
// ----------------------------------------------------------------------------------
// slot_read_latest() de arriba gira en un for(;;) hasta lograr una copia coherente. Eso
// es correcto cuando el productor es sano: una publicación dura un puñado de instrucciones,
// así que el lector reintenta como mucho una vez y sale. PERO el consumidor real corre en
// la ISR del envío del snapshot @100 Hz, y esa ISR NO se puede dar el lujo de girar para
// SIEMPRE. Si un productor se colgó EN MEDIO de una publicación (dejó `seq` IMPAR y nunca
// lo cerró — un cuelgue del DMA, una ISR que abortó, un torn write que no terminó), el
// for(;;) de slot_read_latest() nunca sale → la ISR del snapshot se cuelga con él → el
// CENTRAL deja de recibir el WorldSnapshot → el robot se queda ciego en pleno partido.
// Eso viola la regla dura: el envío del snapshot NUNCA se frena por un sensor.
//
// Esta variante ACOTA los reintentos. Da hasta `max_retries` intentos de copiar coherente;
// si tras ellos `seq` sigue impar (productor a medio publicar) o sigue cambiando bajo
// nuestros pies (productor publicando en ráfaga más rápido que lo que copiamos), se RINDE:
//   • devuelve false  → "el slot no está disponible en este tick",
//   • NO toca `out`   → el caller conserva lo que ya tenía ahí (su sentinela / valor de
//     fallback), nunca un dato a medio escribir. DEFAULT-TO-SAFE: ante la duda, el caller
//     trata el slot como NO fresco y manda el sentinela, igual que si slot_is_fresh() diera
//     false. Nunca propaga un Frankenstein.
// Si en cambio logra una copia coherente (seq par e igual antes/después), la deja en `out`
// y devuelve true.
//
// Contrato de los intentos (para que el conteo sea predecible y nunca-infinito):
//   • Cada vuelta = una lectura de `seq` + (si es par) una copia + relectura de `seq`.
//   • max_retries = cuántas vueltas FALLIDAS se toleran ANTES de rendirse. Hay como mucho
//     (max_retries + 1) vueltas: el "+1" es el primer intento; cada reintento consume uno
//     del tope. max_retries = 0 → UN solo intento, sin reintentos (apto para la ISR más
//     estricta): si esa única pasada no es coherente, false de una.
//   • El for está acotado por un contador → no hay camino que gire infinito, pase lo que
//     pase con el productor. Ese es el punto entero de esta función.
//
// back-compat: slot_read_latest() (el for(;;)) queda intacto para los callers que SÍ pueden
// esperar (un loop cooperativo fuera de ISR). Esta es la versión para el camino de tiempo
// real / fail-safe.
template <typename T>
inline bool slot_read_latest_capped(const SensorSlot<T>& s, T& out,
                                    uint32_t max_retries = 4u) {
    // attempts = 1 (primer intento) + max_retries (reintentos tolerados). Acotado SIEMPRE.
    // (max_retries+1 no puede desbordar en la práctica: max_retries es un conteo chico de
    //  reintentos, no un millis(); aun así, si alguien pasara 0xFFFFFFFF, el lazo termina
    //  por el contador igual — nunca es infinito.)
    for (uint32_t attempt = 0u; attempt <= max_retries; ++attempt) {
        const uint32_t before = s.seq;
        // seq IMPAR → el productor está EN MEDIO de una publicación: el buffer que `seq`
        // indica todavía no es coherente. No copiamos; gastamos un intento y reintentamos
        // (en hardware sano, el productor cierra enseguida; si está colgado, se acaba el tope).
        if (before & 1u) {
            continue;
        }
        IITA_SEQLOCK_FENCE();                // leer `before` ANTES de copiar el buffer
        const uint32_t read_idx = (before >> 1) & 1u;
        T tmp = s.buffer[read_idx];          // copia entera a un temporal (NO a `out` aún)
        IITA_SEQLOCK_FENCE();                // copiar el buffer ANTES de releer `seq`
        const uint32_t after = s.seq;
        if (before == after) {
            out = tmp;                       // coherente → recién ahora tocamos `out`
            return true;
        }
        // seq cambió mientras copiábamos → el productor publicó en el medio → reintentar.
    }
    // Se agotó el tope sin una copia coherente. NO tocamos `out`. El caller trata el slot
    // como no-fresco → sentinela. Never-ok forzado: jamás devolvemos un dato partido.
    return false;
}

// slot_is_fresh(slot, now, timeout_ms) — ¿el dato de la pizarra es de AHORA?
//
// true SOLO si hubo al menos una publicación Y pasó MENOS que timeout_ms desde ella.
// Resta UNSIGNED → wrap-safe (igual que state_timer.h / tof_fresh_or_no_reading).
//
// Detalles:
//   • Nunca se publicó (published=false) → false (no hay de qué medir frescura).
//   • elapsed == timeout exacto → todavía FRESCO (el corte es > timeout, no >=), mismo
//     criterio de borde que tof_fresh_or_no_reading: el límite todavía cuenta como vivo.
//   • timeout_ms == 0 sobre un slot publicado → expira apenas el reloj avanza 1 ms
//     (elapsed 0 sigue fresco; cualquier elapsed > 0 ya venció).
template <typename T>
inline bool slot_is_fresh(const SensorSlot<T>& s, uint32_t now, uint32_t timeout_ms) {
    if (!s.published) return false;
    const uint32_t elapsed = now - s.last_publish_ms;   // unsigned → wrap-safe
    return elapsed <= timeout_ms;
}

// slot_age_ms(slot, now) — cuánto hace (ms) que se publicó por última vez.
// Útil para telemetría/diagnóstico. Sin publicación previa devuelve 0 (no hay edad).
// Resta UNSIGNED → wrap-safe.
template <typename T>
inline uint32_t slot_age_ms(const SensorSlot<T>& s, uint32_t now) {
    if (!s.published) return 0u;
    return now - s.last_publish_ms;
}

}  // namespace iitasoccer
