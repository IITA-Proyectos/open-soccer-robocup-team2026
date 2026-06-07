// bench_mode.h — modo PRUEBA/COMPETENCIA fail-safe por HEARTBEAT.
//
// FASE 1: NÚCLEO PURO (solo <stdint.h>, SIN Arduino), igual que
// tof_distance_hold.h / drive_straight.h / telemetry_down.h. Vive en src/shared,
// namespace iitasoccer, host-testeable. NO toca ningún binario de competencia —
// nadie lo cablea todavía; sólo expone la máquina de estados para que el glue
// (fase posterior) la consuma.
//
// ── Para qué sirve ──────────────────────────────────────────────────────────
// En el banco queremos poder poner el robot en modo PRUEBA (p.ej. habilitar
// diagnósticos, mover motores a mano, ignorar el árbitro) SIN riesgo de que se
// quede así en competencia. La regla fail-safe: el modo PRUEBA sólo se SOSTIENE
// mientras llegue un "heartbeat" (token de hold) fresco; si el heartbeat se
// corta (cable suelto, host colgado, reset), el robot REVIERTE solo a
// COMPETENCIA en BENCH_HOLD_TIMEOUT_MS. El default es siempre COMPETENCIA.
//
// ── Anti-accidente ──────────────────────────────────────────────────────────
// Sólo un token con el magic EXACTO (BENCH_TOKEN_MAGIC) refresca el hold.
// Ruido en la línea, frames corruptos o magics distintos NO entran en PRUEBA:
// el robot NUNCA queda habilitado por casualidad. (Ver test ADVERSARIAL.)
//
// ── WRAP-SAFE ───────────────────────────────────────────────────────────────
// El cálculo de frescura usa la MISMA resta unsigned (now - last_ok) que
// tof_fresh_or_no_reading: si millis() envuelve (~49.7 días) la resta sigue
// dando el elapsed correcto y no dispara un falso-fresco.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Modo VIGENTE del robot. COMPETENCIA = 0 es el default/fail-safe deliberado:
// un BenchHold a ceros (o vencido) computa COMPETENCIA.
enum class BenchMode : uint8_t {
    COMPETENCIA = 0,  // comportamiento de partido (default, fail-safe)
    PRUEBA      = 1,  // banco/diagnóstico — sólo mientras llegue heartbeat fresco
};

// Magic del token de hold. SÓLO este valor refresca el hold; cualquier otro se
// ignora. Valor arbitrario distinto de 0/0xFFFF para no colisionar con ruido
// trivial (línea floja, frame en blanco, frame todo-unos).
constexpr uint16_t BENCH_TOKEN_MAGIC = 0xB42E;

// Sin un token fresco dentro de esta ventana → revierte a COMPETENCIA.
constexpr uint32_t BENCH_HOLD_TIMEOUT_MS = 300;

// Estado del hold (POD plano, sin Arduino). `ever` evita que un BenchHold recién
// init computado a un now grande dé un falso PRUEBA por last_ok_ms==0.
struct BenchHold {
    uint32_t last_ok_ms;  // millis() del último token VÁLIDO (sólo vale si ever)
    bool     ever;        // ¿hubo alguna vez un token con magic correcto?
};

// Inicializa el hold a "nunca hubo token" → modo COMPETENCIA garantizado.
void bench_hold_init(BenchHold& b);

// Procesa un token de "hold" recibido. SÓLO refresca el hold (last_ok_ms=now_ms,
// ever=true) si magic == BENCH_TOKEN_MAGIC. Con cualquier otro magic NO toca el
// estado (no refresca, no resetea): el hold simplemente sigue decayendo.
void bench_hold_feed(BenchHold& b, uint16_t magic, uint32_t now_ms);

// Modo VIGENTE en now_ms. PRUEBA sólo si hubo token válido (ever) y NO venció el
// timeout (elapsed = now-last_ok unsigned, wrap-safe). Si no, COMPETENCIA.
BenchMode bench_hold_mode(const BenchHold& b, uint32_t now_ms);

// Atajo booleano: true sii bench_hold_mode(...) == PRUEBA.
bool bench_hold_in_prueba(const BenchHold& b, uint32_t now_ms);

}  // namespace iitasoccer
