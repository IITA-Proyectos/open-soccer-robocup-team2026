// hcsr04_backoff.h — Backoff de muestreo del HC-SR04 (PURO, host-testeable).
//
// Por qué existe (audit 2026-06-05, hallazgo MEDIUM)
// --------------------------------------------------
// El HC-SR04 frontal se lee con pulseIn() BLOQUEANTE (src/top/sensors_tof.cpp
// read_hcsr04(), líneas 124-136). Cuando hay un objeto a < ~2 m el echo llega
// rápido y pulseIn() retorna en microsegundos. PERO cuando el sensor mira al
// vacío (nada en rango), o el módulo "flota" sin eco, pulseIn() se queda
// esperando hasta el timeout COMPLETO de 12 ms y recién ahí devuelve 0
// (TOF_NO_READING). Hoy se lee cada 3 ticks de ToF (~90 ms): en cancha abierta,
// con la pared lejos, eso es un robo FIJO de ~12 ms al loop de 100 Hz cada 3
// ticks — justo cuando el robot está lejos de todo y MÁS necesita reflejos.
//
// La observación clave: el HC-SR04 es REDUNDANTE (el min_obstacle ya lo aportan
// los 4 ToF VL53L7CX; el HC-SR04 es solo respaldo, ver nota en sensors_tof.cpp
// líneas 110-121). Por eso, cuando el HC-SR04 lleva un rato SIN eco, no hace
// falta seguir pagando 12 ms cada 90 ms para confirmar "sigue sin ver nada":
// alcanza con sondearlo de vez en cuando para detectar si volvió a aparecer un
// obstáculo. Este módulo encapsula esa decisión: cuenta los disparos
// consecutivos sin eco y, pasado un umbral, ESPACIA el muestreo (de cada
// ~NORMAL ticks a cada ~BACKOFF ticks). En cuanto vuelve un eco, resetea y
// retoma el ritmo rápido.
//
// CÓMO SE USA: el wrapper (sensors_tof_tick) pregunta hcsr04_backoff_should_fire()
// con el tick actual; si dice true, dispara read_hcsr04() y reporta el resultado
// con hcsr04_backoff_note_result(got_echo). El módulo NO llama a pulseIn ni toca
// pines: solo decide CUÁNDO conviene gastar la lectura bloqueante. La ACCIÓN
// (saltear o no) la aplica el wrapper, gated tras -DTOP_HCSR04_BACKOFF.
//
// PURO: sin Arduino/Wire. POD + funciones libres → .bss zero-init y g++ host.
// El tick entra como uint32 (el caller elige la fuente: g_tick_count del módulo
// ToF); el módulo solo hace aritmética wrap-safe (resta unsigned) sobre él.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Ritmo NORMAL: cada cuántos ticks se dispara el HC-SR04 mientras VE eco (o aún
// no entró en backoff). Igual al comportamiento actual del firmware (cada 3
// ticks de ToF ≈ 90 ms) para no cambiar el caso "hay obstáculo cerca".
constexpr uint32_t HCSR04_NORMAL_PERIOD_TICKS = 3;

// Ritmo de BACKOFF: cada cuántos ticks se sondea cuando lleva N disparos sin eco.
// ~30 ticks ≈ 900 ms: suficiente para re-detectar un obstáculo que se acerca
// (a 100 Hz de loop, el robot apenas se mueve en 900 ms) pero deja de robar
// 12 ms cada 90 ms en cancha abierta (10x menos lecturas bloqueantes).
constexpr uint32_t HCSR04_BACKOFF_PERIOD_TICKS = 30;

// Cuántos disparos CONSECUTIVOS sin eco hacen falta para entrar en backoff. Con
// un margen (no al primer no-eco): un par de frames perdidos por un objeto en el
// borde del rango NO debe degradar el ritmo. 3 disparos sin eco seguidos ≈ el
// sensor realmente no ve nada, no un glitch puntual.
constexpr uint16_t HCSR04_BACKOFF_ENTER_MISSES = 3;

// Config del backoff (overridable; defaults arriba).
struct Hcsr04BackoffCfg {
    uint32_t normal_period_ticks;   // periodo de muestreo viendo eco
    uint32_t backoff_period_ticks;  // periodo de muestreo en backoff (más espaciado)
    uint16_t enter_misses;          // no-ecos consecutivos para entrar en backoff
};

