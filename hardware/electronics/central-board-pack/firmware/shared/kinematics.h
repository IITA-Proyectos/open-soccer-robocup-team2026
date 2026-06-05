// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
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

}  // namespace iitasoccer
