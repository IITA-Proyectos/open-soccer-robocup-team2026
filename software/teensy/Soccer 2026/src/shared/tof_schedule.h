// tof_schedule.h — TURNERO round-robin de los 4 ToF, saltando el caído (PURO, host-testeable).
//
// Por qué existe (rediseño sensorial de la placa TOP — banco 2026-06-10)
// ----------------------------------------------------------------------------------
// El TOP lee los 4 sensores ToF (VL53L7CX) de a UNO por tick, rotando — NO los 4
// juntos. La razón está medida en banco (sensors_tof.cpp:363): cada getRangingData()
// del VL53L7CX trae un bloque grande por I2C (decenas de ms); leer los 4 en la misma
// pasada hundía el loop del TOP a ~6 Hz y el WorldSnapshot llegaba atrasado a CENTRAL.
// Con UN ToF por tick (round-robin) cada sensor se refresca cada ~120 ms y el loop
// queda holgado. Hoy ese round-robin vive como un `static uint8_t s_rr` dentro de
// sensors_tof_tick() con `s_rr = (s_rr + 1) % NUM_TOF` — lógica enterrada, sin test.
//
// Este módulo SACA esa lógica de turnos a un lugar PURO y testeable, y le suma el
// FAIL-SAFE que el código actual NO tiene: si un ToF se cayó (no responde, se
// desenchufó), el turnero lo SALTEA y sigue rotando entre los vivos. Hoy el código
// vivo igual le "da el turno" a un ToF caído y desperdicia ese tick (el getRangingData
// falla y no pasa nada); este turnero no malgasta turnos en los muertos.
//
// Analogía: una guardia con 4 puestos. El turnero dice "ahora le toca al puesto N".
// Si el puesto N está vacío (sensor caído), pasa al siguiente ocupado — no se planta
// en la silla vacía esperando. Con los 4 ocupados, reparte 0,1,2,3,0,1,... — exacto
// igual que el round-robin de hoy (byte-equivalente).
//
// Qué da y qué NO da (límite honesto)
// ----------------------------------------------------------------------------------
//   • DA: el ÍNDICE del próximo ToF al que conviene leerle el dato (turno).
//   • NO da: el dato del sensor ni decide si está "fresco". La FRESCURA del DATO la
//     maneja tof_fresh_or_no_reading() (expira a TOF_NO_READING tras el timeout). Acá
//     `ready[i]` es "¿el sensor i está vivo / inicializado?" (espejo de g_ready[] del
//     driver), NO "¿su última lectura es reciente?". Separación a propósito: el turno
//     es de planificación; la frescura es del consumidor del dato.
//   • 4 caídos → 0xFF (NINGUNO), SIN loop infinito (degradación total declarada, no
//     un cuelgue). El caller que reciba 0xFF simplemente no lee ToF ese tick.
//   • Degradación parcial: con 3 vivos el turnero cubre los 3 (no se queda pegado al
//     caído) → el robot sigue percibiendo obstáculos con 3 sensores.
//
// PURO: sin Arduino, sin millis() propio. El tiempo entra como uint32 de millis() por
// parámetro (sólo para sellar last_attempt_ms, telemetría/diagnóstico y el modo
// priorizado opcional). Restas de tiempo UNSIGNED → wrap-safe (~49,7 días), mismo
// criterio que sensor_slot.h / state_timer.h / tof_fresh_or_no_reading. Zero-init
// (.bss) == estado válido: rr=0, nada intentado, nadie ready (arranca dando 0xFF
// hasta que el driver marque algún ToF ready — fail-safe por defecto).

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Cantidad de ToF que reparte el turnero. Espejo de NUM_TOF=4 (pinout_common.h).
// Se deja local para que el header sea PURO (no arrastra config del TOP). Si algún
// día NUM_TOF cambia, este valor debe seguirlo (hoy ambos están atados a 4 por
// static_assert en pinout_common.h).
constexpr uint8_t TOF_SCHED_N = 4;

// Sentinela "ningún ToF disponible este turno" (los 4 caídos). 0xFF nunca es un
// índice válido (los índices van 0..TOF_SCHED_N-1). El caller lo trata como "no leer
// ToF ahora", igual que tof_fresh_or_no_reading trata TOF_NO_READING.
constexpr uint8_t TOF_SCHED_NONE = 0xFF;

// Estado del turnero. POD, zero-init == arranque válido.
//   • rr: el cursor del round-robin. Apunta al PRÓXIMO candidato a evaluar. En el
//     caso de los 4 vivos, los índices que va devolviendo son rr, rr+1, ... (0,1,2,3,
//     0,...) — exacto igual que el `s_rr` del driver de hoy.
//   • last_attempt_ms[i]: millis() del último turno que se le DIO al sensor i (sello
//     que pone note_attempt). Para telemetría y para el modo priorizado opcional.
//   • ready[i]: ¿el sensor i está vivo (inicializado/responde)? false = caído → se
//     saltea. Zero-init = todos false = nadie listo (devuelve 0xFF hasta que el
//     driver marque alguno ready — defecto seguro: no inventar lecturas).
struct TofSchedState {
    uint8_t  rr{0};
    uint32_t last_attempt_ms[TOF_SCHED_N]{};
    bool     ready[TOF_SCHED_N]{};
};

