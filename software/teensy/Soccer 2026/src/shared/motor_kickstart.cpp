// motor_kickstart.cpp — implementación PURA del impulso inicial (ver motor_kickstart.h).

#include "motor_kickstart.h"

namespace iitasoccer {

namespace {

// Clamp sign-preserving de |v| a cap_abs SIN calcular abs(v) (evita el UB de
// -INT_MIN). Igual patrón que motor_power_cap.h.
int clamp_abs(int v, int cap_abs) {
    if (v >  cap_abs) return  cap_abs;
    if (v < -cap_abs) return -cap_abs;
    return v;
}

}  // namespace

int motor_kickstart_pwm(int pwm_base, int ms_since_start,
                        int window_ms, int factor_x10, int cap_abs) {
    // Rueda parada: no se inventa arranque.
    if (pwm_base == 0) {
        return 0;
    }
    // GATE OFF: ventana no configurada o factor <= 1.0 → no-op (binario idéntico).
    if (window_ms <= 0 || factor_x10 <= MOTOR_KICKSTART_FACTOR_DEN) {
        return pwm_base;
    }
    // Fuera de la ventana del impulso → PWM de régimen sin tocar.
    // (ms negativos = reloj raro: se tratan como "fuera"/seguro → base intacto.)
    if (ms_since_start < 0 || ms_since_start >= window_ms) {
        return pwm_base;
    }

    // Cap de seguridad: <=0 → usar el default del módulo (153).
    const int cap = (cap_abs > 0) ? cap_abs : MOTOR_KICKSTART_DEFAULT_CAP;

    // Boost = base * factor. long long = 64-bit en todas las plataformas (en Windows/MinGW
    // `long` es 32-bit y base*factor cerca de INT_MIN desbordaría); así no hay UB de overflow.
    const long long boosted = static_cast<long long>(pwm_base)
                            * static_cast<long long>(factor_x10)
                            / MOTOR_KICKSTART_FACTOR_DEN;

    int out = static_cast<int>(boosted);

    // Defensivo: el boost no debe quedar por DEBAJO (en magnitud) del base.
    // Con factor_x10 >= 10 esto no pasa, pero protege ante factores raros / overflow del cast.
    if (pwm_base > 0 && out < pwm_base) out = pwm_base;
    if (pwm_base < 0 && out > pwm_base) out = pwm_base;

    // Cap DURO de seguridad (anti-quemado), conservando el signo.
    return clamp_abs(out, cap);
}

}  // namespace iitasoccer
