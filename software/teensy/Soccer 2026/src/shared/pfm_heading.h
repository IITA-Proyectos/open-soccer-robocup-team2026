// pfm_heading.h — Control de rumbo PI + PFM (duty-cycling) para actuador con
// zona muerta (banco María 2026-06-12, diseño según skills control-pid-zona-muerta
// + dinamica-omni-3-ruedas).
//
// EL PROBLEMA: en el régimen cuantizado (<~420 mm/s) este robot NO sabe girar
// despacio — el giro es casi todo-o-nada por los pisos de PWM. Un PID continuo
// clásico falla por los DOS modos medidos hoy: capado bajo pierde contra la
// deriva parásita (~80°/s) y capado alto oscila violento (overshoot cuantizado).
//
// LA SOLUCIÓN (control industrial estándar para actuadores bang-bang):
//   • PFM (pulse-frequency/duty modulation): el lazo calcula la corrección
//     DESEADA u (°/s promedio) y la entrega como pulsos de magnitud FIJA
//     omega_on (que el actuador SÍ sabe hacer) durante una fracción
//     duty = u/omega_on de cada ventana corta. El promedio temporal ≈ u →
//     corrección fina EFECTIVA con actuador grosero.
//   • Zona muerta del error (deadband): |err| chico → salida 0 (no perseguir
//     ruido; sin temblequeo alrededor del setpoint).
//   • PI con anti-windup: el integrador aprende SOLO la perturbación
//     sistemática (la deriva parásita del strafe) = feedforward automático =
//     la "auto-calibración" pedida. Clampeado a lo que tiene sentido cancelar.
//
// PURO (sin Arduino) → host-testeable: bash scripts/run-host-tests.sh test_pfm_heading
// El caller (strategy) calcula el error envuelto ±180 y convierte la salida a
// centideg/s. Convención: err > 0 ⇒ corregir CCW+ (omega positivo).

#pragma once
#include <stdint.h>

namespace iitasoccer {

struct PfmHeadingCfg {
    float    kp;              // (°/s de corrección) por (° de error)
    float    ki;              // (°/s) por (°·s) — integrador/feedforward automático
    float    deadband_deg;    // |err| menor → salida 0 (anti-temblequeo)
    float    omega_on_degps;  // magnitud FIJA del pulso ON (lo que el robot sí hace)
    float    integ_max_degps; // anti-windup: tope del integrador (≈ perturbación máx)
    uint32_t window_ms;       // ventana del duty-cycling (corta: 120-200 ms)
};

// Punto de partida de banco (titración en la skill control-pid-zona-muerta):
// kp=2 (suave: el grueso lo pone el integrador), ki=0.4, deadband 5°,
// omega_on=100°/s (sobre el mínimo útil con FLOOR_SCALE), integ ±100 (cubre los
// ~80°/s de deriva parásita medida), ventana 160 ms (~16 ticks a 100 Hz).
inline PfmHeadingCfg pfm_heading_default_cfg() {
    return PfmHeadingCfg{2.0f, 0.4f, 5.0f, 100.0f, 100.0f, 80};
}

struct PfmHeadingState {
    float    integ_degps;   // feedforward aprendido
    uint32_t win_t0_ms;     // inicio de la ventana actual (0 = sin iniciar)
};

inline void pfm_heading_reset(PfmHeadingState& s) {
    s.integ_degps = 0.0f;
    s.win_t0_ms   = 0;
}

// Un tick del lazo. err_deg = error de rumbo YA envuelto a ±180 (target-actual,
// >0 ⇒ falta CCW). dt_s = período del tick (ej. 0.01 a 100 Hz). Devuelve el
// omega a comandar ESTE tick en °/s: ±omega_on durante la fracción ON de la
// ventana, 0 el resto. Sin err válido devuelve 0 y congela el integrador.
inline float pfm_heading_tick(PfmHeadingState& s, const PfmHeadingCfg& cfg,
                              float err_deg, bool err_valid,
                              uint32_t now_ms, float dt_s) {
    if (!err_valid) return 0.0f;            // sin gyro: ni corregir ni aprender

    const float aerr = (err_deg < 0.0f) ? -err_deg : err_deg;
    if (aerr < cfg.deadband_deg) {
        return 0.0f;                        // zona muerta: integrador se mantiene
    }

    // PI: corrección deseada (promedio temporal objetivo).
    float u = cfg.kp * err_deg + s.integ_degps;

    // Duty de la ventana: qué fracción del tiempo va el pulso ON.
    float duty = u / cfg.omega_on_degps;
    if (duty >  1.0f) duty =  1.0f;
    if (duty < -1.0f) duty = -1.0f;

    // Anti-windup condicional: solo integrar si la salida NO está saturada
    // (si duty ya es ±1, acumular más solo carga un latigazo futuro).
    if (duty > -1.0f && duty < 1.0f) {
        s.integ_degps += cfg.ki * err_deg * dt_s;
        if (s.integ_degps >  cfg.integ_max_degps) s.integ_degps =  cfg.integ_max_degps;
        if (s.integ_degps < -cfg.integ_max_degps) s.integ_degps = -cfg.integ_max_degps;
    }

    // Ventana de duty-cycling (wrap-safe con resta unsigned).
    if (s.win_t0_ms == 0 || (now_ms - s.win_t0_ms) >= cfg.window_ms) {
        s.win_t0_ms = (now_ms == 0) ? 1 : now_ms;   // 0 = sentinel "sin iniciar"
    }
    const uint32_t elapsed = now_ms - s.win_t0_ms;
    const float aduty = (duty < 0.0f) ? -duty : duty;
    const bool on = (static_cast<float>(elapsed) < aduty * static_cast<float>(cfg.window_ms));

    if (!on) return 0.0f;
    return (duty >= 0.0f) ? cfg.omega_on_degps : -cfg.omega_on_degps;
}

}  // namespace iitasoccer
