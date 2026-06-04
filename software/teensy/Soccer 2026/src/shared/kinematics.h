// kinematics.h — Cinemática inversa para robot omni de 3 ruedas
//
// Convención del frame del robot:
//   - +X: derecha del robot.
//   - +Y: frente del robot.
//   - omega: positivo = CCW visto desde arriba.
//
// Para cada rueda i con ángulo físico theta_i (medido desde +X CCW) y a distancia
// R desde el centro, la velocidad lineal que debe entregar la rueda para producir
// la velocidad deseada (vx, vy, omega) del robot es:
//
//     v_i = -vx * sin(theta_i) + vy * cos(theta_i) + omega * R
//
// Esta library es PURA (sin Arduino, sin hardware). Se compila en host y se testea
// con `pio test -e test_native`.

#pragma once

namespace iitasoccer {

struct WheelConfig {
    float angle_rad;   // posición angular de la rueda desde +X del robot (CCW)
    float radius_mm;   // distancia del centro del robot al centro de la rueda
};

struct WheelSpeeds {
    float wheel[3];    // velocidades en mm/s; signo + = CCW de la rueda
};

// Cinemática inversa: dado (vx, vy, omega) deseados, calcula velocidades de cada rueda.
//   vx_mm_s    : velocidad lateral (positivo = a la derecha del robot)
//   vy_mm_s    : velocidad longitudinal (positivo = al frente del robot)
//   omega_rad_s: velocidad angular (positivo = CCW visto desde arriba)
//   wheels     : array de 3 WheelConfig con la posición de cada rueda
WheelSpeeds inverse_kinematics(float vx_mm_s, float vy_mm_s, float omega_rad_s,
                                const WheelConfig wheels[3]);

// Convierte velocidad de rueda (mm/s) a PWM signed en rango [-max_pwm, +max_pwm].
// Si speed_mm_s excede ±max_speed_mm_s, satura al PWM máximo.
int wheel_speed_to_pwm(float speed_mm_s, float max_speed_mm_s, int max_pwm);

// Saturación PROPORCIONAL: si alguna rueda excede max_speed_mm_s, escala
// las 3 velocidades por el mismo factor para mantener la dirección del
// vector de movimiento pero a la velocidad máxima alcanzable.
// Diferencia importante con la saturación por-rueda: ésta preserva la trayectoria.
void saturate_wheels(WheelSpeeds& ws, float max_speed_mm_s);

// Piso de PWM (deadzone compensation) — porta el concepto del IMPULSO_INICIAL del
// delantero 2025: a vx/vy bajos el PWM de rueda cae en la zona muerta del motor y
// éste raspa/se queda stalled en vez de girar. Esta función eleva todo PWM no-nulo
// por debajo de `min_pwm` hasta `min_pwm` (conservando el signo), de forma que el
// motor reciba siempre suficiente para arrancar.
//
//   - min_pwm <= 0       → GATE OFF: devuelve pwm SIN cambios (no-op).
//   - |pwm| <= noise_thresh → 0 (ruido / comando despreciable, no mueve nada).
//   - 0 < |pwm| < min_pwm   → min_pwm * sign(pwm) (eleva al piso, conserva signo).
//   - en otro caso          → pwm (intacto).
//
// DEFAULT en config_central.h: min_pwm=0, noise_thresh=0 ⇒ no-op ⇒ el binario de
// competencia es IDÉNTICO. El equipo sube MOTOR_MIN_PWM en banco (típico 25-45).
int apply_pwm_floor(int pwm, int min_pwm, int noise_thresh);

}  // namespace iitasoccer
