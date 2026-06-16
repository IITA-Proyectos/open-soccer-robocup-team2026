// line_neighbors.h — LUT de VECINO FÍSICO de los sensores del anillo DOWN.
// Módulo PURO (sin Arduino), header-only, host-testeable.
//
// EL PROBLEMA QUE RESUELVE
// ------------------------
// Varios filtros del anillo (lf_spatial_filter de line_filters.h, el centroide
// de line_early_escape.h, etc.) deciden "este blanco es confiable si TIENE UN
// VECINO también blanco". Hoy ese "vecino" es el vecino de ÍNDICE: i-1 / i+1 en
// el array, con wrap del anillo. Eso asume —implícitamente— que sensores
// contiguos de índice están contiguos en el PCB. EN ESTA PLACA NO ES VERDAD.
//
// El layout físico de DOWN tiene 4 muxes que NO recorren el anillo en orden
// circular limpio: hay saltos grandes en los bordes de mux y entre anillo
// externo e interno. Resultado: pares de índices CONTIGUOS que están FÍSICAMENTE
// LEJOS (medido sobre SENSOR_POS, ver down_model.cpp:245-246):
//
//     idx  7 <-> 8 : 141 mm   (fin mux U1 frontal → arranque mux U2 izquierdo)
//     idx 23 <-> 24: 116 mm   (fin mux U3 derecho → arranque anillo interno)
//     idx 31 <-> 0 : 102 mm   (wrap del anillo: interno trasero → frontal)
//
// Usar el vecino de ÍNDICE para esos pares es un BUG silencioso: un sensor que
// ve línea REAL pero cuyo único "vecino de índice" está a 14 cm (y por eso no
// encendió) queda marcado como "blanco aislado" y se descarta — justo el modo
// de falla del arquero parkeado sobre la línea de fondo (down_model.cpp:248-254).
//
// QUÉ HACE ESTE MÓDULO
// --------------------
// Precomputa, para CADA sensor, sus 1-2 vecinos FÍSICAMENTE más cercanos en mm
// (distancia euclídea sobre las coords reales (x,y) del PCB), descartando a
// cualquiera que esté más lejos que un radio de corte. El vecino físico ≠ vecino
// de índice: idx 7 NO tiene a idx 8 como vecino (141 mm), sino a idx 6 (9 mm).
//
// Es solo una LUT geométrica (constante en runtime — la geometría del PCB no
// cambia). El que la consume (line_early_escape, line_filters, …) usa
// `ln_is_neighbor(lut, i, j)` o itera `ln_neighbors_of(lut, i)` en vez del
// hardcodeo i±1.
//
// FAIL-SAFE / por qué header-only (inline): módulo chico y PURO, así un test lo
// linkea sin arrastrar sensor_geometry.cpp (las coords entran POR PARÁMETRO).
// Andamiaje OFF-by-default: nada de esto corre en el firmware vivo hasta que se
// cablee en banco post-Incheon. No toca ningún .cpp.

#pragma once
#include <stdint.h>
#include <math.h>

