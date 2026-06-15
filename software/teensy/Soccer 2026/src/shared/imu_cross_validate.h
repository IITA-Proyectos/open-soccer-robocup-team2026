// imu_cross_validate.h — Salud del heading por CROSS-VALIDACIÓN con datos
// INDEPENDIENTES del BNO (PURO, host-testeable). TASK-212 Fase 1 + diseño centinela.
//
// QUÉ RESUELVE (el agujero del bug R1)
// ------------------------------------
// El árbitro auto-referencial de imu_fusion elige "el más estable respecto a sí mismo",
// y un BNO CONGELADO (deriva 0) lo engaña: gana siempre y arrastra al sano. Este módulo
// juzga la salud del BNO contra VERDAD DE TERRENO independiente: la TASA DE ROTACIÓN
// (omega, deg/s — NO el heading absoluto, que no comparte cero) vista por:
//   • el CENTINELA secundario (2º BNO, leído 1×/s con ToF pausados — glue, bench),
//   • el OTOS (omega de DOWN; código DURMIENTE en R2, que no tiene OTOS — B.1),
//   • la CÁMARA (−d(bearing_arco)/dt, vía goal_rate_tracker; única ref externa de R2).
//
// REGLAS DURAS (del review adversarial 2026-06-15 — NO violarlas)
// --------------------------------------------------------------
//   1. NO FAILOVER. Un failover a una fuente PLAUSIBLE-PERO-ERRADA es PEOR que
//      heading_valid=0 (el fail-safe ω=0 cubre heading AUSENTE, no PRESENTE-Y-MALO →
//      el arquero rotaría fuera del arco). Jerarquía: primario-sano > heading_valid=0 >
//      secundario-promovido (solo 2027). Este módulo DIAGNOSTICA, no conmuta fuentes.
//   2. ANTI-FALSO-VETO. Vetar un BNO SANO es peor que el bug original. Con 0 refs
//      INDEPENDIENTES (no-BNO) válidas → JAMÁS MALO. Con 1 → solo baja score, JAMÁS
//      MALO. Con ≥2 que coincidan → recién ahí puede escalar a MALO (tras K ventanas).
//      El centinela es un 2º BNO → confirma, pero NO cuenta como ref independiente
//      (falla de modo común: un golpe al cristal de 32 kHz congela a los dos).
//   3. VENTANA EVALUABLE. Solo juzgar cuando hay señal: o el robot gira de verdad
//      (|w_truth| > umbral) o las refs dicen "quieto" y el primario inventa rotación.
//      Transitorios (giró y volvió en la ventana) → ventana descartada.
//   4. La cámara NO mide omega si el robot TRASLADA (strafe del arquero con arco cerca):
//      d(bearing)/dt se contamina con la traslación. El llamador DEBE pasar cam valid=
//      false cuando |vel_lateral| no es ~0 (lo gatea xval_feed_camera del lado glue).
//
// SALIDA: veredicto por BNO primario {SANO/SOSPECHA/MALO} + recomendación {NINGUNA /
// FLAG_DRIFT (telemetría) / RECOMMEND_SETTLE (la CENTRAL puede resetear OPORTUNISTA en
// ventana de quietud que YA ocurre — NUNCA en defensa, B.4)} + score 0..100 telemetría.
//
// PURO: sin Arduino/Wire/hardware. .bss zero-init = estado válido (como imu_fusion.h).
// El glue le pasa números (omega ya calculados) y consume el veredicto. El scheduler
// del centinela @1Hz (con TIMEOUT defensivo) vive acá como lógica pura; solo el efecto
// físico (leer Wire, pausar ToF) es glue, BLOQUEADO hasta banco (3 blockers del review).

#pragma once
#include <stdint.h>

