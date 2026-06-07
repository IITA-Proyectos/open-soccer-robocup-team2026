// bench_mode.cpp — implementación pura de la máquina de estados de hold por
// heartbeat. Sin estado global, sin Arduino. Ver bench_mode.h para el contrato.

#include "bench_mode.h"

namespace iitasoccer {

void bench_hold_init(BenchHold& b) {
    b.last_ok_ms = 0;
    b.ever       = false;  // sin token previo → COMPETENCIA garantizado
}

void bench_hold_feed(BenchHold& b, uint16_t magic, uint32_t now_ms) {
    // Anti-accidente: SÓLO el magic exacto refresca. Un magic distinto NO toca
    // el estado — el hold sigue su decaimiento natural (no se refresca ni se
    // reinicia), de modo que el ruido jamás puede sostener PRUEBA.
    if (magic != BENCH_TOKEN_MAGIC) return;
    b.last_ok_ms = now_ms;
    b.ever       = true;
}

BenchMode bench_hold_mode(const BenchHold& b, uint32_t now_ms) {
    if (!b.ever) return BenchMode::COMPETENCIA;          // nunca hubo token bueno
    // WRAP-SAFE: resta unsigned (mismo patrón que tof_fresh_or_no_reading).
    const uint32_t elapsed = now_ms - b.last_ok_ms;
    if (elapsed > BENCH_HOLD_TIMEOUT_MS) return BenchMode::COMPETENCIA;  // vencido
    return BenchMode::PRUEBA;                            // heartbeat fresco
}

bool bench_hold_in_prueba(const BenchHold& b, uint32_t now_ms) {
    return bench_hold_mode(b, now_ms) == BenchMode::PRUEBA;
}

}  // namespace iitasoccer
