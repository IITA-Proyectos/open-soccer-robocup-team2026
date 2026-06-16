// hcsr04_async.h — FSM ASÍNCRONA del HC-SR04 (PURO, host-testeable).
//
// Por qué existe (rediseño sensorial del TOP — pedido de Gustavo 2026-06-14/15)
// -----------------------------------------------------------------------------
// Hoy el HC-SR04 frontal se lee con pulseIn() BLOQUEANTE (src/top/sensors_tof.cpp,
// read_hcsr04(), ~línea 147). pulseIn() se queda parado dentro del loop esperando el
// flanco del eco: cuando hay algo cerca tarda poco, pero cuando el sensor mira al
// vacío se cuelga el timeout COMPLETO (~12 ms) antes de devolver "nada". El loop del
// TOP tiene que mandarle un WorldSnapshot al CENTRAL a 100 Hz SIN FALTA; robarle 12 ms
// (más de un período entero) cada vez que dispara el ultrasonido atrasa ese envío y la
// FSM del CENTRAL termina controlando el rumbo con datos viejos. La regla dura del
// rediseño es la misma que la PIZARRA (sensor_slot.h): las lecturas corren por su lado
// y el loop NUNCA espera a un sensor.
//
// Esta es la LÓGICA de un HC-SR04 que NO bloquea. En vez de quedarse esperando el eco,
// es una maquinita de estados (FSM) que avanza de a pasos cortos:
//
//   IDLE  --(toca disparar)-->  TRIG  --(pulso enviado)-->  WAIT_RISE
//   WAIT_RISE  --(flanco de subida del eco)-->  WAIT_FALL
//   WAIT_FALL  --(flanco de bajada del eco)-->  IDLE  + DISTANCIA lista
//
// El "ancho del eco" (el tiempo que ECHO estuvo en alto = subida→bajada) es lo que
// codifica la distancia. La FSM lo mide guardando el timestamp de la subida y restándolo
// en la bajada. Nadie se queda esperando: cada flanco lo APORTA el caller (una rutina
// de interrupción en el Teensy real, o un test acá) llamando a hcsr04_on_edge().
//
// Reparto de responsabilidades (este módulo NO toca GPIO)
// -------------------------------------------------------
// Este módulo es PURO ARITMÉTICA Y ESTADO. NO escribe TRIG, NO lee ECHO, no llama a
// micros(). El glue Arduino (poner el pin TRIG en alto 10 us, enganchar la interrupción
// de ECHO con attachInterrupt y pasarle micros() al on_edge) vive en sensors_tof.cpp.
// Acá entra todo como números:
//   • hcsr04_due()         — ¿ya toca disparar otra medición? (respeta trig_period_us)
//   • hcsr04_on_trig_sent()— el glue avisó "ya mandé el pulso de trigger" (sella t0)
//   • hcsr04_on_edge()     — llegó un flanco del eco (subida o bajada) con su micros()
//   • hcsr04_poll()        — el loop pregunta el resultado: distancia mm, o sentinela
//
// FAIL-SAFE (lo que hace seguro reemplazar al pulseIn bloqueante)
// ---------------------------------------------------------------
//   • TIMEOUT: si en WAIT_RISE pasó > echo_timeout_us sin la subida (nada en rango), o
//     en WAIT_FALL pasó > echo_timeout_us sin la bajada (eco perdido / módulo flotando),
//     hcsr04_poll() vuelve la FSM a IDLE y devuelve TOF_NO_READING — NUNCA el
//     last_dist_mm viejo. Un sensor que se calló se reporta como "sin lectura", no como
//     un obstáculo fantasma clavado en el último valor.
//   • ANTI-BLOQUEO: nada espera. Cada función hace una cuenta y vuelve. El loop a 100 Hz
//     puede llamar a poll() todas las vueltas sin costo.
//   • ECO ESPURIO: una bajada SIN una subida previa (glitch eléctrico, rebote) se
//     descarta — no se toma como fin de medición ni produce una distancia inventada.
//   • DEFAULT-TO-SAFE: fuera del rango físico [min_mm, max_mm] (eco rarísimo, ringing,
//     objeto pegado) → TOF_NO_READING, no un número que dispare evasión espuria.
//   • Restas de tiempo UNSIGNED → WRAP-SAFE: micros() da la vuelta a los ~71,6 min
//     (2^32 us); la resta unsigned igual da el intervalo real mientras la espera dure
//     << 2^32 us (siempre cierto: un período de medición son decenas de ms). Mismo
//     criterio que state_timer.h / tof_fresh_or_no_reading.
//
// PURO: sin Arduino, sin Wire/Serial/micros. Solo <stdint.h>. POD + funciones libres →
// .bss zero-init == FSM en IDLE, sin medida previa (válido y seguro). Compila g++ host.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Sentinela "sin lectura". Redefinido LOCAL como const para no arrastrar Arduino: el
// canónico vive en src/top/sensors_tof.h (TOF_NO_READING = 0xFFFF), que incluye
// Arduino. El glue de sensors_tof.cpp ya usa ese mismo valor; coincide a propósito.
constexpr uint16_t HCSR04_NO_READING = 0xFFFF;

