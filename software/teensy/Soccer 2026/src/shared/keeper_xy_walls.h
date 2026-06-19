// keeper_xy_walls.h — Estimador XY HEADING-FREE del arquero por paredes.
//
// Logica pura, host-testeable. NO usa Arduino, Wire, ni hardware.
//
// QUE ES: dado que el arquero MIRA AL FRENTE (heading ~= 0, lo sostiene el
// heading-hold), el ToF izquierdo ve la pared izquierda, el derecho la derecha
// y el trasero la pared propia. Tres distancias -> (x,y) por trilateracion
// DIRECTA, sin BNO. Es la fuente landmark PURA (no fusion con odometria; eso es
// pose_fusion, inversion 2027). Util en la ZONA DE FONDO (cerca del arco propio).
//
// SUPUESTO Y LIMITE (la raiz, segun la skill fusion-pose-odometria-landmarks):
//   * Vale SOLO mientras el arquero mire al frente. Si rota (escape), el mapa
//     "rota" y x/y salen coherentes pero MENTIROSOS -> el consumidor DEBE congelar
//     el XY durante el escape + un margen (lo hace la capa de wiring, no este modulo).
//   * La pared LEJANA (> ~1.3 m) no se ve (el rayo pasa por encima o pega en el
//     piso). Por eso X usa la pared CERCANA + chequeo de consistencia izq/der.
//   * Con el arquero CENTRADO (ambas laterales a ~910, al limite) la X puede
//     quedar ambigua -> se marca x_valid=false (el arquero centrado no necesita
//     corregir X). LIMITE CONOCIDO, no es bug.
//
// Convencion de cancha (docs/CONVENCION-EJES-ROBOT.md §2): origen esquina propia
// izquierda; +X = derecha (corto, field_width=1820); +Y = arco rival (largo,
// field_height=2430); pared propia en y=0. Orden ToF: [0]frente [1]atras
// [2]derecha [3]izquierda (igual que localization.h).

#pragma once
#include <stdint.h>

