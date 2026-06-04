// pose_fusion.cpp — Implementación del filtro complementario ToF+OTOS.
// Logica PURA (sin Arduino/Wire/hardware), todo en enteros.
//
// Ver pose_fusion.h para el contrato. El algoritmo de un tick:
//   PASO 0 — pass-through del heading del BNO.
//   PASO 1 — seed/inicialización (solo el primer tof_valid ancla).
//   PASO 2 — PREDICCIÓN: integrar el delta OTOS (con clamp anti-glitch).
//   PASO 3 — CORRECCIÓN: tirón suave hacia el ToF si pasa el gating.
//   PASO 4 — CONFIDENCE combinada (sube al anclar, decae en deriva pura).
//   PASO 5 — salida + clamp a cancha (solo la SALIDA, no el estado interno).

#include "pose_fusion.h"

namespace iitasoccer {

namespace {

// |v| absoluto para enteros (evita <cstdlib> y comportamiento de abs(int)).
inline int32_t iabs32(int32_t v) { return v < 0 ? -v : v; }

// Clamp entero genérico.
inline int32_t clampi(int32_t v, int32_t lo, int32_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Anti-glitch: acota el módulo del paso (dx,dy) a max_step_mm SIN sqrt.
// Estrategia barata: si cualquier componente o la suma L1 excede el budget,
// se escala el vector proporcionalmente con aritmética entera. Esto descarta
// saltos de reset/wrap de la OTOS (teletransporte) sin matar pasos normales.
//   - clamp por componente a max_step_mm.
//   - chequeo L1 (|dx|+|dy|) <= 2*max_step_mm; si excede, escalar por L1.
void clamp_vector_magnitude(int32_t& dx, int32_t& dy, int32_t max_step) {
    if (max_step <= 0) { dx = 0; dy = 0; return; }

    // 1) Clamp por componente.
    dx = clampi(dx, -max_step, max_step);
    dy = clampi(dy, -max_step, max_step);

    // 2) Clamp de la norma L1 a 2*max_step (cota barata de la magnitud).
    int32_t l1 = iabs32(dx) + iabs32(dy);
    int32_t budget = 2 * max_step;
    if (l1 > budget && l1 > 0) {
        // Escalar proporcionalmente: dx' = dx * budget / l1 (entero).
        dx = (dx * budget) / l1;
        dy = (dy * budget) / l1;
    }
}

}  // namespace

PoseFusionConfig pose_fusion_default_config() {
    PoseFusionConfig cfg{};
    cfg.field_width_mm        = 1820;   // eje X (lateral, lado corto)
    cfg.field_height_mm       = 2430;   // eje Y (arco-a-arco, lado largo)
    cfg.correction_gain_q8    = 26;    // K_real ~= 26/256 = 0.1016
    cfg.tof_jump_gate_mm      = 400;
    cfg.otos_stale_ms         = 60;
    cfg.tof_stale_ms          = 500;
    cfg.max_step_mm           = 80;
    cfg.conf_tof_anchor       = 90;
    cfg.conf_otos_only        = 50;
    cfg.conf_decay_per_100ms  = 5;
    cfg.conf_min              = 10;
    return cfg;
}

void pose_fusion_reset(PoseFusionState& st) {
    st.x_mm_q0          = 0;
    st.y_mm_q0          = 0;
    st.heading_centideg = 0;
    st.confidence       = 0;
    st.initialized      = false;
    st.otos_prev_x_mm   = 0;
    st.otos_prev_y_mm   = 0;
    st.otos_prev_valid  = false;
    st.ms_since_tof_corr = 0;
}

PoseFusionOutput pose_fusion_update(PoseFusionState& st,
                                    const PoseFusionInputs& in,
                                    const PoseFusionConfig& cfg) {
    PoseFusionOutput out{};
    out.source_flags = 0;

    // ---- PASO 0 — Pass-through de heading (SIEMPRE, no se fusiona) ----
    st.heading_centideg = in.bno_heading_centideg;

    // ---- PASO 1 — Seed / inicialización ----
    if (!st.initialized) {
        if (in.tof_valid) {
            // Primer absoluto disponible -> arrancamos anclados al ToF.
            st.x_mm_q0           = in.tof_x_mm;
            st.y_mm_q0           = in.tof_y_mm;
            st.confidence        = cfg.conf_tof_anchor;
            st.initialized       = true;
            st.ms_since_tof_corr = 0;
            out.source_flags |= POSE_FUSION_FLAG_TOF_CORR;  // el seed cuenta como anclaje
        } else {
            // La OTOS sola no provee origen absoluto: esperamos un ToF.
            st.confidence = 0;
        }

        // En ambos casos: registrar el sample OTOS como prev (si fresco) para
        // tener delta listo, y RETORNAR (no se predice en el tick de seed).
        if (in.otos_fresh) {
            st.otos_prev_x_mm  = in.otos_x_mm;
            st.otos_prev_y_mm  = in.otos_y_mm;
            st.otos_prev_valid = true;
        } else {
            st.otos_prev_valid = false;
        }

        out.heading_centideg = st.heading_centideg;
        out.confidence       = st.confidence;
        out.valid            = st.initialized;
        out.x_mm = static_cast<int16_t>(clampi(st.x_mm_q0, 0, cfg.field_width_mm));
        out.y_mm = static_cast<int16_t>(clampi(st.y_mm_q0, 0, cfg.field_height_mm));
        return out;
    }

    // ---- PASO 2 — PREDICCIÓN (integrar delta OTOS) ----
    if (in.otos_fresh) {
        if (st.otos_prev_valid) {
            int32_t dx = static_cast<int32_t>(in.otos_x_mm) - st.otos_prev_x_mm;
            int32_t dy = static_cast<int32_t>(in.otos_y_mm) - st.otos_prev_y_mm;
            // Anti-glitch: descarta saltos de reset/wrap de la OTOS.
            clamp_vector_magnitude(dx, dy, cfg.max_step_mm);
            st.x_mm_q0 += dx;
            st.y_mm_q0 += dy;
            out.source_flags |= POSE_FUSION_FLAG_OTOS_PRED;
        }
        // Actualizar prev (siempre que la OTOS esté fresca).
        st.otos_prev_x_mm  = in.otos_x_mm;
        st.otos_prev_y_mm  = in.otos_y_mm;
        st.otos_prev_valid = true;
    } else {
        // Sin predicción: mantener pose. Invalidar prev para recomputar el delta
        // cuando la OTOS vuelva (evita un delta gigante por el gap).
        st.otos_prev_valid = false;
    }

    // ---- PASO 3 — CORRECCIÓN (anclar a ToF) ----
    bool corrected_this_tick = false;
    if (in.tof_valid) {
        int32_t ex = static_cast<int32_t>(in.tof_x_mm) - st.x_mm_q0;  // residual ToF vs pose predicha
        int32_t ey = static_cast<int32_t>(in.tof_y_mm) - st.y_mm_q0;
        int32_t err = iabs32(ex);                                     // L-inf, barata
        if (iabs32(ey) > err) err = iabs32(ey);

        if (err > cfg.tof_jump_gate_mm) {
            // ToF saltó / inconsistente (robot rival, flip de pared): RECHAZAR.
            out.source_flags |= POSE_FUSION_FLAG_TOF_REJECT;
        } else {
            // Tirón suave hacia el absoluto: pose += K*(pose_tof - pose), K=gain/256.
            st.x_mm_q0 += (ex * static_cast<int32_t>(cfg.correction_gain_q8)) / 256;
            st.y_mm_q0 += (ey * static_cast<int32_t>(cfg.correction_gain_q8)) / 256;
            st.ms_since_tof_corr = 0;
            out.source_flags |= POSE_FUSION_FLAG_TOF_CORR;
            corrected_this_tick = true;
        }
    } else {
        // Contabilizar cuánto hace que no anclamos al absoluto.
        st.ms_since_tof_corr += in.dt_ms;
    }

    // ---- PASO 4 — CONFIDENCE combinada ----
    if (corrected_this_tick) {
        // Recién anclados: subimos directo al objetivo de anclaje.
        st.confidence = cfg.conf_tof_anchor;
    } else {
        // Deriva pura: la confidence decae con el tiempo sin ToF.
        // decay = (ms_since_tof_corr / 100) * conf_decay_per_100ms.
        uint32_t decay = (st.ms_since_tof_corr / 100u) *
                         static_cast<uint32_t>(cfg.conf_decay_per_100ms);
        int32_t conf = static_cast<int32_t>(cfg.conf_otos_only) - static_cast<int32_t>(decay);
        // Acotar a [conf_min, conf_otos_only].
        conf = clampi(conf, static_cast<int32_t>(cfg.conf_min),
                            static_cast<int32_t>(cfg.conf_otos_only));
        st.confidence = static_cast<uint8_t>(conf);
    }

    // ---- PASO 5 — Salida + clamp ----
    // El clamp aplica a la SALIDA; el estado interno st.x/y NO se re-clampa
    // para no sesgar la integración cuando el robot bordea la pared.
    out.x_mm = static_cast<int16_t>(clampi(st.x_mm_q0, 0, cfg.field_width_mm));
    out.y_mm = static_cast<int16_t>(clampi(st.y_mm_q0, 0, cfg.field_height_mm));
    out.heading_centideg = st.heading_centideg;
    out.confidence       = st.confidence;
    out.valid            = st.initialized;
    return out;
}

}  // namespace iitasoccer