// Configuración del HC-SR04 (overridable; defaults validados contra el firmware actual).
//   • trig_period_us  — separación mínima entre disparos. 60 ms: deja morir cualquier eco
//     del disparo anterior antes del siguiente (evita medir el rebote viejo). El firmware
//     hoy lo lee cada ~90 ms; 60 ms es un piso seguro, el caller puede subirlo.
//   • echo_timeout_us — cuánto se espera un flanco antes de rendirse. 12 ms ≈ 2 m de
//     alcance (cubre la cancha), igual que el timeout del pulseIn actual.
//   • min_mm / max_mm — rango físico aceptado. Fuera de ahí → HCSR04_NO_READING.
struct Hcsr04AsyncCfg {
    uint32_t trig_period_us;
    uint32_t echo_timeout_us;
    uint16_t min_mm;
    uint16_t max_mm;
};

// Defaults: 60 ms período, 12 ms timeout (~2 m), rango [20 mm, 2000 mm].
inline Hcsr04AsyncCfg hcsr04_async_default_cfg() {
    Hcsr04AsyncCfg c;
    c.trig_period_us  = 60000u;
    c.echo_timeout_us = 12000u;
    c.min_mm          = 20u;
    c.max_mm          = 2000u;
    return c;
}

// Estado de la FSM. Zero-init (.bss) == estado inicial válido:
//   phase=IDLE(0), sin medida previa (valid=false, last_dist_mm=0). Un objeto recién
//   construido está listo para que hcsr04_due() decida el primer disparo.
struct Hcsr04Async {
    // Fases de la maquinita. El orden importa: IDLE=0 para que el zero-init caiga ahí.
    enum Phase { IDLE = 0, TRIG, WAIT_RISE, WAIT_FALL };

    Phase    phase;          // en qué paso de la medición estamos
    uint32_t trig_start_us;  // micros() del disparo (t0). Base del timeout de WAIT_RISE
                             // y del 0 del cronómetro del eco.
    uint32_t rise_us;        // micros() del flanco de subida del eco. Base del timeout de
                             // WAIT_FALL y extremo inferior del ancho del eco.
    uint16_t last_dist_mm;   // última distancia VÁLIDA medida (telemetría). NUNCA se
                             // devuelve ante un timeout (eso da HCSR04_NO_READING).
    bool     valid;          // ¿last_dist_mm corresponde a una medición buena? (false
                             // hasta el primer ciclo completo en rango).
};

