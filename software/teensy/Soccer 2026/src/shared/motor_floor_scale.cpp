// motor_floor_scale.cpp — implementación PURA (ver motor_floor_scale.h).

#include "motor_floor_scale.h"

namespace iitasoccer {

void motor_floor_scale(int pwm[3], const FloorScaleCfg& cfg) {
    // 1) Eficiencia por rueda: PWM necesario = crudo / eficiencia.
    //    (La trasera rinde más velocidad por PWM → necesita MENOS PWM → /1.31.)
    float w[3];
    for (int i = 0; i < 3; ++i) {
        const int e = (cfg.eff_x100[i] > 0) ? cfg.eff_x100[i] : 100;
        w[i] = static_cast<float>(pwm[i]) * 100.0f / static_cast<float>(e);
    }

    // 2) Cero-o-fiel: componentes ínfimas (≤ ruido) quedan en 0 y NO participan
    //    del escalado (jamás se las dispara al piso → sin bang-bang).
    bool active[3];
    for (int i = 0; i < 3; ++i) {
        const float mag = (w[i] < 0.0f) ? -w[i] : w[i];
        active[i] = mag > static_cast<float>(cfg.noise_thresh);
        if (!active[i]) w[i] = 0.0f;
    }

    // 3) Factor de escalado UNIFORME — definido SOLO por las ruedas PRINCIPALES
    //    (≥35% de la mayor). ⚠️ Banco 2026-06-09: si una componente AUXILIAR chica
    //    (ej. el aporte de ω en la trasera durante el avance: ~14 PWM vs piso 107)
    //    participaba, exigía k≈8 → el cap térmico recortaba → LATIGAZOS a máxima
    //    potencia girando "a lo loco". Las auxiliares no mandan: si tras el escalado
    //    no llegan a su piso, van a 0 (su aporte direccional lo llevan las
    //    principales, que conservan proporciones exactas).
    float maxmag = 0.0f;
    for (int i = 0; i < 3; ++i) {
        const float mag = (w[i] < 0.0f) ? -w[i] : w[i];
        if (active[i] && mag > maxmag) maxmag = mag;
    }
    const float part_min = 0.35f * maxmag;
    float k = 1.0f;
    for (int i = 0; i < 3; ++i) {
        if (!active[i]) continue;
        const float mag = (w[i] < 0.0f) ? -w[i] : w[i];
        if (mag < part_min) continue;   // auxiliar: no define el escalado
        const float need = static_cast<float>(cfg.floor_pwm[i]) / mag;
        if (need > k) k = need;
    }

    // 4) Tope térmico por escalado (proporcional, no clamp por rueda): si el k
    //    necesario manda alguna rueda sobre burn_cap, se reduce k para que la
    //    mayor quede EXACTA en el cap (la dirección se sigue conservando).
    if (cfg.burn_cap > 0) {
        float maxmag = 0.0f;
        for (int i = 0; i < 3; ++i) {
            const float mag = ((w[i] < 0.0f) ? -w[i] : w[i]) * k;
            if (mag > maxmag) maxmag = mag;
        }
        if (maxmag > static_cast<float>(cfg.burn_cap)) {
            k *= static_cast<float>(cfg.burn_cap) / maxmag;
        }
    }

    // 5) Aplicar; si el cap obligó a bajar y una rueda quedó bajo su piso, va a 0
    //    (bajo el piso no gira de todas formas — mejor silencio explícito que
    //    un PWM que solo calienta).
    for (int i = 0; i < 3; ++i) {
        if (!active[i]) { pwm[i] = 0; continue; }
        float v = w[i] * k;
        const float mag = (v < 0.0f) ? -v : v;
        if (mag + 0.5f < static_cast<float>(cfg.floor_pwm[i])) {
            pwm[i] = 0;
            continue;
        }
        pwm[i] = static_cast<int>((v >= 0.0f) ? (v + 0.5f) : (v - 0.5f));
    }
}

}  // namespace iitasoccer