// tof_sched_set_ready(st, i, r) — marca el sensor i como vivo (r=true) o caído (false).
// Lo llama el driver cuando un ToF se inicializa OK o cuando se detecta caído. Índice
// fuera de rango = no-op (defensivo: nunca escribe fuera del array).
inline void tof_sched_set_ready(TofSchedState& st, uint8_t i, bool r) {
    if (i >= TOF_SCHED_N) return;
    st.ready[i] = r;
}

// tof_sched_note_attempt(st, i, now_ms) — sella que al sensor i se le dio el turno
// AHORA. El driver lo llama cuando efectivamente va a intentar leerle (haya o no
// dato). Sólo registra el tiempo; NO toca ready ni rr. Índice fuera de rango = no-op.
inline void tof_sched_note_attempt(TofSchedState& st, uint8_t i, uint32_t now_ms) {
    if (i >= TOF_SCHED_N) return;
    st.last_attempt_ms[i] = now_ms;
}

// tof_sched_next(st, now_ms) — devuelve el ÍNDICE del próximo ToF READY (round-robin),
// y avanza el cursor para que el próximo llamado siga rotando. Si NINGUNO está ready
// devuelve TOF_SCHED_NONE (0xFF), SIN colgarse.
//
// Byte-equivalencia con el round-robin de hoy (sensors_tof.cpp): con los 4 ready y
// arrancando en rr=0, devuelve 0,1,2,3,0,1,... y deja rr en (i+1)%N — exacto igual que
// `const i = s_rr; s_rr = (s_rr+1)%NUM_TOF;`.
//
// Skip del caído: arranca en `rr` y avanza hasta TOF_SCHED_N posiciones buscando el
// primer ready. La cota de TOF_SCHED_N intentos garantiza terminación (no hay loop
// infinito ni siquiera con 0 ready). El parámetro now_ms no se usa en el modo normal
// (queda por simetría de API y para el modo priorizado); marcado (void) para no
// advertir.
inline uint8_t tof_sched_next(TofSchedState& st, uint32_t now_ms) {
    (void)now_ms;  // no usado en round-robin puro (sí en _prioritized)
    for (uint8_t k = 0; k < TOF_SCHED_N; ++k) {
        const uint8_t cand = static_cast<uint8_t>((st.rr + k) % TOF_SCHED_N);
        if (st.ready[cand]) {
            // Avanzar el cursor a UNO DESPUÉS del elegido → la próxima vez seguimos
            // rotando desde ahí (no re-evalúa al recién servido primero).
            st.rr = static_cast<uint8_t>((cand + 1u) % TOF_SCHED_N);
            return cand;
        }
    }
    // Ninguno ready: no movemos rr (no hay a quién darle el turno) y avisamos NONE.
    return TOF_SCHED_NONE;
}

// tof_sched_next_prioritized(st, now_ms) — OPCIONAL (default: usar tof_sched_next).
//
// Variante que, en vez de puro round-robin, le da el turno al ToF READY cuyo último
// intento es el MÁS VIEJO (el que hace más que no se lee). Útil si una rotación pura
// dejara un sensor sistemáticamente atrasado; reparte la "deuda" de lecturas de forma
// más pareja bajo caídas intermitentes. NO es el modo por defecto: cambia el orden de
// turnos (rompe byte-equivalencia), así que sólo se activa a propósito.
//
// Empate de antigüedad (mismo last_attempt_ms, p.ej. al arranque con todo en 0) →
// gana el de índice más bajo, en orden round-robin desde rr (determinístico). Si
// ninguno está ready → TOF_SCHED_NONE. Avanza rr igual que next() para que, si se
// mezcla con llamados a next(), el cursor quede coherente.
//
// Antigüedad wrap-safe: edad = now_ms - last_attempt_ms[i] (resta UNSIGNED). Más edad
// = más viejo = más prioridad.
inline uint8_t tof_sched_next_prioritized(TofSchedState& st, uint32_t now_ms) {
    uint8_t  best      = TOF_SCHED_NONE;
    uint32_t best_age  = 0;
    // Recorrer en orden round-robin desde rr para que el desempate sea estable y
    // coherente con la rotación.
    for (uint8_t k = 0; k < TOF_SCHED_N; ++k) {
        const uint8_t cand = static_cast<uint8_t>((st.rr + k) % TOF_SCHED_N);
        if (!st.ready[cand]) continue;
        const uint32_t age = now_ms - st.last_attempt_ms[cand];  // unsigned → wrap-safe
        if (best == TOF_SCHED_NONE || age > best_age) {
            best     = cand;
            best_age = age;
        }
    }
    if (best != TOF_SCHED_NONE) {
        st.rr = static_cast<uint8_t>((best + 1u) % TOF_SCHED_N);
    }
    return best;
}

}  // namespace iitasoccer