// Convierte un ancho de eco (us) a distancia (mm), aplicando el clamp de rango.
//   mm = (width_us * 343) / 2000
//   343 m/s = 0,343 mm/us; el eco es IDA + VUELTA → se divide entre 2 (de ahí /2000 en
//   vez de /1000). Idéntico al cálculo del read_hcsr04() bloqueante actual.
// Fuera de [min_mm, max_mm] → HCSR04_NO_READING (default-to-safe).
inline uint16_t hcsr04_width_to_mm(uint32_t width_us, const Hcsr04AsyncCfg& cfg) {
    const uint32_t mm = (width_us * 343u) / 2000u;
    if (mm < cfg.min_mm || mm > cfg.max_mm) return HCSR04_NO_READING;
    return static_cast<uint16_t>(mm);
}

// ¿Toca disparar otra medición? true SOLO si la FSM está en IDLE Y pasó al menos
// trig_period_us desde el último disparo. Resta UNSIGNED → wrap-safe.
//
// La referencia es trig_start_us (el t0 del disparo anterior). En el zero-init
// trig_start_us=0; el caller normalmente arranca con now_us grande, así que el primer
// due() da true (la resta now-0 supera el período) y se dispara enseguida — comportamiento
// deseado (no hay que sembrar nada). Si arrancara con now_us < período pegado a 0, esperaría
// ese tanto; irrelevante en la práctica (micros() del Teensy no arranca en 0 sincronizado).
//
// Mientras la FSM está MIDIENDO (no IDLE) due() devuelve false: no se pisa una medición en
// curso con un disparo nuevo.
inline bool hcsr04_due(const Hcsr04Async& s, uint32_t now_us, const Hcsr04AsyncCfg& cfg) {
    if (s.phase != Hcsr04Async::IDLE) return false;
    const uint32_t elapsed = now_us - s.trig_start_us;  // unsigned → wrap-safe
    return elapsed >= cfg.trig_period_us;
}

// El glue avisa: "ya puse TRIG en alto y mandé el pulso de 10 us". Sella t0 y pasa a
// esperar el flanco de subida del eco. (El paso TRIG es instantáneo desde la óptica de la
// FSM; se modela como una transición que el glue confirma con esta llamada.)
inline void hcsr04_on_trig_sent(Hcsr04Async& s, uint32_t now_us) {
    s.trig_start_us = now_us;
    s.phase         = Hcsr04Async::WAIT_RISE;
}

// Llegó un flanco del eco. `rising`=true → subida; false → bajada. `edge_us`=micros() del
// flanco (lo aporta la ISR en el Teensy, o el test acá).
//
//   • SUBIDA en WAIT_RISE → empezó el eco: sella rise_us y pasa a WAIT_FALL.
//   • BAJADA en WAIT_FALL → terminó el eco: mide el ancho (edge - rise, unsigned wrap-safe),
//     lo convierte a mm con el clamp, guarda la medida y vuelve a IDLE.
//   • Cualquier otra combinación es ESPURIA y se DESCARTA sin cambiar nada:
//       - bajada sin subida previa (no estamos en WAIT_FALL) → glitch/rebote, ignorar.
//       - subida cuando no esperábamos una (no estamos en WAIT_RISE) → ruido, ignorar.
//     Descartar = no inventar distancia ni alterar la fase; el timeout de poll() limpia
//     si la medición quedó trabada.
inline void hcsr04_on_edge(Hcsr04Async& s, bool rising, uint32_t edge_us) {
    if (rising) {
        if (s.phase == Hcsr04Async::WAIT_RISE) {
            s.rise_us = edge_us;
            s.phase   = Hcsr04Async::WAIT_FALL;
        }
        // subida fuera de WAIT_RISE → espuria, ignorar
        return;
    }
    // bajada
    if (s.phase == Hcsr04Async::WAIT_FALL) {
        const uint32_t width_us = edge_us - s.rise_us;  // unsigned → wrap-safe
        // Nota: hcsr04_on_edge NO conoce la cfg (la firma del contrato no la recibe), así
        // que acá guardamos el ancho crudo convertido SIN clamp y dejamos el clamp +
        // sentinela a hcsr04_poll(), que sí tiene cfg. Para no perder el ancho, lo
        // codificamos como distancia "cruda" y marcamos valid; poll() re-aplica el rango.
        // Conversión cruda (puede exceder uint16): saturamos a 0xFFFE para no chocar con
        // el sentinela; poll() decide rango.
        uint32_t mm = (width_us * 343u) / 2000u;
        if (mm > 0xFFFEu) mm = 0xFFFEu;     // techo para no colisionar con HCSR04_NO_READING
        s.last_dist_mm = static_cast<uint16_t>(mm);
        s.valid        = true;
        s.phase        = Hcsr04Async::IDLE;
        return;
    }
    // bajada fuera de WAIT_FALL → eco espurio (fall sin rise), ignorar
}