namespace iitasoccer {

constexpr uint16_t KEEPER_TOF_NO_READING = 0xFFFF;

// ---------------------------------------------------------------------------
// Reduccion de las 16 zonas de un ToF a UNA distancia de pared.
//
// Mediana + recorte SIMETRICO: descarta el piso (cumulo bajo), el "por encima
// de la pared" (cumulo alto, aun dentro de cancha) y los picos espurios (rebotes),
// quedandose con el cumulo de PARED (el del medio). Sin necesidad de conocer la
// rotacion del sensor ni de vetar zonas a mano.
//
//   zones      : lecturas crudas (mm), KEEPER_TOF_NO_READING = invalida.
//   n          : cantidad de zonas (<=16).
//   no_reading : centinela de invalida (0xFFFF tipico).
//   trim_pct   : mitad del ancho de ventana, en % de la mediana (35 tipico ->
//                conserva [0.65*med .. 1.35*med]).
//
// Devuelve la distancia de pared (mm), o no_reading si no hay zonas validas.
// LIMITE CONOCIDO: si una clase espuria DOMINA el FOV (mayoria de zonas), la
// mediana cae en ella. Para las paredes CERCANAS del arquero la pared domina,
// asi que en su regimen de uso es robusto.
inline uint16_t keeper_wall_dist_mm(const uint16_t* zones, uint8_t n,
                                    uint16_t no_reading, uint8_t trim_pct) {
    uint16_t v[16];
    uint8_t m = 0;
    for (uint8_t i = 0; i < n && m < 16; ++i) {
        const uint16_t z = zones[i];
        if (z != no_reading && z != 0) v[m++] = z;
    }
    if (m == 0) return no_reading;

    // insertion sort (m <= 16)
    for (uint8_t i = 1; i < m; ++i) {
        const uint16_t key = v[i];
        int8_t j = (int8_t)i - 1;
        while (j >= 0 && v[j] > key) { v[j + 1] = v[j]; --j; }
        v[j + 1] = key;
    }

    const uint16_t med = (m & 1)
        ? v[m / 2]
        : (uint16_t)(((uint32_t)v[m / 2 - 1] + v[m / 2]) / 2);

    const uint32_t lo = (uint32_t)med * (100 - trim_pct) / 100;
    const uint32_t hi = (uint32_t)med * (100 + trim_pct) / 100;

    uint32_t sum = 0;
    uint8_t cnt = 0;
    for (uint8_t i = 0; i < m; ++i) {
        if (v[i] >= lo && v[i] <= hi) { sum += v[i]; ++cnt; }
    }
    if (cnt == 0) return med;   // fallback defensivo (no deberia pasar)
    return (uint16_t)(sum / cnt);
}

// ---------------------------------------------------------------------------
// Estimacion (x,y) a partir de las distancias de pared ya reducidas.

struct KeeperWallDist {
    uint16_t left_mm;    // pared izquierda  (ToF izq reducido), o KEEPER_TOF_NO_READING
    uint16_t right_mm;   // pared derecha
    uint16_t back_mm;    // pared propia (atras)
};

struct KeeperXYConfig {
    uint16_t field_width_mm;   // 1820 (eje X, lateral)
    uint16_t tof_offset_mm;    // 95  (plano del sensor -> centro del robot)
    uint16_t wall_reach_mm;    // ~1000: una "pared" reducida mas lejos que esto no es confiable
    uint16_t consistency_mm;   // ~200: si x_izq y x_der coinciden dentro de esto -> promedio
    uint16_t symmetric_mm;     // ~150: si |d_izq-d_der|<esto pero x NO cierra -> ambiguo (centro/piso)
};

struct KeeperXY {
    int16_t x_mm;   bool x_valid;
    int16_t y_mm;   bool y_valid;
};

// Reglas:
//   Y: de la pared propia (atras). Valida si la trasera esta dentro de alcance.
//   X: de la pared lateral CERCANA. Si ambas validas y coinciden -> promedio.
//      Si ambas validas pero NO coinciden y leen casi lo mismo -> ambiguo (centro
//      con piso) -> x invalida. Si discrepan y una es mas cercana -> esa (pared real).
//      Si solo una valida -> esa.
inline KeeperXY keeper_xy_from_walls(const KeeperWallDist& d, const KeeperXYConfig& c) {
    KeeperXY r;
    r.x_mm = 0; r.x_valid = false;
    r.y_mm = 0; r.y_valid = false;

    const uint16_t NR = KEEPER_TOF_NO_READING;

    // --- Y: pared propia (atras) ---
    if (d.back_mm != NR && d.back_mm <= c.wall_reach_mm) {
        r.y_mm = (int16_t)((int32_t)d.back_mm + c.tof_offset_mm);
        r.y_valid = true;
    }

    // --- X: paredes laterales ---
    const bool lok = (d.left_mm  != NR && d.left_mm  <= c.wall_reach_mm);
    const bool rok = (d.right_mm != NR && d.right_mm <= c.wall_reach_mm);

    const int32_t xL = (int32_t)d.left_mm + c.tof_offset_mm;
    const int32_t xR = (int32_t)c.field_width_mm - ((int32_t)d.right_mm + c.tof_offset_mm);

    if (lok && rok) {
        const int32_t xdiff = (xL > xR) ? (xL - xR) : (xR - xL);
        const int32_t ddiff = (d.left_mm > d.right_mm)
            ? (int32_t)d.left_mm - d.right_mm
            : (int32_t)d.right_mm - d.left_mm;
        if (xdiff <= c.consistency_mm) {
            r.x_mm = (int16_t)((xL + xR) / 2);
            r.x_valid = true;
        } else if (ddiff < c.symmetric_mm) {
            r.x_valid = false;   // ambas ven ~lo mismo pero x no cierra -> piso/centro ambiguo
        } else {
            r.x_mm = (int16_t)((d.left_mm < d.right_mm) ? xL : xR);  // pared cercana = real
            r.x_valid = true;
        }
    } else if (lok) {
        r.x_mm = (int16_t)xL;
        r.x_valid = true;
    } else if (rok) {
        r.x_mm = (int16_t)xR;
        r.x_valid = true;
    }

    return r;
}

}  // namespace iitasoccer
