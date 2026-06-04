// behind_ball_abs.h — Behind-the-ball en coordenadas ABSOLUTAS de cancha.
//
// CENTRAL Nivel 3 (spec FIRMWARE-PLACA-CENTRAL §8.4/§8.5). Función PURA: ninguna
// dependencia de Arduino/Wire/hardware, sólo <stdint.h> (header) y <cmath>
// (impl), para testear host-native con `run-host-tests.sh` / `pio test -f
// test_behind_ball_abs`.
//
// Diferencia con `behind_ball.h` (Nivel 2): aquel es RELATIVO al robot (sólo
// ángulos/distancias relativas, sin pose absoluta). Éste usa la POSE absoluta
// del robot + la posición de la pelota + la posición del arco rival, TODO en
// coordenadas de cancha (mm), y calcula el punto detrás de la pelota sobre la
// recta pelota→arco. Es FUNDACIÓN de Nivel 3: NO está cableado en ningún runtime
// vivo todavía (se enchufa cuando la pose absoluta sea confiable en banco).
//
// ── Convención de ejes de CANCHA (docs/CONVENCION-EJES-ROBOT.md §2) ──────────
//   • Origen (0,0) = esquina propia-izquierda (pared SO).
//   • +Y crece hacia el arco RIVAL — lado LARGO (arco-a-arco), FIELD_HEIGHT.
//   • +X crece hacia la DERECHA del robot mirando al arco — lado CORTO, FIELD_WIDTH.
//   • Heading 0 = el robot mira al arco rival (+Y). El heading CRECE en sentido
//     CCW (a la izquierda del robot). Por eso, para apuntar a un vector de
//     cancha (dx, dy), el heading deseado es  -atan2(dx, dy):
//       - atan2(dx,dy) da el ángulo del vector medido desde +Y, creciendo
//         hacia +X (derecha, CW);
//       - el heading crece CCW (izquierda), de signo opuesto → se niega.
//     (Mismo razonamiento que el §1 de la convención: un objeto a +X = a la
//      derecha = girar CW = heading negativo.)
//
// Todos los valores de posición son enteros en mm; los ángulos de heading van
// en CENTIGRADOS (centideg = grados*100), igual que el resto del firmware
// (WorldSnapshot.heading_centideg), normalizados a (-18000, +18000].

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Entrada: pose del robot + pelota + arco rival, todo en coords ABSOLUTAS de
// cancha (mm). gap_mm = distancia que dejamos por DETRÁS de la pelota (lado
// opuesto al arco) para el punto de posicionamiento (~120-150 mm). Las dims de
// cancha (field_width_mm = eje X corto, field_height_mm = eje Y largo) se pasan
// como entrada para clampear el target adentro y mantener el módulo PURO (sin
// depender de pinout_common.h).
struct BehindBallAbsIn {
    int16_t  robot_x_mm;
    int16_t  robot_y_mm;
    int16_t  robot_heading_centideg;   // heading actual (centideg); informativo
    int16_t  ball_x_mm;
    int16_t  ball_y_mm;
    int16_t  opp_goal_x_mm;            // centro boca del arco rival (~910)
    int16_t  opp_goal_y_mm;            // boca/fondo del arco rival (~2310/2430)
    uint16_t gap_mm;                   // separación detrás de la pelota
    uint16_t field_width_mm;           // eje X (corto), p.ej. FIELD_WIDTH_MM=1820
    uint16_t field_height_mm;          // eje Y (largo), p.ej. FIELD_HEIGHT_MM=2430
};

// Salida: punto-objetivo absoluto donde el robot debe ir (detrás de la pelota,
// sobre la recta pelota→arco, lado opuesto al arco), el heading deseado
// (apuntando del robot hacia el ARCO rival, para empujar derecho al arco), y si
// el robot ya está alineado para empujar al arco.
struct BehindBallAbsOut {
    int16_t target_x_mm;
    int16_t target_y_mm;
    int16_t desired_heading_centideg;  // robot→arco rival, centideg, (-18000,18000]
    bool    is_aligned_to_shoot;
};

// Calcula el behind-the-ball absoluto.
//
// target = pos sobre la recta pelota→arco_rival, a `gap_mm` por DETRÁS de la
//   pelota (en sentido OPUESTO al arco): target = pelota - gap * û, con
//   û = (arco - pelota) / |arco - pelota|. Se clampea dentro de la cancha
//   (pared-a-pared: x∈[0,field_width], y∈[0,field_height]).
//
// desired_heading = heading que deja al robot mirando al ARCO rival desde su
//   posición actual = -atan2(goal_x - robot_x, goal_y - robot_y) en centideg
//   (apuntar al arco; así, al empujar la pelota que está delante, va al arco).
//
// is_aligned_to_shoot = el robot, la pelota y el arco están COLINEALES dentro de
//   `align_tol_centideg`, Y el robot está DETRÁS de la pelota (del lado opuesto
//   al arco) → un empuje recto manda la pelota al arco. Se compara la dirección
//   robot→pelota con la dirección pelota→arco: si coinciden (mismo sentido,
//   diferencia ≤ tol), el robot está detrás y alineado.
//
// Robustez: si la pelota coincide con el arco (|arco-pelota|≈0) la recta es
//   degenerada → target = pelota clampeada, heading = robot→pelota (mejor
//   esfuerzo), is_aligned_to_shoot = false. Igual si robot==pelota para el test
//   de alineación. Usa atan2/hypot (sin división insegura).
//
// align_tol_centideg: tolerancia angular de colinealidad (p.ej. 1000 = 10°).
BehindBallAbsOut behind_ball_abs(const BehindBallAbsIn& in,
                                 int16_t align_tol_centideg);

}  // namespace iitasoccer
