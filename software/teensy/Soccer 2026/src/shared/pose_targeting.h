// pose_targeting.h — Geometría de targeting ABSOLUTO en cancha (pose conocida).
//
// Helpers de geometría de cancha útiles a AMBOS roles (arquero y delantero)
// CUANDO se dispone de pose absoluta confiable (x, y, heading del robot en el
// marco de la cancha). Mientras la pose absoluta no esté validada en banco,
// este módulo es FUNDACIÓN de Nivel 3: PURO y host-testeable, pero NO cableado
// en strategy.cpp ni en ningún runtime vivo. Se enchufa recién cuando el equipo
// confíe en la pose (no lo decide este código).
//
// Pureza: sin Arduino/Wire/hardware. Solo <stdint.h>/<cmath>/<stddef.h>.
// Se compila host-native con el runner scripts/run-host-tests.sh
// (env test_native, -I src/shared).
//
// ── Convención de ejes (CANÓNICA: docs/CONVENCION-EJES-ROBOT.md §2) ──────────
// Marco de CANCHA (pose absoluta), origen en la esquina propia-izquierda (pared):
//   • +X = lateral, lado CORTO (pared a pared = FIELD_WIDTH_MM = 1820 mm).
//   • +Y = arco-a-arco, lado LARGO (pared a pared = FIELD_HEIGHT_MM = 2430 mm).
//     +Y crece hacia el arco RIVAL.
//   • Arco PROPIO centrado en x≈910, boca sobre la línea blanca en y≈120
//     (la pared sur está en y=0). Arco RIVAL boca en y≈2310 (pared norte y=2430).
//   • Ambos arcos: caja de GOAL_WIDTH_MM = 600 mm de ancho, centrada en X
//     → abarca x ∈ [610, 1210].
//
// Heading (igual que localization.cpp::classify_wall y CONVENCION §1):
//   • heading 0 = el robot mira al arco rival (+Y).
//   • heading crece CCW (antihorario visto desde arriba) = el robot gira a su
//     IZQUIERDA. La dirección del FRENTE del robot para heading θ es
//     (−sin θ, cos θ) en (X, Y): θ=0→(0,+1)=+Y, θ=+90°→(−1,0)=−X (izquierda),
//     θ=+270°→(+1,0)=+X (derecha). Coincide con classify_wall.
//   • Ángulos de heading en CENTIGRADOS (deg×100), normalizados a [0, 36000).
//
// ⚠️ Estos valores ESPEJAN pinout_common.h (FIELD_WIDTH_MM/FIELD_HEIGHT_MM) y la
// geometría de arco de CONVENCION-EJES §2. pinout_common.h vive en src/top (NO
// está en el include path de los tests host ni es PURO), por eso NO se incluye
// acá; se replican como defaults en FieldGeometry. Si pinout cambia las dims,
// actualizar también acá.

#pragma once
#include <stdint.h>
#include <cmath>
#include <stddef.h>

namespace iitasoccer {

// ── Constantes de cancha (espejo de pinout_common.h + CONVENCION-EJES §2) ────
constexpr float FIELD_WIDTH_MM_F  = 1820.0f;  // eje X (lateral, lado corto)
constexpr float FIELD_HEIGHT_MM_F = 2430.0f;  // eje Y (arco-a-arco, lado largo)

// Ancho de la boca del arco (caja RCJ 2026: 600 mm de ancho). El tiro a defender
// se clampa a [centro - GOAL_WIDTH/2, centro + GOAL_WIDTH/2] sobre la línea de gol.
constexpr float GOAL_WIDTH_MM = 600.0f;

// Línea de gol PROPIA: y de la boca del arco propio (sobre la línea blanca sur).
// La pared sur está en y=0; la boca del arco queda 120 mm hacia adentro. El
// arquero defiende parado sobre/junto a esta línea.
constexpr float OWN_GOAL_LINE_Y_MM = 120.0f;
// Línea de gol RIVAL: y de la boca del arco rival (línea blanca norte).
constexpr float RIVAL_GOAL_LINE_Y_MM = 2310.0f;

// Descripción de la cancha para los helpers que necesitan dimensiones / arcos.
// Defaults = valores canónicos arriba; se puede sobreescribir para tests o si
// la cancha real difiere. Todo en mm, marco de cancha (origen esquina propia).
struct FieldGeometry {
    float width_mm        = FIELD_WIDTH_MM_F;       // extensión en X
    float height_mm       = FIELD_HEIGHT_MM_F;      // extensión en Y
    float own_goal_line_y = OWN_GOAL_LINE_Y_MM;     // y de la línea de gol propia
    float goal_center_x   = FIELD_WIDTH_MM_F * 0.5f; // centro del arco en X (≈910)
    float goal_width_mm   = GOAL_WIDTH_MM;          // ancho de la boca del arco
};

// Un punto en el marco de la cancha (mm).
struct FieldPoint {
    float x_mm;
    float y_mm;
};

// ── Helpers ──────────────────────────────────────────────────────────────────

// Distancia euclídea entre dos puntos (mm). Helper compartido.
float dist_mm(float ax, float ay, float bx, float by);

// Heading deseado (en CENTIGRADOS, [0, 36000)) para que el robot, parado en
// (robot_x, robot_y), mire al punto (target_x, target_y).
//   • heading 0 = mira +Y (arco rival); crece CCW (izquierda).
//   • Front del robot apunta (−sin θ, cos θ) → θ = atan2(−dx, dy).
// Caso degenerado: si el target coincide con el robot (dx=dy=0), no hay
// dirección definida → devuelve 0 (mirando al arco rival, fallback seguro).
int16_t aim_heading_to_point_centideg(float robot_x, float robot_y,
                                      float target_x, float target_y);

// Punto sobre la LÍNEA DE GOL PROPIA (y = field.own_goal_line_y) que intercepta
// el tiro recto pelota→centro-del-arco-propio, clampeado al ancho del arco
// [centro - GOAL_WIDTH/2, centro + GOAL_WIDTH/2]. Es dónde debería pararse el
// arquero para bloquear.
//
// Modelo: el arquero "sigue la pelota" sobre su línea. La pelota delante del
// arco entraría, si fuera recta, por su propia x; el arquero se para en esa x
// proyectada a la línea de gol, clampeada a la boca del arco (postes) para no
// salirse del arco. Ver el .cpp para el detalle.
//
// Casos degenerados (devuelve el CENTRO del arco):
//   • La pelota está DETRÁS o SOBRE la línea de gol propia (ball_y ≤ línea):
//     no hay tiro entrante claro → quedarse en el centro.
//   • La pelota está exactamente sobre la línea (evita usar un modelo sin tiro).
FieldPoint gk_defend_point(float ball_x, float ball_y, const FieldGeometry& field);

// ¿El robot está SOBRE (cerca de) la recta pelota→arco? Es decir, ¿está dentro
// del "carril de tiro"? Mide la distancia perpendicular del robot al segmento
// (ball)→(goal) y la compara con tol_mm.
//   • Si la pelota y el arco coinciden (segmento degenerado), se mide la
//     distancia punto-a-punto del robot a ese punto.
//   • La proyección se clampa al segmento: un robot "más allá" del arco o
//     "más atrás" de la pelota mide su distancia al extremo correspondiente,
//     no a la recta infinita.
bool in_shooting_lane(float robot_x, float robot_y,
                      float ball_x, float ball_y,
                      float goal_x, float goal_y,
                      float tol_mm);

}  // namespace iitasoccer
