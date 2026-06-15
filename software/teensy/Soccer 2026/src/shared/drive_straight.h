// drive_straight.h — control "manejar/patear DERECHO" usando odometría OTOS (WP-2A, Capa 2).
//
// Módulo PURO host-testeable (solo <stdint.h>/<math.h>/<string.h>, SIN Arduino),
// vive en src/shared como pids.h / kinematics.h. Lo consume CENTRAL (strategy.cpp)
// para refinar el avance recto / la corrección antes de patear con la pose+vel
// que DOWN difunde por OTOS (Capa 1 ya mergeada).
//
// ── Convención de signos (igual que kinematics.h) ──────────────────────────
//   • +Y  = frente del robot.
//   • +X  = derecha del robot.
//   • omega CCW+ (positivo = girar antihorario visto desde arriba).
//   • Heading en grados; angdiff normalizado a [-180, +180].
//
// ── Ley de control ─────────────────────────────────────────────────────────
//   omega_deg_s = kp_heading * angdiff(target_heading, cur_heading)
//       → error de heading POSITIVO (target adelante de cur en sentido CCW)
//         produce omega POSITIVO, que con CCW+ aumenta el heading hacia target
//         y por ende REDUCE el error. (Mismo sentido que HeadingPID.)
//   vy_mm_s     = -kp_lateral * otos_vy_mm_s
//       → cancela la deriva lateral MEDIDA por el OTOS: si el OTOS observa
//         vy>0, la corrección es vy<0 (se opone). Realimentación de velocidad
//         pura (no integra), de baja latencia (sin round-trip por el TOP).
//   vx_mm_s     = fwd_speed_mm_s
//       → el avance pedido por la FSM se respeta tal cual (la corrección NO lo toca).
//
// Es deliberadamente proporcional/sin estado: una sola función pura, fácil de
// testear y de blendear con el HeadingPID existente en strategy si hiciera falta.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Ganancias del lazo. Valores por defecto pensados como punto de partida de banco:
//   • kp_heading = 3.0 grados/s por grado de error — alineado con HeadingPID.kp
//     (pids.h) para que el comportamiento sea consistente con el resto de strategy.
//   • kp_lateral = 0.5 (mm/s correctivo) por (mm/s de deriva medida) — amortigua
//     ~la mitad de la deriva observada por tick; conservador para no sobre-corregir
//     (la realimentación de velocidad pura puede oscilar si la ganancia es ~1).
struct DriveStraightCfg {
    float kp_heading = 3.0f;
    float kp_lateral = 0.5f;
    // H6 (bug #24): tope de |omega| en °/s. El caller hace omega×100 → centideg int16;
    // 327 °/s ×100 = 32700 < 32767, así NUNCA desborda. drive_straight_compute clampea
    // a ±omega_max para que un target de heading REAL no produzca un omega gigante.
    float omega_max_deg_s = 327.0f;
};

// Entradas: el setpoint de heading (orientación de cancha deseada), el heading
// actual y la velocidad lateral medida por OTOS, más el avance pedido por la FSM.
struct DriveStraightIn {
    float target_heading_deg;   // setpoint de orientación (marco cancha)
    float cur_heading_deg;      // heading actual (del OTOS o del snapshot)
    float otos_vy_mm_s;         // velocidad lateral MEDIDA por OTOS (deriva a cancelar)
    float fwd_speed_mm_s;       // avance deseado (lo fija la FSM)
};

// Salida: comando de movimiento en unidades de strategy (mm/s y grados/s).
//   El caller convierte omega_deg_s → centideg/s (×100) al armar el MotorCommand.
//
// ⚠️ NAMING INTERNO — NO coincide con kinematics.h. A pesar del bloque de
// "convención de signos" de arriba (heredado), los campos vx_mm_s / vy_mm_s de
// ESTE struct NO usan los ejes de kinematics (donde +X=derecha y vx es lateral).
// Acá el naming es INTERNO al módulo drive-straight:
//   • vx_mm_s = AVANCE recto (eje +Y / frente del robot).
//   • vy_mm_s = CORRECCIÓN LATERAL (eje +X / lateral del robot).
// Es decir, vx↔vy están "cruzados" respecto de kinematics. El caller DEBE
// re-bindear al armar el MotorCommand:
//   MotorCommand.vy_mm_s (+Y/frente)   ← ds.vx_mm_s
//   MotorCommand.vx_mm_s (+X/lateral)  ← ds.vy_mm_s
// (omega_deg_s SÍ es el ω directo, sin cruce: ×100 → centideg/s.)
// strategy.cpp ya hace este re-bindeo; no renombramos los campos para no tocar
// los call sites existentes.
struct DriveStraightCmd {
    float vx_mm_s;       // = fwd_speed (AVANCE recto, eje +Y/frente — ver nota ⚠️ arriba)
    float vy_mm_s;       // corrección lateral (eje +X/lateral, opuesta a la deriva OTOS)
    float omega_deg_s;   // corrección de heading (CCW+)
};

// angdiff con wrap-around a [-180, +180]: devuelve (a - b) por el camino corto.
// (Misma semántica que pids.h::wrap_diff_deg; se reexpone acá para que el módulo
//  sea autocontenido y no dependa del link order de pids.cpp.)
float drive_straight_angdiff_deg(float a, float b);

// Computa el comando drive-straight (función PURA, sin estado).
DriveStraightCmd drive_straight_compute(const DriveStraightIn& in,
                                        const DriveStraightCfg& cfg);

}  // namespace iitasoccer
