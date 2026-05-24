// behind_ball.h — Helpers de posicionamiento táctico del delantero.
//
// CENTRAL Nivel 2 (spec FIRMWARE-PLACA-CENTRAL §18.2). Pure functions: ningún
// dependencia de Arduino para que se puedan testear host-native con
// `pio test -e test_native -f test_behind_ball`.
//
// El problema: si el robot embiste la pelota desde cualquier ángulo, la patea
// hacia donde estaba apuntando ANTES de tocarla — no hacia el arco rival. Hay
// que posicionarse DETRÁS de la pelota mirando al arco rival, y recién ahí
// avanzar a empujar/patear.
//
// Convenciones de coords (TODAS relativas al robot):
//   • +y = frente del robot
//   • +x = lateral derecho
//   • Ángulos en grados, atan2(x, y) (0 = frente, +90 = derecha, ±180 = atrás)
//
// Lo que NO hacemos acá:
//   • Pose absoluta del robot (queda para EKF Nivel 3).
//   • Orbit suave alrededor de la pelota — el "target" se recalcula en cada
//     tick y la FSM lo persigue como punto móvil. Es más simple y robusto.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Target para el estado POSITION del delantero — punto donde el robot debería
// estar para quedar detrás de la pelota mirando al arco rival.
//
// Inputs (todos relativos al robot):
//   ball_x_mm, ball_y_mm  : posición de la pelota
//   goal_angle_deg        : ángulo al arco rival respecto al frente del robot
//                           (atan2(goal_x, goal_y) en deg)
//   gap_mm                : distancia que dejamos entre el robot y la pelota
//                           cuando estamos posicionados detrás (~100-150 mm)
//
// Output: target relativo al robot (mm). El robot debe moverse a (target_x,
// target_y) y orientarse mirando al arco. Cuando llega, queda en línea
// pelota–arco con la pelota delante.
struct BehindBallTarget {
    float target_x_mm;
    float target_y_mm;
};

BehindBallTarget compute_behind_ball_target(float ball_x_mm,
                                            float ball_y_mm,
                                            float goal_angle_deg,
                                            float gap_mm);

// Chequea si el robot está alineado para patear / empujar:
//   • La pelota está cerca (distance ≤ kick_dist_mm)
//   • El robot está apuntando al arco rival (|goal_angle_deg| ≤ kick_angle_deg)
//
// Se usa para decidir cuándo emitir cmd.kicker_fire = 1.
bool is_aligned_to_shoot(float ball_x_mm,
                         float ball_y_mm,
                         float goal_angle_deg,
                         float kick_dist_mm,
                         float kick_angle_deg);

// Decide si la pelota ya está bien posicionada respecto al arco para hacer
// approach DIRECTO en vez de orbit. Idea: si el ángulo robot→pelota está
// alineado con el ángulo robot→arco (mismo signo, diferencia chica), entonces
// pasar por la pelota me lleva igual al arco — no hace falta orbit.
//
// Threshold sugerido: ~25-35°. Por encima de eso, hay que rodear primero.
bool ball_is_in_attack_line(float ball_x_mm,
                            float ball_y_mm,
                            float goal_angle_deg,
                            float angle_tolerance_deg);

// Velocidad de kickoff — un boost simple al frente cuando arranca el partido.
// Durante kickoff_ms el robot avanza a vy = kickoff_speed_mm_s + heading hacia
// el arco rival. Después transitions a SEARCH normal.
//
// Output: (vx, vy) en mm/s relativo al robot.
struct KickoffVelocity {
    float vx_mm_s;
    float vy_mm_s;
};

KickoffVelocity compute_kickoff_velocity(float kickoff_speed_mm_s);

}  // namespace iitasoccer
