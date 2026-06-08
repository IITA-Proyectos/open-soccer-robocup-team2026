#include "motor_brake.h"

namespace iitasoccer {

namespace {
// Cap sign-preserving sin UB (mismo criterio que motor_power_cap.h): no calcula
// -INT_MIN; compara por valor. cap_abs<=0 → passthrough.
int cap_abs_signed(int v, int cap_abs) {
    if (cap_abs <= 0) return v;
    if (v >  cap_abs) return  cap_abs;
    if (v < -cap_abs) return -cap_abs;
    return v;
}
}  // namespace

int plug_brake_compute(const PlugBrakeIn& in, int window_ms, int cap_abs) {
    // Sin movimiento previo → nada que frenar (coast). Evita "patear" reversa
    // desde el reposo (la rueda no tenía inercia que disipar).
    if (in.last_wheel_pwm == 0) return 0;
    // Ventana cerrada o ya vencida → coast. (window_ms<=0 deshabilita el plug.)
    if (window_ms <= 0 || in.ms_since_stop < 0 || in.ms_since_stop >= window_ms) {
        return 0;
    }
    // Magnitud de reversa: cruda capeada a cap_abs (seguridad del motor).
    // ⚠️ Con cap_abs=150 y crudas 275/187, AMBAS ruedas salen a 150 → la
    // asimetría 275/187 (que compensaba mecánica entre delanteras) se PIERDE.
    int mag = in.plug_pwm_raw;
    if (mag < 0) mag = -mag;                 // magnitud (defensivo)
    mag = cap_abs_signed(mag, cap_abs);      // capea a +cap (mag ya es >=0)
    // Signo OPUESTO al último comando = reversa (plugging).
    return (in.last_wheel_pwm > 0) ? -mag : mag;
}

int rear_brake_lead_ms(bool is_parallel, int vel_pct, int base_ms, int max_ms) {
    if (!is_parallel) return 0;              // solo la trasera paralela al strafe
    int v = vel_pct;
    if (v < 0) v = -v;                       // magnitud de velocidad
    long lead = static_cast<long>(base_ms) * v / 100;  // escala lineal x VEL/100
    if (lead < 0) lead = 0;
    if (max_ms > 0 && lead > max_ms) lead = max_ms;     // clamp superior
    return static_cast<int>(lead);
}

}  // namespace iitasoccer
