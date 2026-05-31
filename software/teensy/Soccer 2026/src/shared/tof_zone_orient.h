// tof_zone_orient.h — Corrección de ORIENTACIÓN INTERNA de las zonas de los ToF.
//
// Header-only, puro (sin Arduino). Lo usan tanto el diag (diag_top_tof_zonemap)
// como el firmware del lidar-360 cuando se escriba (HAL Sprint B). Una sola
// fuente de verdad para "cómo está girado cada sensor".
//
// Hallazgo de banco (2026-05-31, Gustavo) — ver
// docs/CONVENCION-EJES-ROBOT.md y journal/2026-05-31-top-tof-posiciones-...:
//   • TOF0 (FRENTE), TOF1 (ATRÁS), TOF2 (DERECHA) comparten la MISMA
//     orientación interna entre sí.
//   • TOF3 (IZQUIERDA) es de otro fabricante y se montó mirando hacia abajo →
//     su grilla está ROTADA 180° respecto a los otros 3: "lo de arriba es como
//     abajo de los otros, y lo de izquierda es como la derecha de los otros".
//     Una rotación de 180° = invertir filas Y columnas.
//
// "Canónico" aquí = el marco interno común de TOF0/1/2. La corrección alinea al
// IZQUIERDO con ese marco, de modo que los 4 sensores quedan mutuamente
// consistentes. (Aparte de esto, ese marco común tiene una rotación de ~90°
// respecto a la grilla "cruda impresa" del diag; ESO se maneja al mapear
// zona→azimut para el lidar-360, no acá. Acá solo igualamos los 4 entre sí.)

#pragma once

namespace iitasoccer {

// Índices de los 4 ToF fijos (coinciden con PIN_TOF_XSHUT y posición física):
//   0 = FRENTE, 1 = ATRÁS, 2 = DERECHA, 3 = IZQUIERDA.
constexpr int TOF_IDX_FRONT = 0;
constexpr int TOF_IDX_BACK  = 1;
constexpr int TOF_IDX_RIGHT = 2;
constexpr int TOF_IDX_LEFT  = 3;

// ¿Este sensor necesita la corrección de 180°? Solo el izquierdo.
inline bool tof_zone_needs_180(int sensor_idx) {
    return sensor_idx == TOF_IDX_LEFT;
}

// Dada una zona 'canon' (0..grid_w*grid_w-1) en el marco canónico (el de
// TOF0/1/2), devuelve de qué zona CRUDA del sensor hay que leerla.
// - Para TOF0/1/2: identidad (ya están en el marco canónico).
// - Para TOF3 (izquierdo): rotación 180° = invertir fila y columna.
//   Como la rotación de 180° es su propia inversa, esta misma función sirve
//   en ambos sentidos (canónico↔crudo).
// 'grid_w' es el ancho de la grilla (8 para 8x8, 4 para 4x4).
// Defensivo: ante índices/anchos inválidos devuelve el mismo valor.
inline int tof_raw_zone_for_canonical(int sensor_idx, int canon_zone, int grid_w) {
    if (grid_w <= 0) return canon_zone;
    const int n = grid_w * grid_w;
    if (canon_zone < 0 || canon_zone >= n) return canon_zone;
    if (!tof_zone_needs_180(sensor_idx)) return canon_zone;
    const int row = canon_zone / grid_w;
    const int col = canon_zone % grid_w;
    const int nrow = (grid_w - 1) - row;
    const int ncol = (grid_w - 1) - col;
    return nrow * grid_w + ncol;
}

}  // namespace iitasoccer
