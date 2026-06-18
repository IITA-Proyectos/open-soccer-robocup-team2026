// tof_zone_mask_orient.h — Rotacion/espejo de la MASCARA de zonas del ToF (A2.2),
// PURO host-testeable. Generaliza tof_zone_orient.h (que solo hace el 180 del sensor
// izquierdo) a rotacion 0/90/180/270 + flip por-sensor, para que la ROTACION sea
// responsabilidad del FIRMWARE (leida de TopConfig/EEPROM) y NO de la app.
//
// Para que sirve
// ----------------------------------------------------------------------------------
// Cada ToF esta montado con una orientacion distinta (el izquierdo a 180, etc.). El
// veto de zonas (zone_mask) que define el operador esta en un marco CANONICO (comun a
// los 4, el de la app cuando deja de plegar). Pero g_zones_mm viene en el marco CRUDO
// del sensor. Esta funcion convierte la mascara CANONICA -> CRUDA aplicando la
// rotacion/flip del sensor, asi el veto cae en las zonas fisicas correctas. La rotacion
// se aplica UNA sola vez, aca en el firmware (ver el flag TOP_ENABLE_TOF_ROT en
// sensors_tof.cpp). Si la app sigue plegando (flag apagado, comportamiento de hoy) NO
// se llama a esta funcion -> no hay doble rotacion.
//
// Convencion (grilla grid_w x grid_w, indice row-major = row*grid_w + col):
//   rot_deg = 0/90/180/270 = rotacion HORARIA de la grilla.
//   flip: bit0 = espejo en X (invierte columnas), bit1 = espejo en Y (invierte filas).
//   El flip se aplica DESPUES de la rotacion. La operacion es una permutacion (biyeccion)
//   de las zonas. rot_deg no multiplo de 90 -> se trata como 0 (defensivo).
//   El caso 180 coincide exactamente con tof_zone_orient.h (invertir fila y columna),
//   asi la correccion del sensor izquierdo da el mismo resultado por las dos vias.
//
// PURO: solo <stdint.h>. Compila g++ host.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Dada una zona canonica 'idx', devuelve la zona CRUDA donde cae tras aplicar
// rotacion 'rot_deg' (0/90/180/270 horaria) + 'flip' (bit0=X, bit1=Y) en una grilla
// grid_w x grid_w. Defensivo ante args invalidos: devuelve 'idx' sin tocar.
inline int tof_zone_orient_index(int idx, int rot_deg, uint8_t flip, int grid_w) {
    if (grid_w <= 0) return idx;
    const int n = grid_w * grid_w;
    if (idx < 0 || idx >= n) return idx;
    const int r = idx / grid_w;
    const int c = idx % grid_w;
    const int rot = ((rot_deg % 360) + 360) % 360;
    int nr = r, nc = c;
    switch (rot) {
        case 90:  nr = c;                nc = (grid_w - 1) - r; break;
        case 180: nr = (grid_w - 1) - r; nc = (grid_w - 1) - c; break;
        case 270: nr = (grid_w - 1) - c; nc = r;                break;
        default:  nr = r;                nc = c;                break;  // 0 e invalidos
    }
    if (flip & 0x1u) nc = (grid_w - 1) - nc;   // espejo X (columnas)
    if (flip & 0x2u) nr = (grid_w - 1) - nr;   // espejo Y (filas)
    return nr * grid_w + nc;
}

// Permuta una mascara de 16 bits (grilla 4x4) del marco CANONICO (= el del DISPLAY de la
// app, lo que ve y vetea el operador) al marco CRUDO del sensor. Con rot_deg=0 y flip=0
// devuelve la misma mascara (no-op byte-neutro).
//
// CLAVE (convencion identica a la app, tof_layout.zone_source_map): la celda CRUDA `rawpos`
// se MUESTRA en el display en la posicion flip(rotate(rawpos)) (= tof_zone_orient_index).
// Entonces la zona cruda `rawpos` queda vetada si y solo si su posicion-en-display esta
// vetada en la mascara canonica. Asi el firmware produce EXACTAMENTE la misma mascara cruda
// que hoy pliega la app -> el veto que el operador ve en pantalla cae en las zonas fisicas
// correctas (coinciden en 0/180; para 90/270 hay que iterar las CRUDAS, no las canonicas).
inline uint16_t tof_zone_mask_orient(uint16_t canon_mask, int rot_deg, uint8_t flip) {
    constexpr int GW = 4;
    if (rot_deg == 0 && flip == 0) return canon_mask;   // no-op rapido (default = sin rotar)
    uint16_t raw = 0;
    for (int rawpos = 0; rawpos < GW * GW; ++rawpos) {
        const int disp = tof_zone_orient_index(rawpos, rot_deg, flip, GW);  // donde se ve esta zona cruda
        if ((canon_mask >> disp) & 1u) raw = (uint16_t)(raw | (uint16_t)(1u << rawpos));
    }
    return raw;
}

}  // namespace iitasoccer
