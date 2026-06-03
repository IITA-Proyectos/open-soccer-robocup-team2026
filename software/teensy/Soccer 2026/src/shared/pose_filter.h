// pose_filter.h — Filtro temporal de pose (suavizado + gate de saltos imposibles).
//
// Módulo PURO host-testeable (sin Arduino.h, sin Wire, sin hardware). Toma UNA
// pose por ciclo (x_mm, y_mm, heading_centideg) + un flag `valid` y devuelve una
// pose filtrada estable. Sigue el idiom struct-de-estado + función update() ya
// usado en imu_fusion.h y line_filters.h (FilterBuffer / LiftedDetector).
//
// QUÉ HACE (pipeline por ciclo):
//   0. SEMBRADO — el primer ciclo válido siembra la salida (out = measured) sin
//      suavizar ni gatear (no hay referencia previa).
//   1. GATE de invalidez — si !valid, sostiene la última pose buena ("hold last
//      good") y degrada la confianza; tras max_hold_cycles la salida es stale.
//   2. GATE de salto físicamente imposible — un salto euclidiano > max_jump_mm
//      en un ciclo es un teleport (outlier): NO se integra, se sostiene la última
//      buena (igual que invalid). Ortogonal al suavizado: corta el outlier ANTES
//      de que contamine el EMA/ventana.
//   3. SUAVIZADO — EMA entero (fixed-point alpha=num/den, determinista, sin float
//      para x/y) o MEDIAN de N (rechaza spikes impulsivos puntuales por completo).
//      El heading se suaviza CIRCULAR (maneja wrap ±18000 centideg).
//
// QUÉ NO HACE:
//   No conoce ToF, paredes, ni localization.cpp. Recibe una FilterPose abstracta:
//   puede filtrar pose de ToF, de OTOS, o de una fusión futura. Es independiente
//   de F1 (localization): el caller le pasa la salida de localization_compute() o
//   cualquier otra fuente de pose.
//
// CONVENCIÓN: heading en centideg, rango [-18000,18000], CCW+ (igual que Pose2D
// de types.h). x/y en mm enteros (cancha 2430x1820 cabe holgado en int16).
// No se reusa types.h a propósito, para mantener el módulo 100% standalone.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// === Entrada/salida de pose (espejo de Pose2D, NO se reusa types.h) ===
struct FilterPose {
    int16_t x_mm;              // posición X en cancha (mm)
    int16_t y_mm;              // posición Y en cancha (mm)
    int16_t heading_centideg;  // heading -18000..18000 (CCW+, igual que Pose2D)
    uint8_t confidence;        // 0..100; salida = confianza de entrada degradada por hold
};

// === Modo de suavizado ===
enum class PoseSmooth : uint8_t { EMA = 0, MEDIAN = 1 };

// === Config estática (tuning; el caller la fija una vez al boot) ===
struct PoseFilterConfig {
    PoseSmooth smooth_mode;       // EMA (default) o MEDIAN de N
    uint8_t    ema_alpha_num;     // EMA: out += (new-out)*num/den. default 1
    uint8_t    ema_alpha_den;     //   den>=1; default 4  => alpha=0.25
    uint8_t    median_window;     // MEDIAN: tamaño N impar (3 o 5). default 3
    uint16_t   max_jump_mm;       // gate: salto euclidiano >X mm/ciclo => outlier. default 400
    uint8_t    max_hold_cycles;   // cuántos ciclos sostener la última pose buena
                                  //   ante invalid/outlier antes de marcar stale. default 5
    uint8_t    warmup_cycles;     // ciclos válidos antes de habilitar el gate (sembrado).
                                  //   default 2 (no gatear hasta tener referencia confiable)
};

// Devuelve los defaults documentados arriba.
PoseFilterConfig pose_filter_default_config();

// === Estado (persiste entre ciclos; lo maneja el módulo, lo posee el caller) ===
constexpr int POSE_FILTER_MAX_WINDOW = 5;   // ventana MEDIAN máxima

struct PoseFilterState {
    FilterPose out;            // última pose FILTRADA emitida (la "buena")
    bool       primed;         // false hasta el 1er update válido (sembrado)
    uint8_t    valid_seen;     // ciclos válidos acumulados (clamp a warmup)
    uint8_t    hold_count;     // ciclos consecutivos sosteniendo (invalid/outlier)
    // buffers para MEDIAN (ignorados en EMA):
    int16_t    win_x[POSE_FILTER_MAX_WINDOW];   // ventana circular X
    int16_t    win_y[POSE_FILTER_MAX_WINDOW];   // ventana circular Y
    int16_t    win_h[POSE_FILTER_MAX_WINDOW];   // ventana circular heading (centideg)
    uint8_t    win_head;       // índice de escritura circular
    uint8_t    win_count;      // cuántas muestras válidas hay en la ventana (<=N)
    uint8_t    last_reject;    // diag: 0=ok,1=invalid,2=jump-gate,3=hold-expired(stale)
};

// === API ===

// Pone primed=false, contadores en 0, out={0,0,0,0}. Llamar una vez al boot.
void pose_filter_init(PoseFilterState& st);

// FUNCIÓN PRINCIPAL. measured = pose cruda de este ciclo (ej. de localization).
// valid = el productor considera la medición utilizable este ciclo.
// Devuelve la pose filtrada a usar (== st.out tras el update). Sin side effects
// fuera de st. Determinista: misma (st,cfg,measured,valid) => mismo resultado.
FilterPose pose_filter_update(PoseFilterState& st,
                              const PoseFilterConfig& cfg,
                              const FilterPose& measured,
                              bool valid);

// === Helpers puros (expuestos para test) ===

// true si hold_count >= max_hold_cycles (la salida ya no es confiable).
bool pose_filter_is_stale(const PoseFilterState& st, const PoseFilterConfig& cfg);

// distancia euclidiana al cuadrado (mm^2) entre a y b en XY (sin sqrt; cabe en
// int32: max ~ 2430^2+1820^2 ~ 9.2e6 < 2.1e9). Para comparar vs max_jump_mm^2.
int32_t pose_filter_jump_sq_mm2(const FilterPose& a, const FilterPose& b);

// un paso EMA entero: prev + (cur-prev)*num/den (truncado a cero, C-style).
int16_t pose_filter_ema_step(int16_t prev, int16_t cur, uint8_t num, uint8_t den);

// promedio/median CIRCULAR de heading en centideg (maneja wrap ±18000). Para
// MEDIAN y para suavizar heading sin saltar en el cruce ±180.
int16_t pose_filter_circular_mean_centideg(const int16_t* vals, uint8_t n);

}  // namespace iitasoccer
