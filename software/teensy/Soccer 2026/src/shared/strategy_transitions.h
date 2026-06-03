// strategy_transitions.h — Árbol de decisión PURO de la FSM (caracterización).
//
// ⚠️ LEER ESTO ANTES DE USAR ⚠️
//
// Este módulo NO lo llama strategy.cpp (todavía). Es una RÉPLICA FIEL del árbol
// de transiciones de `src/central/strategy.cpp` (attacker_tick / goalkeeper_tick),
// extraída como funciones puras testeables host-native.
//
// Propósito: dar una RED DE CARACTERIZACIÓN. Los tests sobre estas funciones
// fijan el comportamiento de transición que la FSM tiene HOY (commit b10c66d,
// Nivel 2). Si alguien cambia la lógica de la FSM, primero la cambia acá, los
// tests fallan, y queda explícito qué transición cambió.
//
// LÍMITE HONESTO: mientras strategy.cpp no llame a estas funciones, los tests
// prueban la LÓGICA replicada, no el binario que corre en el robot. Conectar
// strategy.cpp → strategy_transitions es un paso SEPARADO (tema-a-analizar con
// su propio risk-fix, ver journal 2026-05-15). No se hizo acá por decisión del
// coach (cero riesgo de tocar el cerebro que ya anda).
//
// REGLA DE SINCRONIZACIÓN: si tocás el árbol de transiciones en strategy.cpp,
// tenés que reflejar el cambio acá y correr `pio test -e test_native -f
// test_strategy_transitions`. Si no, esta caracterización miente.
//
// Pure functions — sin Arduino. El tiempo (millis) se inyecta por parámetro.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// === Enums espejo ===
// Mismo orden que los `enum class AtkState/GkState` del anonymous namespace de
// strategy.cpp (no son accesibles desde acá — son file-local en el .cpp).
enum class AtkPhase : uint8_t {
    WAIT_START, KICKOFF, SEARCH, POSITION, APPROACH, LINE_AVOID
};
enum class GkPhase : uint8_t {
    WAIT_START, PATROL, INTERCEPT, CLEAR, LINE_AVOID
};

// === Vista del mundo que consume la FSM (lo que world_model expone) ===
struct AtkWorldView {
    bool     match_running;
    bool     imminent_exit;
    bool     line_fresh;
    bool     ball_visible;
    bool     goal_visible;       // arco rival visible
    float    ball_x_mm;          // relativo al robot
    float    ball_y_mm;
    float    goal_angle_deg;     // ángulo al arco rival respecto al frente
    uint32_t now_ms;
    uint32_t kickoff_started_ms; // timestamp guardado cuando se entró a KICKOFF
};

struct GkWorldView {
    bool     match_running;
    bool     imminent_exit;
    bool     line_fresh;
    bool     ball_visible;
    float    ball_x_mm;
    float    ball_y_mm;
};

// === Constantes de tuning de la FSM ===
// DEBEN coincidir con las constexpr de strategy.cpp (líneas ~76-102 del commit
// b10c66d). Si cambian allá, cambiarlas acá. Se pasan como struct para que los
// tests puedan variar thresholds sin tocar el código.
struct AtkTuning {
    float    attack_line_tol_deg;   // ATK_ATTACK_LINE_TOL_DEG       (30)
    float    position_reached_mm;   // ATK_POSITION_REACHED_MM       (80)
    float    behind_ball_gap_mm;    // ATK_BEHIND_BALL_GAP_MM        (120)
    float    kick_dist_mm;          // ATK_KICK_DIST_MM              (80)
    float    kick_angle_deg;        // ATK_KICK_ANGLE_DEG            (12)
    uint32_t kickoff_duration_ms;   // ATK_KICKOFF_DURATION_MS       (250)
};

struct GkTuning {
    float clear_trigger_mm;         // GK_CLEAR_TRIGGER_MM           (250)
    float clear_release_mm;         // GK_CLEAR_RELEASE_MM           (400)
};

// Factories con los valores que strategy.cpp usa hoy. Un test que use estos
// defaults está testeando la FSM tal cual corre en el robot (módulo el acople
// documentado: si strategy.cpp cambia los constexpr, actualizar acá).
AtkTuning atk_tuning_default();
GkTuning  gk_tuning_default();

// === Resultado de una decisión de transición ===
struct AtkDecision {
    AtkPhase next_phase;
    // El robot NO tiene kicker físico. Este flag ya no dispara nada: marca el
    // "punto de empuje alineado" (pelota cerca + apuntando al arco), que es
    // pura geometría de alineación. Se conserva como red de caracterización de
    // strategy.cpp; el robot sigue empujando la pelota por inercia.
    bool     kicker_fire;        // ¿este tick el robot está alineado para empujar?
    bool     start_kickoff_timer;// ¿el caller debe guardar now como kickoff_started?
};

struct GkDecision {
    GkPhase next_phase;
};

// === Funciones puras de decisión ===
//
// Replican EXACTAMENTE la secuencia de strategy.cpp:
//   1. Transiciones globales prioritarias (match parado / línea / flanco kickoff).
//   2. Transición por-estado sobre el estado intermedio resultante.
//
// `prev_match_running` es el estado de g_match_was_running ANTES de este tick
// (para detectar el flanco STOP→RUN que dispara KICKOFF).
AtkDecision atk_decide_transition(AtkPhase current,
                                  bool prev_match_running,
                                  const AtkWorldView& w,
                                  const AtkTuning& t);

GkDecision gk_decide_transition(GkPhase current,
                                const GkWorldView& w,
                                const GkTuning& t);

}  // namespace iitasoccer
