// goal_rate_tracker.h — Tasa de rotación inferida del BEARING al arco (PURO, host-test).
//
// Por qué existe (TASK-212 Fase 1 + diseño centinela 2026-06-15)
// -------------------------------------------------------------
// Para cross-validar la salud del BNO con una referencia FÍSICAMENTE INDEPENDIENTE,
// la cámara aporta: si el robot gira +omega, el arco visible "rota" −omega en el marco
// del robot. Entonces w_cam ≈ −d(bearing_arco)/dt es una estimación de la tasa de giro.
//
// ⚠️ POR QUÉ **NO** ES "espejo de ball_velocity" (lo pidió el review adversarial):
//   • ball_velocity deriva velocidad CARTESIANA (x,y) — sin discontinuidad.
//   • el bearing al arco es ANGULAR y CRUZA ±180°: restar lineal da un salto falso de
//     360°/s en el cruce (+179°→−179° es +2°, NO −358°). Hay que restar con ENVOLTURA
//     (norm180). Además un salto físicamente imposible (|w|>max_jump) = glitch o cambio
//     de arco → se descarta. Y exige visibilidad CONTINUA del arco (≥2 frames), porque
//     es la ÚNICA referencia externa de R2 (que no tiene OTOS): no puede dar picos.
//
// ⚠️ LO QUE **NO** RESUELVE (lo gatea el llamador — BLOCKER del review):
//   Si el robot TRASLADA (el arquero hace STRAFE) con el arco CERCA, d(bearing)/dt está
//   dominado por la TRASLACIÓN, no por la rotación → w_cam es basura como referencia de
//   omega. Este módulo solo da la tasa angular + validez por visibilidad/salto/dt; el
//   llamador (xval_feed_camera) DEBE invalidarla cuando |vel_lateral| no es ~0.
//
// CONVENCIÓN: bearing en centideg (igual que goal_opp_angle_centideg del WorldSnapshot),
// rango [-18000,18000]. Salida w_cam en deg/s, CCW+ (misma que imu_fusion/gyro_z).
// PURO: sin Arduino. .bss zero-init = estado inicial válido (como ball_velocity.h).

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Diferencia angular ENVUELTA en centideg → [-18000,18000]. Maneja el cruce ±180°.
inline int32_t grt_wrap_centideg(int32_t d) {
    while (d >   18000) d -= 36000;
    while (d <= -18000) d += 36000;
    return d;
}

struct GoalRateParams {
    uint16_t max_jump_dps;        // |w_cam| mayor a esto = glitch/cambio de arco → descartar
    uint16_t min_dt_ms;           // dt menor a esto = no confiable (división ruidosa)
    uint16_t max_dt_ms;           // dt mayor a esto = muestra vieja → re-siembra, no deriva
    uint8_t  min_visible_streak;  // frames visibles CONTINUOS para dar valid=true
};

inline GoalRateParams goal_rate_default_params() {
    // max_jump 720°/s = 2 vueltas/s: por encima es glitch o salto de arco, no giro real.
    // min_dt 5 ms / max_dt 200 ms: ventana razonable a 30 Hz de cámara (~33 ms/frame).
    // min_visible_streak 2: hace falta un par de frames del MISMO arco para una tasa.
    return GoalRateParams{720, 5, 200, 2};
}

struct GoalRateTracker {
    bool     seen;                // ¿hay una muestra previa sembrada?
    int16_t  last_bearing_cdeg;   // bearing del frame anterior (centideg)
    uint32_t last_ms;             // timestamp del frame anterior
    uint8_t  visible_streak;      // frames visibles consecutivos (satura)
};

inline void goal_rate_reset(GoalRateTracker& t) {
    t.seen = false;
    t.last_bearing_cdeg = 0;
    t.last_ms = 0;
    t.visible_streak = 0;
}

struct GoalRateResult {
    float rate_dps;   // w_cam = −Δbearing/Δt (CCW+). 0 si !valid.
    bool  valid;      // visibilidad continua ≥N + salto físico OK + dt en rango
};

// Alimenta un frame. visible = la cámara ve ESTE arco en este frame. now_ms = reloj.
//
// Re-siembra (sin emitir tasa) cuando: no se ve el arco, primera muestra, dt fuera de
// rango (gap), o salto imposible (glitch/cambio de arco). Solo emite valid=true con
// visibilidad continua ≥ min_visible_streak y un Δbearing físicamente plausible.
inline GoalRateResult goal_rate_update(GoalRateTracker& t, int16_t bearing_cdeg,
                                       bool visible, uint32_t now_ms,
                                       const GoalRateParams& p) {
    GoalRateResult out{0.0f, false};

    if (!visible) {
        // Perdimos el arco: cortar la racha (la próxima visibilidad re-siembra).
        t.visible_streak = 0;
        t.seen = false;
        return out;
    }

    if (!t.seen) {
        // Primer frame visible: siembra la referencia, todavía sin tasa.
        t.seen = true;
        t.last_bearing_cdeg = bearing_cdeg;
        t.last_ms = now_ms;
        t.visible_streak = 1;
        return out;
    }

    const uint32_t dt_ms = now_ms - t.last_ms;   // wrap-safe (unsigned)
    if (dt_ms < p.min_dt_ms || dt_ms > p.max_dt_ms) {
        // dt no confiable o gap largo → re-sembrar como si fuera nueva adquisición.
        t.last_bearing_cdeg = bearing_cdeg;
        t.last_ms = now_ms;
        t.visible_streak = 1;
        return out;
    }

    const int32_t dcdeg = grt_wrap_centideg((int32_t)bearing_cdeg - (int32_t)t.last_bearing_cdeg);
    const float ddeg = dcdeg / 100.0f;
    const float dt_s = dt_ms / 1000.0f;
    // w_cam = −Δbearing/Δt: el arco "retrocede" cuando el robot avanza en CCW+.
    const float rate = -ddeg / dt_s;

    // Avanzar la referencia SIEMPRE (incluso si descartamos por salto: el frame es real).
    t.last_bearing_cdeg = bearing_cdeg;
    t.last_ms = now_ms;
    if (t.visible_streak < 0xFF) t.visible_streak++;

    const float mag = (rate >= 0.0f) ? rate : -rate;
    if (mag > (float)p.max_jump_dps) {
        // Salto físicamente imposible: glitch de detección o cambio de arco. Descartar
        // la tasa y reiniciar la racha (no confiar hasta re-confirmar continuidad).
        t.visible_streak = 1;
        return out;
    }

    out.rate_dps = rate;
    out.valid = (t.visible_streak >= p.min_visible_streak);
    return out;
}

}  // namespace iitasoccer
