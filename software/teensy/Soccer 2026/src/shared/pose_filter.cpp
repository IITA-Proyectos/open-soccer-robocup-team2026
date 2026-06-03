#include "pose_filter.h"

namespace iitasoccer {

namespace {

constexpr int32_t CENTIDEG_FULL = 36000;   // 360.00 grados en centideg
constexpr int32_t CENTIDEG_HALF = 18000;   // 180.00 grados en centideg

// Normaliza un ángulo en centideg al rango [-18000, 18000].
// Robusto para valores grandes (lazo, no asume <2 vueltas).
int32_t norm_centideg(int32_t v) {
    // Trae a [0, 36000) primero (módulo bien definido para negativos).
    v %= CENTIDEG_FULL;
    if (v < 0) v += CENTIDEG_FULL;
    if (v > CENTIDEG_HALF) v -= CENTIDEG_FULL;   // [0,36000) -> (-18000,18000]
    return v;
}

// Diferencia circular corta a-b en [-18000, 18000].
int32_t diff_centideg(int32_t a, int32_t b) {
    return norm_centideg(a - b);
}

// Inserta `p` en la ventana circular MEDIAN del estado.
void window_push(PoseFilterState& st, const FilterPose& p, uint8_t n) {
    if (n < 1) n = 1;
    if (n > POSE_FILTER_MAX_WINDOW) n = POSE_FILTER_MAX_WINDOW;
    st.win_x[st.win_head] = p.x_mm;
    st.win_y[st.win_head] = p.y_mm;
    st.win_h[st.win_head] = p.heading_centideg;
    st.win_head = static_cast<uint8_t>((st.win_head + 1) % n);
    if (st.win_count < n) st.win_count = static_cast<uint8_t>(st.win_count + 1);
}

// Mediana lineal de hasta 5 enteros (ordenamiento O(N^2) trivial, in-place copia).
int16_t median_lineal(const int16_t* src, uint8_t count) {
    int16_t v[POSE_FILTER_MAX_WINDOW];
    if (count > POSE_FILTER_MAX_WINDOW) count = POSE_FILTER_MAX_WINDOW;
    for (uint8_t i = 0; i < count; ++i) v[i] = src[i];
    // insertion sort
    for (uint8_t i = 1; i < count; ++i) {
        int16_t key = v[i];
        int j = static_cast<int>(i) - 1;
        while (j >= 0 && v[j] > key) { v[j + 1] = v[j]; --j; }
        v[j + 1] = key;
    }
    if (count == 0) return 0;
    return v[count / 2];   // elemento central (para N par, el de la mitad superior)
}

// Confianza de salida durante un hold: decae linealmente con hold_count hasta 0
// cuando hold_count >= max_hold_cycles. base = confianza de la última buena.
uint8_t hold_decay(uint8_t base, uint8_t hold_count, uint8_t max_hold) {
    if (max_hold == 0) return 0;
    if (hold_count >= max_hold) return 0;
    // remaining = base * (max_hold - hold_count) / max_hold
    int32_t rem = static_cast<int32_t>(base) *
                  static_cast<int32_t>(max_hold - hold_count) /
                  static_cast<int32_t>(max_hold);
    if (rem < 0) rem = 0;
    if (rem > 255) rem = 255;
    return static_cast<uint8_t>(rem);
}

}  // namespace

// ============================ Helpers públicos ============================

int32_t pose_filter_jump_sq_mm2(const FilterPose& a, const FilterPose& b) {
    int32_t dx = static_cast<int32_t>(a.x_mm) - static_cast<int32_t>(b.x_mm);
    int32_t dy = static_cast<int32_t>(a.y_mm) - static_cast<int32_t>(b.y_mm);
    return dx * dx + dy * dy;   // max ~9.2e6, cabe en int32
}

int16_t pose_filter_ema_step(int16_t prev, int16_t cur, uint8_t num, uint8_t den) {
    if (den == 0) return cur;            // guard: den inválido => sin suavizado
    int32_t delta = static_cast<int32_t>(cur) - static_cast<int32_t>(prev);
    // truncamiento a cero (C-style): la división entera de C ya trunca a cero.
    int32_t step = (delta * static_cast<int32_t>(num)) / static_cast<int32_t>(den);
    int32_t out = static_cast<int32_t>(prev) + step;
    if (out > 32767) out = 32767;
    if (out < -32768) out = -32768;
    return static_cast<int16_t>(out);
}

int16_t pose_filter_circular_mean_centideg(const int16_t* vals, uint8_t n) {
    if (n == 0) return 0;
    if (n == 1) return static_cast<int16_t>(norm_centideg(vals[0]));
    if (n > POSE_FILTER_MAX_WINDOW) n = POSE_FILTER_MAX_WINDOW;
    // Desenrollar cada valor al camino corto respecto al PRIMERO, luego median
    // sobre los desenrollados y renormalizar. Maneja el cruce ±18000.
    int32_t base = vals[0];
    int32_t unwrapped[POSE_FILTER_MAX_WINDOW];
    for (uint8_t i = 0; i < n; ++i) {
        int32_t d = diff_centideg(vals[i], base);   // [-18000,18000]
        unwrapped[i] = base + d;
    }
    // median lineal de los desenrollados (insertion sort sobre int32 local).
    int32_t v[POSE_FILTER_MAX_WINDOW];
    for (uint8_t i = 0; i < n; ++i) v[i] = unwrapped[i];
    for (uint8_t i = 1; i < n; ++i) {
        int32_t key = v[i];
        int j = static_cast<int>(i) - 1;
        while (j >= 0 && v[j] > key) { v[j + 1] = v[j]; --j; }
        v[j + 1] = key;
    }
    return static_cast<int16_t>(norm_centideg(v[n / 2]));
}

bool pose_filter_is_stale(const PoseFilterState& st, const PoseFilterConfig& cfg) {
    return st.hold_count >= cfg.max_hold_cycles;
}

// ============================ Config / init ============================

PoseFilterConfig pose_filter_default_config() {
    PoseFilterConfig c;
    c.smooth_mode    = PoseSmooth::EMA;
    c.ema_alpha_num  = 1;
    c.ema_alpha_den  = 4;      // alpha = 0.25
    c.median_window  = 3;
    c.max_jump_mm    = 400;
    c.max_hold_cycles = 5;
    c.warmup_cycles  = 2;
    return c;
}

void pose_filter_init(PoseFilterState& st) {
    st.out.x_mm = 0;
    st.out.y_mm = 0;
    st.out.heading_centideg = 0;
    st.out.confidence = 0;
    st.primed = false;
    st.valid_seen = 0;
    st.hold_count = 0;
    for (int i = 0; i < POSE_FILTER_MAX_WINDOW; ++i) {
        st.win_x[i] = 0;
        st.win_y[i] = 0;
        st.win_h[i] = 0;
    }
    st.win_head = 0;
    st.win_count = 0;
    st.last_reject = 0;
}

// ============================ Update (pipeline) ============================

FilterPose pose_filter_update(PoseFilterState& st,
                              const PoseFilterConfig& cfg,
                              const FilterPose& measured,
                              bool valid) {
    // --- PASO 0: SEMBRADO (primer ciclo / no primed) ---
    if (!st.primed) {
        if (valid) {
            st.out = measured;
            st.primed = true;
            st.valid_seen = 1;
            st.hold_count = 0;
            st.last_reject = 0;
            // sembrar también la ventana MEDIAN
            st.win_head = 0;
            st.win_count = 0;
            window_push(st, measured, cfg.median_window);
        } else {
            // No sembrar basura: out queda {0,0,0,0}, primed sigue false.
            st.last_reject = 1;
        }
        return st.out;
    }

    // --- PASO 1: GATE de invalidez ---
    if (!valid) {
        st.hold_count = static_cast<uint8_t>(st.hold_count + 1);
        st.last_reject = pose_filter_is_stale(st, cfg) ? 3 : 1;
        st.out.confidence = hold_decay(st.out.confidence, st.hold_count, cfg.max_hold_cycles);
        return st.out;   // hold last good
    }

    // --- PASO 2: GATE de salto físicamente imposible (solo tras warmup) ---
    bool gate_active = (st.valid_seen >= cfg.warmup_cycles);
    if (gate_active) {
        int32_t jump2 = pose_filter_jump_sq_mm2(measured, st.out);
        int32_t thr2  = static_cast<int32_t>(cfg.max_jump_mm) *
                        static_cast<int32_t>(cfg.max_jump_mm);
        if (jump2 > thr2) {
            // outlier/teleport: NO integrar; sostener última buena.
            st.hold_count = static_cast<uint8_t>(st.hold_count + 1);
            st.last_reject = pose_filter_is_stale(st, cfg) ? 3 : 2;
            st.out.confidence = hold_decay(st.out.confidence, st.hold_count, cfg.max_hold_cycles);
            return st.out;
        }
    }

    // --- PASO 3: RESET de hold (medición aceptada) ---
    st.hold_count = 0;
    st.last_reject = 0;
    if (st.valid_seen < cfg.warmup_cycles) {
        st.valid_seen = static_cast<uint8_t>(st.valid_seen + 1);
    }

    // --- PASO 4: SUAVIZADO ---
    if (cfg.smooth_mode == PoseSmooth::EMA) {
        st.out.x_mm = pose_filter_ema_step(st.out.x_mm, measured.x_mm,
                                           cfg.ema_alpha_num, cfg.ema_alpha_den);
        st.out.y_mm = pose_filter_ema_step(st.out.y_mm, measured.y_mm,
                                           cfg.ema_alpha_num, cfg.ema_alpha_den);
        // heading: EMA CIRCULAR sobre el delta corto.
        int32_t d = diff_centideg(measured.heading_centideg, st.out.heading_centideg);
        // mismo paso entero que ema_step pero sobre el delta (prev implícito = 0).
        int32_t step = (d * static_cast<int32_t>(cfg.ema_alpha_num)) /
                       (cfg.ema_alpha_den == 0 ? 1 : static_cast<int32_t>(cfg.ema_alpha_den));
        st.out.heading_centideg =
            static_cast<int16_t>(norm_centideg(st.out.heading_centideg + step));
    } else {  // MEDIAN
        window_push(st, measured, cfg.median_window);
        st.out.x_mm = median_lineal(st.win_x, st.win_count);
        st.out.y_mm = median_lineal(st.win_y, st.win_count);
        st.out.heading_centideg =
            pose_filter_circular_mean_centideg(st.win_h, st.win_count);
    }
    // medición fresca aceptada => confianza plena del productor.
    st.out.confidence = measured.confidence;

    // --- PASO 5: return ---
    return st.out;
}

}  // namespace iitasoccer