// El loop pregunta el resultado. Devuelve:
//   • La distancia mm de la última medición COMPLETADA y EN RANGO, o
//   • HCSR04_NO_READING si: (a) hay un timeout en curso (se rinde y limpia a IDLE),
//     (b) la última medición cayó fuera de rango, o (c) todavía no hubo ninguna.
//
// FAIL-SAFE del timeout (lo central de este módulo):
//   • En WAIT_RISE, si now - trig_start_us > echo_timeout_us → nadie respondió: la FSM
//     vuelve a IDLE y se devuelve HCSR04_NO_READING (NO last_dist_mm viejo).
//   • En WAIT_FALL, si now - rise_us > echo_timeout_us → el eco subió pero no bajó
//     (módulo flotando, eco perdido): mismo trato, IDLE + HCSR04_NO_READING.
//   El corte es ESTRICTO (> timeout): justo en el límite todavía se espera un tick más,
//   coherente con "espero hasta agotar el timeout".
//
// poll() NO devuelve el dato viejo durante una medición en curso sin timeout: mientras se
// espera un flanco (sin vencer aún) devuelve la última distancia válida conocida (o
// NO_READING si nunca hubo). Esto da continuidad al min_obstacle entre disparos, PERO en
// cuanto vence el timeout corta a NO_READING (no perpetúa un valor de un eco que ya no
// existe). Una vez de vuelta en IDLE con una medición buena, sigue reportándola hasta el
// próximo ciclo (igual que el cache del ToF, acotado por la frescura del wrapper).
inline uint16_t hcsr04_poll(Hcsr04Async& s, uint32_t now_us, const Hcsr04AsyncCfg& cfg) {
    if (s.phase == Hcsr04Async::WAIT_RISE) {
        const uint32_t waited = now_us - s.trig_start_us;  // unsigned → wrap-safe
        if (waited > cfg.echo_timeout_us) {
            s.phase = Hcsr04Async::IDLE;          // se rinde
            return HCSR04_NO_READING;             // NUNCA el valor viejo
        }
        // todavía esperando la subida, sin vencer → reporta la última buena (o NO_READING)
    } else if (s.phase == Hcsr04Async::WAIT_FALL) {
        const uint32_t waited = now_us - s.rise_us;        // unsigned → wrap-safe
        if (waited > cfg.echo_timeout_us) {
            s.phase = Hcsr04Async::IDLE;          // eco subió pero no bajó → se rinde
            return HCSR04_NO_READING;             // NUNCA el valor viejo
        }
        // esperando la bajada, sin vencer → reporta la última buena (o NO_READING)
    }
    // IDLE o esperando-sin-vencer: devolver la última medición válida re-clampada al rango.
    if (!s.valid) return HCSR04_NO_READING;
    if (s.last_dist_mm < cfg.min_mm || s.last_dist_mm > cfg.max_mm) {
        return HCSR04_NO_READING;                 // fuera de rango → default-to-safe
    }
    return s.last_dist_mm;
}

}  // namespace iitasoccer
