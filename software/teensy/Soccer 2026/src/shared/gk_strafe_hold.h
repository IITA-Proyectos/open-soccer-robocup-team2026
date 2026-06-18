#pragma once
// gk_strafe_hold.h — control de rumbo del ARQUERO durante el strafe CONTINUO, por
// modulación de la rueda TRASERA con PEQUEÑAS correcciones (idea Gustavo 2026-06-17).
//
// POR QUÉ la rueda trasera y no ω: en este omni a baja velocidad (régimen cuantizado)
// corregir el rumbo con ω desborda las ruedas DELANTERAS, que están pegadas a su piso
// (70) → bandazo (medido en banco 2026-06-12). La rueda TRASERA, en cambio, trabaja
// con holgura sobre su piso (107 → ~150) durante el strafe, así que PEQUEÑAS
// modulaciones de SU potencia se quedan en zona proporcional y dan una corrección
// suave de rumbo sin tocar las delanteras.
//
// Este módulo es PURO (host-testeable): toma el error de rumbo y devuelve el trim de
// PWM para la trasera. NO sabe de hardware ni del signo físico (ese signo lo aplica el
// actuador, GK_REAR_TRIM_SIGN, porque hay que confirmarlo en banco). El trim es
// PROPORCIONAL al error: trim>0 cuando error>0 (target − heading > 0).
//
// Controlador PI con zona muerta + anti-windup:
//  - El término I cancela la DERIVA PARÁSITA del strafe (~80°/s, sistemática) que un P
//    puro no puede (la planta lo dice: ver skill dinamica-omni-3-ruedas).
//  - Zona muerta: |error| < deadband → trim 0, para no pelear el ruido del heading.
//  - Fuera de banda: |error| > band → out_of_band=true; el caller dispara el IMPULSO
//    fuerte (la corrección suave por trim ya no alcanza). El impulso lo hace el caller.

#include <stdint.h>

namespace iitasoccer {

struct GkStrafeHoldCfg {
    float kp;            // PWM por grado de error
    float ki;            // PWM por (grado·segundo) — cancela la deriva sistemática
    float deadband_deg;  // |error| < deadband → trim 0 (no pelear el ruido)
    float band_deg;      // |error| > band → out_of_band (el caller dispara el impulso)
    int   trim_max_pwm;  // |trim| ≤ esto (pequeña modulación; NO romper el régimen)
    float i_max_pwm;     // anti-windup: |término I| ≤ esto
};

struct GkStrafeHoldState {
    float integ;         // acumulador del término integral (en PWM)
};

struct GkStrafeHoldOut {
    int  trim_pwm;       // corrección a sumar a la trasera (signo = signo del error)
    bool out_of_band;    // true → el rumbo se fue de la zona de control; disparar impulso
};

inline void gk_strafe_hold_reset(GkStrafeHoldState& st) { st.integ = 0.0f; }

// Un paso del lazo. error_deg = target − heading (grados, ya normalizado a [-180,180]).
// dt_s = tiempo desde el paso anterior (segundos).
inline GkStrafeHoldOut gk_strafe_hold_step(GkStrafeHoldState& st,
                                           const GkStrafeHoldCfg& cfg,
                                           float error_deg, float dt_s) {
    GkStrafeHoldOut out{0, false};
    const float aerr = (error_deg < 0.0f) ? -error_deg : error_deg;

    out.out_of_band = (aerr > cfg.band_deg);

    // Zona muerta: dentro de ella no corregimos ni integramos (anti-creep por ruido).
    if (aerr < cfg.deadband_deg) {
        out.trim_pwm = 0;
        return out;
    }

    // Integro (anti-windup por clamp). dt_s≤0 → no integra (defensivo).
    if (dt_s > 0.0f) st.integ += cfg.ki * error_deg * dt_s;
    if (st.integ >  cfg.i_max_pwm) st.integ =  cfg.i_max_pwm;
    if (st.integ < -cfg.i_max_pwm) st.integ = -cfg.i_max_pwm;

    float trim = cfg.kp * error_deg + st.integ;
    if (trim >  (float)cfg.trim_max_pwm) trim =  (float)cfg.trim_max_pwm;
    if (trim < -(float)cfg.trim_max_pwm) trim = -(float)cfg.trim_max_pwm;

    out.trim_pwm = (int)(trim >= 0.0f ? trim + 0.5f : trim - 0.5f);  // redondeo simétrico
    return out;
}

}  // namespace iitasoccer