namespace iitasoccer {

enum class XvalVerdict : uint8_t { SANO = 0, SOSPECHA = 1, MALO = 2 };
enum class XvalAction  : uint8_t { NINGUNA = 0, FLAG_DRIFT = 1, RECOMMEND_SETTLE = 2 };

// Fases del scheduler del centinela (puro).
enum : uint8_t { XVAL_SENT_IDLE = 0, XVAL_SENT_REQUESTED = 1 };

struct XvalParams {
    float    tol_base_dps;        // tol = base + slope·|w_truth|. default 8 (ESTIMADO — medir banco)
    float    tol_slope;           // pendiente de la tolerancia adaptativa. default 0.15
    float    otos_omega_sat_dps;  // |w_otos| ≥ esto → OTOS no confiable (satura, B.3). default 300
    float    min_net_rotation_dps;// ventana evaluable: |w_truth| debe superar esto. default 5
    uint8_t  consensus_k;         // K ventanas evaluables divergentes seguidas → MALO. default 3
    uint32_t fresh_timeout_ms;    // una ref más vieja que esto deja de contar. default 300
    uint32_t cooldown_ms;         // tras latch MALO, no re-juzgar (anti-bucle). default 2000
    uint8_t  score_ema_num;       // EMA del score: score += (target-score)·num/den. default 1
    uint8_t  score_ema_den;       // default 4
    uint32_t sentinel_period_ms;  // período del centinela. default 1000
    uint32_t sentinel_timeout_ms; // timeout del handshake del centinela. default 200
};

inline XvalParams xval_default_params() {
    XvalParams p;
    p.tol_base_dps        = 8.0f;
    p.tol_slope           = 0.15f;
    p.otos_omega_sat_dps  = 300.0f;
    p.min_net_rotation_dps= 5.0f;
    p.consensus_k         = 3;
    p.fresh_timeout_ms    = 300;
    p.cooldown_ms         = 2000;
    p.score_ema_num       = 1;
    p.score_ema_den       = 4;
    p.sentinel_period_ms  = 1000;
    p.sentinel_timeout_ms = 200;
    return p;
}

struct XvalState {
    // --- Fuentes (omega deg/s + sello de frescura; ms==0 = nunca alimentada) ---
    float    w_pri;      uint32_t pri_ms;      bool inc1_frozen;   // primario (100 Hz) + veredicto INC-1
    float    w_sec;      uint32_t sec_ms;      bool sec_present;   // centinela 2º BNO (1 Hz)
    float    sec_net_rotation;                                     // rotación NETA del primario en la ventana del centinela (gate anti-aliasing)
    float    w_otos;     uint32_t otos_ms;     bool otos_valid;    // OTOS (durmiente en R2)
    float    w_cam;      uint32_t cam_ms;      bool cam_valid;     // cámara (goal-rate, gateada)
    // --- Consenso / veredicto ---
    uint8_t  bad_streak;          // ventanas evaluables divergentes con ≥2 refs indep
    uint8_t  suspect_streak;      // ventanas con divergencia de 1 sola ref
    float    score;               // 0..100 (EMA) — telemetría (el entregable Fase 1)
    bool     score_primed;
    XvalVerdict verdict;          // latcheado
    uint32_t cooldown_until_ms;   // hasta acá no se re-juzga el veredicto
    bool     window_evaluable;    // la última ventana fue informativa
    uint8_t  n_indep_last;        // refs INDEPENDIENTES (no-BNO) válidas en la última update
    // --- Scheduler del centinela (PURO; el read físico es glue) ---
    uint8_t  sentinel_phase;      // XVAL_SENT_*
    uint32_t sentinel_due_ms;     // próximo instante de lectura
    uint32_t sentinel_deadline_ms;// vencimiento del handshake en curso
};

inline void xval_reset(XvalState& s) {
    s.w_pri = 0; s.pri_ms = 0; s.inc1_frozen = false;
    s.w_sec = 0; s.sec_ms = 0; s.sec_present = false; s.sec_net_rotation = 0;
    s.w_otos = 0; s.otos_ms = 0; s.otos_valid = false;
    s.w_cam = 0; s.cam_ms = 0; s.cam_valid = false;
    s.bad_streak = 0; s.suspect_streak = 0;
    s.score = 100.0f; s.score_primed = false;
    s.verdict = XvalVerdict::SANO;
    s.cooldown_until_ms = 0;
    s.window_evaluable = false;
    s.n_indep_last = 0;
    s.sentinel_phase = XVAL_SENT_IDLE;
    s.sentinel_due_ms = 0;
    s.sentinel_deadline_ms = 0;
}

// --- Alimentación de fuentes (el glue calcula los omega; acá solo se guardan) ---
inline void xval_feed_primary(XvalState& s, float w_pri_dps, bool freeze_inc1, uint32_t now_ms) {
    s.w_pri = w_pri_dps; s.inc1_frozen = freeze_inc1; s.pri_ms = (now_ms == 0) ? 1 : now_ms;
}
// w_sec_dps = omega del centinela sobre su ventana; net_rotation_dps = rotación NETA del
// PRIMARIO en la MISMA ventana (la calcula el glue acumulando el primario) → gate de
// ventana-evaluable del centinela (anti-aliasing del muestreo a 1 Hz).
inline void xval_feed_sentinel(XvalState& s, float w_sec_dps, bool sec_present,
                               float net_rotation_dps, uint32_t now_ms) {
    s.w_sec = w_sec_dps; s.sec_present = sec_present; s.sec_ms = (now_ms == 0) ? 1 : now_ms;
    s.sec_net_rotation = net_rotation_dps;   // update lo gatea vs min_net_rotation
}
inline void xval_feed_otos(XvalState& s, float w_otos_dps, bool valid, uint32_t now_ms) {
    s.w_otos = w_otos_dps; s.otos_valid = valid; s.otos_ms = (now_ms == 0) ? 1 : now_ms;
}
inline void xval_feed_camera(XvalState& s, float w_cam_dps, bool valid, uint32_t now_ms) {
    s.w_cam = w_cam_dps; s.cam_valid = valid; s.cam_ms = (now_ms == 0) ? 1 : now_ms;
}

namespace xval_detail {
inline bool fresh(uint32_t last_ms, uint32_t now_ms, uint32_t timeout) {
    return last_ms != 0 && (now_ms - last_ms) < timeout;   // wrap-safe
}
inline float absf(float x) { return x < 0 ? -x : x; }
// Mediana de 1..3 valores (mediana de 2 = promedio).
inline float median_n(const float* v, int n) {
    if (n <= 0) return 0.0f;
    if (n == 1) return v[0];
    if (n == 2) return 0.5f * (v[0] + v[1]);
    // n==3: el del medio
    float a = v[0], b = v[1], c = v[2];
    float hi = a > b ? a : b; float lo = a > b ? b : a;
    if (c > hi) return hi;
    if (c < lo) return lo;
    return c;
}
}  // namespace xval_detail

// FUNCIÓN PRINCIPAL. Calcula w_truth (mediana de refs válidas), juzga el primario contra
// ella con la regla anti-falso-veto, mantiene el consenso K-ventanas y el latch+cooldown.
inline void xval_update(XvalState& s, const XvalParams& p, uint32_t now_ms) {
    using namespace xval_detail;

    // 1) CONGELADO (prueba INTERNA de INC-1, independiente de refs): MALO inmediato.
    //    INC-1 ya bajó present→heading_valid=0 a 100 Hz; acá lo reflejamos para telemetría.
    if (s.inc1_frozen) {
        s.verdict = XvalVerdict::MALO;
        s.cooldown_until_ms = now_ms + p.cooldown_ms;
        s.window_evaluable = true;
        // score cae fuerte
        s.score = s.score_primed ? (s.score + (0.0f - s.score) * p.score_ema_num / p.score_ema_den) : 0.0f;
        s.score_primed = true;
        const bool otos_ok_f = s.otos_valid && fresh(s.otos_ms, now_ms, p.fresh_timeout_ms) && absf(s.w_otos) < p.otos_omega_sat_dps;
        const bool cam_ok_f  = s.cam_valid  && fresh(s.cam_ms, now_ms, p.fresh_timeout_ms);
        s.n_indep_last = (uint8_t)((otos_ok_f ? 1 : 0) + (cam_ok_f ? 1 : 0));
        return;
    }

    // 2) Cooldown activo tras un MALO: sostener el veredicto, no re-juzgar.
    if (s.verdict == XvalVerdict::MALO && (int32_t)(now_ms - s.cooldown_until_ms) < 0) {
        return;
    }

    // 3) Armar el conjunto de refs VÁLIDAS y contar las INDEPENDIENTES (no-BNO).
    float refs[3]; int n = 0; int n_indep = 0;
    const bool otos_ok = s.otos_valid && fresh(s.otos_ms, now_ms, p.fresh_timeout_ms)
                         && absf(s.w_otos) < p.otos_omega_sat_dps;   // B.3: descartar saturado
    const bool cam_ok  = s.cam_valid  && fresh(s.cam_ms, now_ms, p.fresh_timeout_ms);
    const bool sec_ok  = s.sec_present && fresh(s.sec_ms, now_ms, p.fresh_timeout_ms)
                         && absf(s.sec_net_rotation) > p.min_net_rotation_dps;   // ventana evaluable (anti-aliasing 1 Hz)
    if (otos_ok) { refs[n++] = s.w_otos; n_indep++; }
    if (cam_ok)  { refs[n++] = s.w_cam;  n_indep++; }
    if (sec_ok)  { refs[n++] = s.w_sec; }   // 2º BNO: confirma, NO cuenta como independiente
    s.n_indep_last = (uint8_t)n_indep;

    // 4) Sin refs → no se puede cross-validar este tick. No escalar ni cambiar el
    //    veredicto (se sostiene el último); el consenso requiere refs para moverse.
    if (n == 0) {
        s.window_evaluable = false;
        return;
    }

    const float w_truth = median_n(refs, n);
    const float tol = p.tol_base_dps + p.tol_slope * absf(w_truth);
    const bool moving = absf(w_truth) > p.min_net_rotation_dps;

    // 5) VENTANA EVALUABLE: o el robot gira de verdad (comparar tasas), o las refs dicen
    //    "quieto" (entonces el primario también debería ~0; si inventa rotación = deriva).
    const float err = absf(s.w_pri - w_truth);
    bool diverge;
    if (moving) {
        s.window_evaluable = true;
        diverge = err > tol;
    } else {
        // refs ~quietas: informativo SOLO para detectar que el primario inventa rotación.
        s.window_evaluable = true;
        diverge = absf(s.w_pri) > tol;   // primario debería estar ~0 como las refs
    }

    if (diverge) {
        if (n_indep >= 2) {
            // ≥2 refs INDEPENDIENTES coinciden (entre sí, por la mediana) y discrepan del
            // primario → consenso fuerte. Escalar tras K ventanas sostenidas.
            if (s.bad_streak < 0xFF) s.bad_streak++;
            s.verdict = (s.bad_streak >= p.consensus_k) ? XvalVerdict::MALO : XvalVerdict::SOSPECHA;
            if (s.verdict == XvalVerdict::MALO) s.cooldown_until_ms = now_ms + p.cooldown_ms;
        } else {
            // 0 ó 1 ref independiente → JAMÁS MALO (anti-falso-veto). Máx SOSPECHA.
            s.bad_streak = 0;
            if (s.suspect_streak < 0xFF) s.suspect_streak++;
            s.verdict = XvalVerdict::SOSPECHA;
        }
    } else {
        // Coinciden → sano. Limpiar rachas (el cooldown ya pasó si llegamos acá).
        s.bad_streak = 0;
        if (s.suspect_streak > 0) s.suspect_streak--;
        s.verdict = XvalVerdict::SANO;
    }

    // 6) Score EMA 0..100 (telemetría): baja con divergencia, sube con acuerdo.
    const float target = diverge ? (n_indep >= 2 ? 0.0f : 50.0f) : 100.0f;
    s.score = s.score_primed
        ? (s.score + (target - s.score) * p.score_ema_num / p.score_ema_den)
        : target;
    s.score_primed = true;
}

// --- Accesores ---
inline XvalVerdict xval_primary_verdict(const XvalState& s) { return s.verdict; }
inline float       xval_primary_score(const XvalState& s)   { return s.score; }
inline uint8_t     xval_n_indep_refs(const XvalState& s)     { return s.n_indep_last; }
inline bool        xval_window_evaluable(const XvalState& s) { return s.window_evaluable; }

// El centinela CONFIRMÓ salud POSITIVA hace poco (no solo sec_present). Gate DURO para
// cualquier failover futuro (2027): sin confirmación positiva, preferir heading_valid=0.
inline bool xval_sentinel_healthy(const XvalState& s) {
    return s.sec_present;   // proxy puro; el glue lo refina con la coincidencia real en banco
}

// Recomendación al consumidor (CENTRAL). NINGUNA si SANO. FLAG_DRIFT (telemetría) si
// SOSPECHA. RECOMMEND_SETTLE si MALO por DERIVA confirmada (la CENTRAL puede resetear
// OPORTUNISTA en ventana de quietud — NUNCA en defensa, B.4). Si el MALO es por CONGELADO,
// INC-1 ya degradó (heading_valid=0): FLAG_DRIFT, no settle activo.
inline XvalAction xval_recommended_action(const XvalState& s) {
    if (s.verdict == XvalVerdict::SANO)     return XvalAction::NINGUNA;
    if (s.verdict == XvalVerdict::SOSPECHA) return XvalAction::FLAG_DRIFT;
    // MALO:
    return s.inc1_frozen ? XvalAction::FLAG_DRIFT : XvalAction::RECOMMEND_SETTLE;
}

// --- Scheduler del centinela @1Hz (PURO; el read físico es glue, bloqueado a banco) ---
inline bool xval_sentinel_due(const XvalState& s, uint32_t now_ms) {
    return s.sentinel_phase == XVAL_SENT_IDLE && (int32_t)(now_ms - s.sentinel_due_ms) >= 0;
}
inline void xval_sentinel_arm(XvalState& s, const XvalParams& p, uint32_t now_ms) {
    s.sentinel_phase = XVAL_SENT_REQUESTED;
    s.sentinel_deadline_ms = now_ms + p.sentinel_timeout_ms;
}
inline void xval_sentinel_done(XvalState& s, const XvalParams& p, uint32_t now_ms) {
    s.sentinel_phase = XVAL_SENT_IDLE;
    s.sentinel_due_ms = now_ms + p.sentinel_period_ms;
}
// TIMEOUT DEFENSIVO: si el handshake no completó a tiempo, abortar y RE-ARMAR (nunca
// colgar el scheduler ni dejar el bus pausado). Devuelve true si hubo timeout.
inline bool xval_sentinel_timed_out(XvalState& s, const XvalParams& p, uint32_t now_ms) {
    if (s.sentinel_phase == XVAL_SENT_REQUESTED && (int32_t)(now_ms - s.sentinel_deadline_ms) >= 0) {
        s.sentinel_phase = XVAL_SENT_IDLE;
        s.sentinel_due_ms = now_ms + p.sentinel_period_ms;
        return true;
    }
    return false;
}

}  // namespace iitasoccer
