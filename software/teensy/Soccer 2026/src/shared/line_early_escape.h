// line_early_escape.h — Detección TEMPRANA de línea + VECTOR DE ESCAPE con
// tolerancia a fallas. Módulo PURO (sin Arduino), header-only, host-testeable.
//
// Pedido Gustavo (banco arquero 2026-06-14): hoy el aviso de "salida inminente"
// espera `sensors_on_line >= 6` (DownModelCfg.imminent_depth → EV_IMMINENT_EXIT,
// ver types.h:46-51). Eso es TARDE: para cuando 6 sensores ven blanco el robot
// ya tiene medio anillo sobre la línea. Este módulo dispara EVIDENCIA desde el
// PRIMER sensor blanco CONFIABLE y entrega de una un vector de dirección para
// alejarse, sin esperar al umbral de inminencia.
//
// "Confiable" tiene tres condiciones, todas obligatorias (filtra el sensor loco
// aislado que arruinaría el centroide):
//   1) cruza SU umbral individual (calibrado por sensor) CON histéresis;
//   2) está SANO (flag de sensor_health: excluye muerto/clavado/ruidoso);
//   3) tiene al menos un VECINO también blanco-confiable (filtro espacial en el
//      anillo, igual criterio que lf_spatial_filter de line_filters.h).
//
// El vector de escape sale del CENTROIDE CARTESIANO de los sensores blancos
// confiables, usando su posición física (x,y) real — misma convención y misma
// idea que lg_compute_xy() de line_geometry.h (un sensor del anillo externo
// pesa más que uno interno, porque está más cerca del borde). El escape es el
// OPUESTO de ese centroide: "la línea está hacia allá → andá para el otro lado".
//
// Convención de ejes (idéntica al resto del firmware, ver sensor_geometry.h:13):
//   +X = derecha del robot,  +Y = adelante.
//   Ángulo 0° = frente (+Y),  +90° = derecha (+X),  ±180° = atrás,  -90° = izq.
//   Sentido horario positivo. Rango devuelto: (-180°, +180°].
//
// Por qué header-only (inline): es un módulo chico y PURO; así un test puede
// linkearlo sin arrastrar dependencias (p.ej. NO necesita sensor_geometry.cpp:
// las posiciones (x,y) entran por parámetro). Andamiaje OFF-by-default: nada de
// esto corre en el firmware vivo hasta integrarlo en banco post-Incheon.

#pragma once
#include <stdint.h>
#include <math.h>