namespace iitasoccer {

// Tope de sensores que maneja el módulo (anillo DOWN = 32). Mismo tope que LEE.
constexpr int LN_MAX_SENSORS = 32;

// Cuántos vecinos físicos guarda como máximo por sensor. El layout de DOWN da
// 1 o 2 vecinos cercanos reales (el 3º salta a >23 mm en el anillo, a >45 mm en
// el interno). 2 es suficiente para reproducir el criterio "tiene un vecino" sin
// inflar la LUT. (Si algún día un anillo más denso necesita 3, subir esto.)
constexpr int LN_MAX_NEIGHBORS = 2;

// Radio de corte por defecto, en mm. Un candidato cuenta como vecino físico solo
// si está a <= este radio. Elegido a partir de la geometría MEDIDA de DOWN:
//   - el vecino más cercano de CADA sensor está a <= ~15 mm (siempre entra);
//   - el 2º vecino del anillo externo está a ~10-20 mm (entra);
//   - los sensores internos (idx 28-31) tienen su 2º candidato a ~45-52 mm
//     (queda AFUERA → 1 solo vecino, correcto: están aislados);
//   - los pares contiguos-de-índice-pero-lejanos (141/116/102 mm) quedan MUY
//     afuera (jamás se cuelan como vecinos). 30 mm los descarta con margen 3x.
// El llamador puede pisarlo (ln_build acepta radio explícito) para titular.
constexpr float LN_DEFAULT_RADIUS_MM = 30.0f;

// Posición física de un sensor (mm). Misma convención y mismo layout de struct
// que SensorPos2D (sensor_geometry.h) y LeeSensorPos (line_early_escape.h). Se
// redefine acá para que el módulo sea autónomo (no incluye sensor_geometry.h):
// el caller pasa SENSOR_POS o la geometría que quiera por parámetro.
struct LnSensorPos {
    float x_mm;   // +X = derecha del robot
    float y_mm;   // +Y = adelante del robot
};

// LUT de vecinos físicos. `count[i]` = cuántos vecinos válidos tiene el sensor i
// (0..LN_MAX_NEIGHBORS); `idx[i][k]` = índice del k-ésimo vecino más cercano
// (los slots no usados quedan en -1). Tamaño fijo → cero malloc, apta para MCU.
struct LineNeighbors {
    int8_t count[LN_MAX_SENSORS];
    int8_t idx[LN_MAX_SENSORS][LN_MAX_NEIGHBORS];
};

// === Implementación PURA (inline, header-only) ===

// Distancia euclídea (mm) entre dos sensores. Float: las coords son chicas
// (< 90 mm) y un MCU con FPU (Teensy 4.0) lo hace en nada. Pero esta LUT se
// construye UNA vez (boot), así que el costo es irrelevante igual.
inline float ln_dist_mm(const LnSensorPos& a, const LnSensorPos& b) {
    const float dx = a.x_mm - b.x_mm;
    const float dy = a.y_mm - b.y_mm;
    return sqrtf(dx * dx + dy * dy);
}

// Construye la LUT de vecinos físicos.
//   pos          array de n posiciones (x,y) en mm (p.ej. SENSOR_POS).
//   n            cantidad de sensores (<= LN_MAX_SENSORS).
//   out          LUT de salida (se llena entera).
//   radius_mm    radio de corte; un candidato cuenta solo si dist <= radius_mm.
//
// Para cada sensor i recorre TODOS los demás, ordena por distancia y se queda
// con los LN_MAX_NEIGHBORS más cercanos que estén dentro del radio. O(n²) pero
// n=32 y corre una sola vez → trivial. Selección por inserción directa (sin
// asignar memoria) para que sea apta para correr en el MCU al bootear.
//
// FAIL-SAFE: ante pos==nullptr / n<=0 deja la LUT en "sin vecinos" (count=0,
// idx=-1). Un consumidor que pregunte ln_is_neighbor sobre una LUT vacía recibe
// false para todo → degrada al comportamiento "ningún vecino", que es el lado
// CONSERVADOR (no inventa adyacencias falsas). Nunca produce un vecino fantasma.
inline void ln_build(const LnSensorPos* pos, int n, LineNeighbors* out,
                     float radius_mm = LN_DEFAULT_RADIUS_MM) {
    if (out == nullptr) return;

    // Inicializar TODO a "sin vecinos" primero (cubre el caso de guarda y los
    // slots no usados de cada sensor).
    for (int i = 0; i < LN_MAX_SENSORS; ++i) {
        out->count[i] = 0;
        for (int k = 0; k < LN_MAX_NEIGHBORS; ++k) out->idx[i][k] = -1;
    }

    if (pos == nullptr || n <= 0) return;
    if (n > LN_MAX_SENSORS) n = LN_MAX_SENSORS;

    for (int i = 0; i < n; ++i) {
        // Top-K por inserción: mantenemos hasta LN_MAX_NEIGHBORS candidatos
        // ordenados por distancia ascendente. best_d[k]/best_j[k] son paralelos.
        float best_d[LN_MAX_NEIGHBORS];
        int   best_j[LN_MAX_NEIGHBORS];
        int   k_used = 0;

        for (int j = 0; j < n; ++j) {
            if (j == i) continue;
            const float d = ln_dist_mm(pos[i], pos[j]);
            if (d > radius_mm) continue;   // fuera del radio → no es vecino físico

            // Insertar (d, j) manteniendo el array ordenado por distancia.
            // Buscamos la posición donde entra (el primer slot con d menor a d
            // del candidato, o el final si hay lugar).
            int p = k_used;
            // Si ya está lleno y el candidato es peor que el peor guardado, descarto.
            if (k_used == LN_MAX_NEIGHBORS && d >= best_d[LN_MAX_NEIGHBORS - 1]) {
                continue;
            }
            // Punto de inserción: deslizar hacia abajo los que son peores que d.
            int start = (k_used < LN_MAX_NEIGHBORS) ? k_used : (LN_MAX_NEIGHBORS - 1);
            p = start;
            while (p > 0 && best_d[p - 1] > d) {
                best_d[p] = best_d[p - 1];
                best_j[p] = best_j[p - 1];
                --p;
            }
            best_d[p] = d;
            best_j[p] = j;
            if (k_used < LN_MAX_NEIGHBORS) ++k_used;
        }

        out->count[i] = (int8_t)k_used;
        for (int k = 0; k < k_used; ++k) out->idx[i][k] = (int8_t)best_j[k];
    }
}

// ¿El sensor j es vecino FÍSICO del sensor i (según la LUT)?
// Relación simétrica en la práctica (si i tiene a j cerca, j tiene a i cerca),
// pero NO se asume: con top-K y radio podría no ser estrictamente simétrica en
// casos límite (i en la lista de j pero no al revés si j tiene 2 más cercanos).
// Por eso ln_is_neighbor chequea SOLO la lista de i — el llamador decide si
// quiere OR simétrico. lut==nullptr o índices fuera de rango → false (seguro).
inline bool ln_is_neighbor(const LineNeighbors* lut, int i, int j) {
    if (lut == nullptr) return false;
    if (i < 0 || i >= LN_MAX_SENSORS || j < 0 || j >= LN_MAX_SENSORS) return false;
    const int c = lut->count[i];
    for (int k = 0; k < c && k < LN_MAX_NEIGHBORS; ++k) {
        if (lut->idx[i][k] == (int8_t)j) return true;
    }
    return false;
}

// Versión simétrica explícita: j es vecino de i O i es vecino de j. Es la que
// conviene usar como "están físicamente adyacentes" (robusta al caso límite del
// top-K asimétrico). lut==nullptr → false.
inline bool ln_adjacent(const LineNeighbors* lut, int i, int j) {
    return ln_is_neighbor(lut, i, j) || ln_is_neighbor(lut, j, i);
}

// ¿El sensor i tiene ALGÚN vecino físico, dentro de la lista `white`, en blanco?
// Es el reemplazo directo del hardcodeo "white[i-1] || white[i+1]" del filtro
// espacial, pero con vecinos REALES. white==nullptr o lut==nullptr → false.
inline bool ln_has_white_neighbor(const LineNeighbors* lut, const bool* white,
                                  int i, int n) {
    if (lut == nullptr || white == nullptr) return false;
    if (i < 0 || i >= LN_MAX_SENSORS) return false;
    const int c = lut->count[i];
    for (int k = 0; k < c && k < LN_MAX_NEIGHBORS; ++k) {
        const int j = lut->idx[i][k];
        if (j >= 0 && j < n && white[j]) return true;
    }
    return false;
}

}  // namespace iitasoccer
