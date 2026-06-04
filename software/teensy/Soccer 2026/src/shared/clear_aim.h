// clear_aim.h — Despeje DIRECCIONAL del arquero: hacia qué rumbo empujar la
// pelota al hacer CLEAR para SACARLA del área, no devolverla al centro.
//
// ── El problema ──────────────────────────────────────────────────────────────
// Hoy el arquero en CLEAR empuja la pelota DERECHO (hacia donde mira). Si la
// pelota está frente a su propio arco, ese empuje recto la deja en el medio del
// área — justo donde el rival la vuelve a meter. Mejor: empujarla hacia la BANDA
// (lateral) más alejada del centro del arco propio, alejándola del área.
//
// ── La idea geométrica ───────────────────────────────────────────────────────
// El eje "robot → arco propio" apunta hacia atrás del campo (hacia la pared del
// arco). Las dos BANDAS laterales corren PERPENDICULARES a ese eje (son el lado
// corto del área respecto al arco). Por lo tanto, el rumbo de despeje deseado es
// PERPENDICULAR a la dirección del arco propio: ±90° respecto a goal_own_angle.
//
//   • Si el arco propio queda a la IZQUIERDA del robot (goal_own_angle < 0),
//     la banda más lejana del centro del arco está a la DERECHA → empujar a la
//     derecha (push_heading > 0).
//   • Si el arco propio queda a la DERECHA (goal_own_angle > 0) → empujar a la
//     IZQUIERDA (push_heading < 0).
//   • Si el arco propio está justo ENFRENTE o JUSTO ATRÁS (|goal_own_angle| ≈ 0
//     o ≈ 180), el lado lateral es ambiguo; se desempata con la posición lateral
//     de la pelota (ball_x): se empuja hacia el lado donde ya está la pelota
//     (la banda que le queda más cerca / más lejos del centro del arco), y si la
//     pelota también está centrada, se elige la derecha por convención estable.
//
// El módulo NO mueve nada: sólo entrega un RUMBO objetivo (push_heading) en el
// marco del robot que el caller (GkState::CLEAR) puede usar para orientar el
// empuje. La conversión rumbo→velocidades y el lazo de control son del caller.
//
// ── Convención de ejes (marco ROBOT, ver docs/CONVENCION-EJES-ROBOT.md) ───────
//   • +x = DERECHA del robot, −x = IZQUIERDA.
//   • +y = FRENTE del robot,  −y = ATRÁS.
//   • Ángulo de un objeto = atan2(x, y): 0 = frente, +90 = derecha,
//     −90 = izquierda, ±180 = atrás.  (¡x primero!)
//   goal_own_angle_centideg sigue esta misma convención (del WorldSnapshot v3).
//
// ── FALLBACK EXACTO ──────────────────────────────────────────────────────────
// Si goal_own_visible == false → valid = false y push_heading = 0. El caller
// debe, en ese caso, usar su empuje DERECHO actual (byte-idéntico al de hoy):
// este módulo NO altera la conducta cuando no hay arco propio visible.
//
// ── Estilo ───────────────────────────────────────────────────────────────────
// Módulo PURO host-testeable: sólo <stdint.h>/<cmath>, SIN Arduino/Wire, vive en
// src/shared como behind_ball.h / tof_distance_hold.h. Sin estado: función pura.
// NO está cableado todavía (fundación; se valida en banco antes de activar).

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Entrada: pelota relativa al robot + ángulo al arco PROPIO (del snapshot v3).
struct ClearAimIn {
    int16_t ball_x_mm;               // +x = derecha del robot.
    int16_t ball_y_mm;               // +y = frente del robot.
    int16_t goal_own_angle_centideg; // atan2(x,y) al arco propio, centideg.
    bool    goal_own_visible;        // false → fallback exacto (valid=false).
};

// Salida: rumbo de empuje deseado, en el marco del robot.
struct ClearAimOut {
    int16_t push_heading_centideg;   // rumbo (atan2(x,y), centideg) hacia donde
                                     // empujar; sólo válido si valid==true.
    bool    valid;                   // false → usar el empuje derecho actual.
};

// Calcula el rumbo de despeje direccional. Ver doc de cabecera.
//   • goal_own_visible == false → {0, false} (fallback exacto).
//   • si no → push perpendicular al eje del arco propio, hacia la banda más
//     lejana del centro del arco; {push_heading_centideg, true}.
ClearAimOut clear_aim(const ClearAimIn& in);

}  // namespace iitasoccer