inline Hcsr04BackoffCfg hcsr04_backoff_default_cfg() {
    Hcsr04BackoffCfg c;
    c.normal_period_ticks  = HCSR04_NORMAL_PERIOD_TICKS;
    c.backoff_period_ticks = HCSR04_BACKOFF_PERIOD_TICKS;
    c.enter_misses         = HCSR04_BACKOFF_ENTER_MISSES;
    return c;
}

// Estado del backoff (persiste entre ticks). Zero-init (.bss) == estado inicial
// válido: primed=false → el primer should_fire() siembra last_fire_tick y
// dispara (arranca leyendo, sin asumir un tick previo); misses=0 → ritmo normal.
struct Hcsr04Backoff {
    bool     primed;          // ¿ya hubo un primer disparo que sembró la referencia?
    uint32_t last_fire_tick;  // tick en el que se disparó por última vez
    uint16_t misses;          // disparos consecutivos sin eco (satura; resetea al ver eco)
};

// Lo deja en estado limpio (sin referencia, sin backoff). Llamar al boot o tras
// re-init del sensor.
inline void hcsr04_backoff_reset(Hcsr04Backoff& s) {
    s.primed = false;
    s.last_fire_tick = 0;
    s.misses = 0;
}

// ¿Estamos en backoff? (acumuló >= enter_misses disparos sin eco). Útil para
// telemetría/tests; should_fire() la usa para elegir el periodo.
inline bool hcsr04_backoff_active(const Hcsr04Backoff& s, const Hcsr04BackoffCfg& cfg) {
    return s.misses >= cfg.enter_misses;
}

// Decide si en ESTE tick conviene disparar el HC-SR04 (gastar el pulseIn
// bloqueante). NO muta el estado salvo la siembra inicial: el caller, si
// dispara, debe llamar hcsr04_backoff_note_result() con el resultado.
//
// Lógica:
//   • Primer llamado (no primed) => sí: arranca leyendo y siembra la referencia.
//   • Si NO está en backoff => dispara cuando pasaron >= normal_period_ticks
//     desde el último disparo (ritmo actual del firmware).
//   • Si está en backoff (misses >= enter_misses) => dispara solo cada
//     backoff_period_ticks (espaciado, deja de robar el loop en cancha abierta).
//
// La resta `now_tick - last_fire_tick` es unsigned => WRAP-SAFE ante el overflow
// del contador de ticks (mientras el periodo sea << 2^32, siempre cierto).
inline bool hcsr04_backoff_should_fire(Hcsr04Backoff& s, uint32_t now_tick,
                                       const Hcsr04BackoffCfg& cfg) {
    if (!s.primed) {
        s.primed = true;
        s.last_fire_tick = now_tick;
        return true;   // primer disparo: arrancamos leyendo
    }
    const uint32_t period = hcsr04_backoff_active(s, cfg)
                                ? cfg.backoff_period_ticks
                                : cfg.normal_period_ticks;
    const uint32_t elapsed = now_tick - s.last_fire_tick;  // wrap-safe
    return elapsed >= period;
}

// Registra el resultado del disparo que el caller acaba de hacer (porque
// should_fire() dijo true). Sella el tick del disparo y actualiza la racha de
// no-ecos.
//
//   • got_echo=true  => el sensor vio algo: resetea la racha (sale de backoff,
//     vuelve al ritmo rápido en el próximo should_fire).
//   • got_echo=false => no hubo eco: extiende la racha (con saturación) hasta
//     que cruza enter_misses y entra en backoff.
//
// IMPORTANTE: llamar SIEMPRE que el caller disparó, para mantener
// last_fire_tick alineado con el periodo elegido por should_fire().
inline void hcsr04_backoff_note_result(Hcsr04Backoff& s, uint32_t now_tick,
                                       bool got_echo) {
    s.last_fire_tick = now_tick;
    if (got_echo) {
        s.misses = 0;
    } else if (s.misses < 0xFFFF) {
        s.misses++;
    }
}

}  // namespace iitasoccer