namespace iitasoccer {

// Tope de sensores que maneja el módulo (anillo DOWN = 32).
constexpr int LEE_MAX_SENSORS = 32;

// === Parámetros de sintonía (defaults; cada llamador puede pisarlos) ===

// Banda de histéresis alrededor del umbral individual, en counts del ADC.
// Mismo valor que LF_HYSTERESIS_BAND (line_filters.h): entrar a blanco exige
// raw >= umbral + banda; salir exige raw < umbral - banda. Evita flickeo cuando
// la lectura oscila justo sobre el umbral.
constexpr uint16_t LEE_HYSTERESIS_BAND = 20;

// Cuántos sensores blancos-confiables marcan "inminente" (avisar fuerte a
// CENTRAL). La EVIDENCIA dispara mucho antes (desde 1 confiable); esto es solo
// el escalón de urgencia. Default 3, más temprano que el imminent_depth=6 actual.
constexpr uint8_t LEE_IMMINENT_COUNT = 3;

// "lifted": fracción de los sensores SANOS que tienen que estar en blanco para
// declarar levantado/imposible. 7/8 = 87.5 %, igual criterio que lf_all_white.
// Es físicamente imposible que una franja de línea de cancha encienda casi todo
// el anillo a la vez → si pasa, el robot está en el aire o la calib está rota:
// se INVALIDA la lectura (no se usa para escapar).
constexpr uint8_t LEE_LIFTED_NUM = 7;   // numerador
constexpr uint8_t LEE_LIFTED_DEN = 8;   // denominador

// === Entrada y salida ===

// Posición física de un sensor (mm). Misma convención que SensorPos2D de
// sensor_geometry.h. Se redefine acá (no se incluye sensor_geometry.h) para que
// el módulo sea autónomo y el caller pueda pasar la geometría que quiera.
struct LeeSensorPos {
    float x_mm;   // +X = derecha del robot
    float y_mm;   // +Y = adelante del robot
};

// Configuración (toda con defaults razonables). Permite titular en banco sin
// recompilar la lógica.
struct LeeConfig {
    uint16_t hysteresis_band = LEE_HYSTERESIS_BAND;
    uint8_t  imminent_count  = LEE_IMMINENT_COUNT;
    uint8_t  lifted_num      = LEE_LIFTED_NUM;
    uint8_t  lifted_den      = LEE_LIFTED_DEN;
};

struct LeeResult {
    bool    line_evidence;    // hay al menos UN sensor blanco confiable
    bool    imminent;         // N >= imminent_count blancos confiables
    float   escape_angle_deg; // dirección para alejarse de la línea (opuesto al
                              //   centroide). 0 = frente. Válido solo si line_evidence.
    uint8_t confidence;       // 0..100 (escala con cuántos confiables hay)
    bool    lifted;           // ~todos los sanos en blanco → lectura INVÁLIDA
};

// === Implementación PURA (inline, header-only) ===

// Decide si un sensor está "en blanco" con histéresis sobre SU umbral individual.
// Réplica de lf_hysteresis_on_white (line_filters.h:55) para no depender de él:
//   - si venía en blanco, sigue en blanco mientras raw >= umbral - banda;
//   - si venía en carpeta, pasa a blanco solo cuando raw >= umbral + banda.
inline bool lee_hysteresis_white(uint16_t raw, uint16_t threshold,
                                 uint16_t band, bool was_white) {
    // Cálculo saturado de las bandas (evita wrap de uint16 cerca de 0 / 65535).
    const int32_t hi = (int32_t)threshold + (int32_t)band;
    const int32_t lo = (int32_t)threshold - (int32_t)band;
    const int32_t v  = (int32_t)raw;
    if (was_white) return v >= lo;   // ya blanco: aguanta hasta caer bajo la banda baja
    return v >= hi;                  // venía carpeta: necesita superar la banda alta
}

// Calcula la detección temprana + el vector de escape.
//
// Parámetros (todos arrays de n elementos, n <= LEE_MAX_SENSORS):
//   raw         lecturas crudas del ADC por sensor.
//   threshold   umbral calibrado INDIVIDUAL por sensor (de line_calib / EEPROM).
//   healthy     flag de salud por sensor (de sensor_health): true = usable.
//   pos         posición física (x,y) de cada sensor (mm).
//   prev_white  estado blanco del tick anterior por sensor, para la histéresis.
//               Puede ser nullptr en el primer tick (se asume todo en carpeta).
//   n           cantidad de sensores.
//   cfg         parámetros de sintonía.
//
// IMPORTANTE: `prev_white` se ACTUALIZA in-place con el estado crudo-con-histéresis
// de ESTE tick (antes del filtro espacial), para que el próximo tick tenga memoria.
// Pasalo como un buffer persistente del caller (bool[n]). Si es nullptr, el módulo
// igual funciona pero sin memoria temporal (histéresis arranca de cero cada vez).
inline LeeResult lee_compute(const uint16_t* raw,
                             const uint16_t* threshold,
                             const bool* healthy,
                             const LeeSensorPos* pos,
                             bool* prev_white,
                             int n,
                             const LeeConfig& cfg = LeeConfig{}) {
    LeeResult out{};
    out.escape_angle_deg = 0.0f;

    // Guardas defensivas: sin datos válidos no hay evidencia ni escape.
    if (raw == nullptr || threshold == nullptr || healthy == nullptr ||
        pos == nullptr || n <= 0) {
        return out;
    }
    if (n > LEE_MAX_SENSORS) n = LEE_MAX_SENSORS;

    // --- Paso 1: blanco crudo con histéresis, SOLO en sensores sanos ---
    // Un sensor enfermo (muerto/clavado/ruidoso) NUNCA cuenta como blanco: se
    // excluye antes de la geometría (tolerancia a fallas, el "súper confiable").
    bool white_raw[LEE_MAX_SENSORS];
    for (int i = 0; i < n; ++i) {
        const bool was = (prev_white != nullptr) ? prev_white[i] : false;
        bool w = false;
        if (healthy[i]) {
            w = lee_hysteresis_white(raw[i], threshold[i], cfg.hysteresis_band, was);
        }
        white_raw[i] = w;
        if (prev_white != nullptr) prev_white[i] = w;  // memoria para el próximo tick
    }

    // --- Paso 2: filtro espacial — un blanco aislado NO cuenta ---
    // Un sensor confiable necesita un VECINO (i-1 o i+1 en el anillo cerrado)
    // que también esté blanco. Así un único sensor loco que cruzó su umbral no
    // dispara nada ni sesga el centroide. Mismo criterio que lf_spatial_filter
    // (line_filters.h:68), con el wrap del anillo (sensor 0 vecino del n-1).
    bool white_ok[LEE_MAX_SENSORS];
    for (int i = 0; i < n; ++i) {
        if (!white_raw[i]) { white_ok[i] = false; continue; }
        const int prev = (i == 0) ? (n - 1) : (i - 1);
        const int next = (i == n - 1) ? 0 : (i + 1);
        white_ok[i] = (white_raw[prev] || white_raw[next]);
    }

    // --- Paso 3: contar sanos y confiables; detectar lifted ---
    int healthy_count = 0;
    int confident_count = 0;
    for (int i = 0; i < n; ++i) {
        if (healthy[i]) ++healthy_count;
        if (white_ok[i]) ++confident_count;
    }

    // "lifted": casi TODOS los sensores SANOS leen blanco a la vez. Físicamente
    // imposible para una franja de línea → robot levantado o calib rota. Se basa
    // en el blanco CRUDO (white_raw), no en el espacial: si todo el anillo está
    // blanco, todos tienen vecino igual, así que ambos criterios coinciden, pero
    // contar crudo es lo correcto conceptualmente (saturación, no geometría).
    if (healthy_count > 0) {
        int white_among_healthy = 0;
        for (int i = 0; i < n; ++i) {
            if (healthy[i] && white_raw[i]) ++white_among_healthy;
        }
        // white_among_healthy / healthy_count >= num/den  (en enteros).
        const long lhs = (long)white_among_healthy * (long)cfg.lifted_den;
        const long rhs = (long)healthy_count * (long)cfg.lifted_num;
        if (lhs >= rhs) {
            out.lifted = true;
            // Lectura inválida: NO reportamos evidencia ni escape (todo a 0/false).
            out.line_evidence = false;
            out.imminent = false;
            out.confidence = 0;
            out.escape_angle_deg = 0.0f;
            return out;
        }
    }

    // --- Paso 4: evidencia, inminencia y vector de escape ---
    if (confident_count <= 0) {
        // Ningún blanco confiable → sin evidencia. (Un blanco aislado sin vecino
        // cae acá: confident_count == 0.)
        return out;
    }

    out.line_evidence = true;                                  // desde el 1er confiable
    out.imminent = (confident_count >= (int)cfg.imminent_count);

    // Centroide CARTESIANO de los blancos confiables (posición real, igual que
    // lg_compute_xy). El escape es el OPUESTO de ese centroide.
    float sum_x = 0.0f, sum_y = 0.0f;
    for (int i = 0; i < n; ++i) {
        if (!white_ok[i]) continue;
        sum_x += pos[i].x_mm;
        sum_y += pos[i].y_mm;
    }

    // Dirección de la línea (hacia dónde está el blanco) en la convención del
    // firmware: 0°=frente=+Y, horario positivo, +90°=+X (derecha). Eso es
    // atan2(x, y) (NO atan2(y,x)), idéntico a sg_angle_deg/line_geometry.
    const float RAD2DEG = 57.29577951308232f;  // 180/π
    const float line_angle_deg = atan2f(sum_x, sum_y) * RAD2DEG;

    // Escape = opuesto. Normalizado a (-180, +180].
    float esc = line_angle_deg + 180.0f;
    while (esc > 180.0f)  esc -= 360.0f;
    while (esc <= -180.0f) esc += 360.0f;
    out.escape_angle_deg = esc;

    // Confianza 0..100: escala con cuántos sensores confiables hay, saturando en
    // imminent_count (a partir de ahí ya estamos seguros). Mínimo 1 confiable da
    // algo de confianza, no cero, porque YA es evidencia válida (tiene vecino).
    const int denom = (cfg.imminent_count > 0) ? (int)cfg.imminent_count : 1;
    int conf = (confident_count * 100) / denom;
    if (conf > 100) conf = 100;
    if (conf < 1)   conf = 1;
    out.confidence = (uint8_t)conf;

    return out;
}

}  // namespace iitasoccer
