// atk_nogyro.h — helpers PUROS del delantero SIN GIROSCOPIO (práctica R1, 2026-06-12).
//
// Contexto: ROBOT1 corre sin BNO (los 2 quedaron desconectados tras el recableado
// del 2026-06-11) pero SÍ tiene odometría de piso (2 OTOS en DOWN, broadcast
// Capa 1 ya mergeado). Estos helpers le dan al delantero lo que el gyro le daba:
//
//   1) EJE DE ATAQUE sin BNO: el heading del OTOS entra como TERCER fallback
//      (cámara > BNO > OTOS). 0° del OTOS = rumbo del boot, que por protocolo
//      mira al arco rival — la misma convención que ya usa el fallback del BNO.
//   2) RUMBO SOSTENIDO durante el empuje: P puro sobre el yaw del OTOS, con
//      clamp y BAIL-OUT (si el error es enorme, mejor no corregir que perseguir
//      un signo invertido — misma filosofía que gk_gyro_hold_omega en strategy).
//   3) DIRECCIÓN DE EMPUJE congelada: sin lazo fino de rumbo el robot no puede
//      "apuntar el frente al arco"; en cambio empuja EN LA DIRECCIÓN DONDE
//      ESTABA LA PELOTA al comprometerse (el omni traslada en diagonal sin
//      rotar). La condición de disparo (pelota sobre el eje de ataque) garantiza
//      que esa dirección ≈ dirección al arco.
//   4) FRENO ANTI-CHOQUE (2ª instancia, env *_obst): corta el avance si
//      min_obstacle del snapshot (4 ToF + HC-SR04 frontal) está bajo el umbral.
//
// PURO (sin Arduino, solo <stdint.h>/<math.h>) → host-testeable en
// test_atk_nogyro:  bash scripts/run-host-tests.sh test_atk_nogyro
//
// Convenciones (las de kinematics.h / behind_ball.h):
//   +Y = frente del robot · +X = derecha · ángulos en deg, atan2(x, y),
//   0 = frente, +90 = derecha · omega CCW+ · heading CCW+ (0 = rumbo del boot).
// ⚠️ El signo CCW+ del yaw del OTOS se VERIFICA EN BANCO antes de confiar el
//   empuje (paso 2 de la secuencia de práctica): girar el robot a mano en
//   sentido ANTIHORARIO visto desde arriba → `otos=` del panel debe SUBIR.

#pragma once
#include <stdint.h>
#include <math.h>

namespace iitasoccer {

// Normaliza un ángulo a (-180, +180].
inline float nogyro_norm180(float a) {
    while (a > 180.0f)  a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

// === (1) Eje de ataque con fallback OTOS ===
//
// Espejo puro de atk_attack_axis (strategy.cpp) con la rama OTOS agregada.
// Prioridad: cámara (referencia absoluta, corrige drift) > BNO > OTOS > inválido.
// rel_deg = hacia dónde queda el arco rival RELATIVO al frente del robot.
struct NoGyroAxis {
    bool  valid;
    float rel_deg;
};

inline NoGyroAxis nogyro_attack_axis(bool goal_visible, float goal_rel_deg,
                                     bool bno_valid, float bno_heading_deg,
                                     bool otos_fresh, float otos_heading_deg) {
    if (goal_visible) return {true, goal_rel_deg};
    if (bno_valid)    return {true, nogyro_norm180(-bno_heading_deg)};
    if (otos_fresh)   return {true, nogyro_norm180(-otos_heading_deg)};
    return {false, 0.0f};
}

// === (2) Rumbo sostenido por OTOS (P + clamp + bail-out) ===
//
// Devuelve omega en deg/s (CCW+) para mantener hdg0 (el yaw capturado al entrar
// al empuje). Sin dato fresco → 0 (no corregir a ciegas). |error| > bailout →
// 0 (un error que CRECE con el P activo = signo invertido o yaw basura: mejor
// empuje abierto recto-ish que enroscarse persiguiendo la corrección al revés).
inline float nogyro_yaw_hold_omega_degps(bool otos_fresh,
                                         float hdg0_deg, float hdg_deg,
                                         float kp,
                                         float omega_max_degps,
                                         float bailout_deg) {
    if (!otos_fresh) return 0.0f;
    const float err  = nogyro_norm180(hdg0_deg - hdg_deg);
    const float aerr = (err < 0.0f) ? -err : err;
    if (aerr > bailout_deg) return 0.0f;
    float omega = kp * err;
    if (omega >  omega_max_degps) omega =  omega_max_degps;
    if (omega < -omega_max_degps) omega = -omega_max_degps;
    return omega;
}

// === (3) Dirección de empuje congelada ===
//
// Vector unitario hacia la pelota (robot-frame) al momento de comprometer el
// empuje. Caso degenerado (pelota encima, dist ~0): empujar al frente — es la
// dirección menos mala y la que el robot 2025 usaba siempre.
struct NoGyroPushDir {
    bool  valid;
    float ux;
    float uy;
};

inline NoGyroPushDir nogyro_push_dir(float ball_x_mm, float ball_y_mm) {
    const float d = sqrtf(ball_x_mm * ball_x_mm + ball_y_mm * ball_y_mm);
    if (d < 1.0f) return {false, 0.0f, 1.0f};
    return {true, ball_x_mm / d, ball_y_mm / d};
}

// === (4) Freno anti-choque sobre el componente de avance ===
//
// min_obstacle viene del snapshot del TOP = min(4 ToF, HC-SR04 frontal);
// 65535 = sin lectura (sensores mudos) → NO frena (el gate no inventa
// obstáculos). Solo corta vy>0 (avance): el dato no trae dirección, pero el
// caso de uso es "no embestir al adversario de frente"; retroceso y lateral
// pasan intactos. El caller decide a qué rol aplicarlo (al arquero NO: patrulla
// con la pared del arco atrás y el min incluiría esa pared).
inline int16_t nogyro_obstacle_gate_vy(int16_t vy_mm_s,
                                       bool snapshot_fresh,
                                       uint16_t min_obstacle_mm,
                                       uint16_t stop_mm) {
    if (!snapshot_fresh) return vy_mm_s;     // sin mundo fresco no se decide acá
    if (vy_mm_s <= 0)    return vy_mm_s;     // solo gobierna el AVANCE
    if (min_obstacle_mm < stop_mm) return 0; // obstáculo cerca → cortar avance
    return vy_mm_s;
}

}  // namespace iitasoccer
