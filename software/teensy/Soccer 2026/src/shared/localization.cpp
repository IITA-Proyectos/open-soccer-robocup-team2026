// localization.cpp — Trilateracion geometrica directa.
// Ver localization.h para la API publica y la spec para el algoritmo.

#include "localization.h"

namespace iitasoccer {

// Identificadores internos de las 4 paredes de la cancha.
enum Wall { WALL_NORTH, WALL_SOUTH, WALL_EAST, WALL_WEST, WALL_NONE };

namespace {

// Normaliza un angulo a [0, 360).
int normalize_angle_deg(int angle) {
    angle = angle % 360;
    if (angle < 0) angle += 360;
    return angle;
}

// ============================================================
// F1b: proyeccion por coseno (LUT entera Q12, sin <math.h> ni float).
// ============================================================
// Tabla de cos(deg)*4096 redondeado, deg en [0,90]. Q12 fixed-point.
// 91 entradas (~182 bytes flash). Monotona decreciente: cos(0)=4096, cos(90)=0.
// Se evita float para que el codigo sea identico host y target (Teensy 4.0),
// como pide la spec 4.3. El host no necesita libm para este path.
constexpr int16_t kCosQ12[91] = {
    4096,  4095,  4094,  4090,  4086,  4080,  4074,  4065,  4056,  4046,
    4034,  4021,  4006,  3991,  3974,  3956,  3937,  3917,  3896,  3873,
    3849,  3824,  3798,  3770,  3742,  3712,  3681,  3650,  3617,  3582,
    3547,  3511,  3474,  3435,  3396,  3355,  3314,  3271,  3228,  3183,
    3138,  3091,  3044,  2996,  2946,  2896,  2845,  2793,  2741,  2687,
    2633,  2578,  2522,  2465,  2408,  2349,  2290,  2231,  2171,  2110,
    2048,  1986,  1923,  1860,  1796,  1731,  1666,  1600,  1534,  1468,
    1401,  1334,  1266,  1198,  1129,  1060,   991,   921,   852,   782,
     711,   641,   570,   499,   428,   357,   286,   214,   143,    71,
        0
};

// cos(deg)*4096 (Q12), deg clampeado a [0,90].
int cos_q12(int deg) {
    if (deg < 0) deg = 0;
    if (deg > 90) deg = 90;
    return kCosQ12[deg];
}

// F1b: angulo agudo [0,45] entre el rayo del ToF (world) y la normal de la
// pared cardinal mas cercana. world_angle ya viene en [0,360) por
// normalize_angle_deg; el ((w%90)+90)%90 es defensivo para negativos.
int ray_to_wall_angle_deg(int world_angle_deg) {
    int a = ((world_angle_deg % 90) + 90) % 90;  // [0,90)
    if (a > 45) a = 90 - a;                       // plegar a [0,45]
    return a;
}

// F1a + F1b combinados: distancia perpendicular del CENTRO del robot a la pared.
//   d_meas      : lectura cruda del ToF (mm).
//   world_angle : heading + mount normalizado [0,360).
//   r           : tof_offset_mm (radio del robot; 0 desactiva F1a).
// El sensor apunta radialmente hacia afuera (mount = direccion del rayo), asi que
// su offset respecto al centro es r en la direccion del rayo. La componente
// perpendicular a la pared del rayo (d_meas) es d_meas*cos(alpha); la del offset
// r es r*cos(alpha). Distancia centro->pared = (d_meas + r) * cos(alpha).
int center_perp_distance_mm(int d_meas, int world_angle_deg, uint16_t r) {
    int alpha = ray_to_wall_angle_deg(world_angle_deg);
    int32_t projected = (int32_t)(d_meas + (int)r) * cos_q12(alpha);  // Q12
    return (int)((projected + 2048) / 4096);  // redondeo al entero mas cercano
}

// Clasifica a que pared apunta un TOF dado el angulo del robot (heading
// relativo a cancha) + angulo de montaje. La clasificacion usa cuadrantes:
//   [-45, 45) → NORTH (pared +Y)
//   [45, 135) → WEST  (pared -X) — porque +90 grados del robot apunta a su izq
//   [135, 225) → SOUTH (pared -Y)
//   [225, 315) → EAST  (pared +X)
//
// IMPORTANTE: esto asume heading positivo = giro CCW (antihorario visto desde
// arriba), o sea girar a la IZQUIERDA SUBE el heading.
// El BNO055 fisico de la placa entrega CW-positivo (medido en banco 2026-05-31,
// diag_top_bno: girar a la derecha -> +90). Esa inversion YA SE HACE EN LA
// FUENTE: sensors_imu.cpp aplica HEADING_SIGN = -1 antes de exponer el heading.
// => Cuando este codigo recibe robot_heading_deg, YA viene en convencion CCW.
// NO volver a invertir aca (seria doble inversion). Si algun dia el heading
// llega invertido, arreglar el signo en sensors_imu.cpp, no aca.
Wall classify_wall(int16_t robot_heading_deg, uint16_t mount_angle_deg) {
    int world_angle = normalize_angle_deg(
        static_cast<int>(robot_heading_deg) + static_cast<int>(mount_angle_deg)
    );

    if (world_angle < 45)  return WALL_NORTH;
    if (world_angle < 135) return WALL_WEST;
    if (world_angle < 225) return WALL_SOUTH;
    if (world_angle < 315) return WALL_EAST;
    return WALL_NORTH;  // 315..360 wraps to NORTH
}

}  // namespace

LocalizationPose localization_compute(
    const LocalizationInputs& in,
    const LocalizationConfig& cfg
) {
    LocalizationPose pose{};
    pose.valid = false;

    // Recolectar estimaciones por eje, con el indice del TOF que la genero.
    struct Estimate { int16_t value; int tof_idx; };
    Estimate x_estimates[4]; int x_count = 0;
    Estimate y_estimates[4]; int y_count = 0;

    int16_t heading_deg = (in.bno_heading_centideg - cfg.bno_offset_centideg) / 100;
    const uint16_t max_dim = (cfg.field_width_mm > cfg.field_height_mm)
        ? cfg.field_width_mm : cfg.field_height_mm;

    for (int i = 0; i < 4; ++i) {
        if (!in.tof_valid[i]) continue;
        uint16_t d = in.tof_distance_mm[i];
        // Filtro de rango sobre la d CRUDA (antes de F1a/F1b), para no romper
        // los tests de descarte existentes (lectura > cancha o < 10 mm).
        if (d > max_dim || d < 10) continue;

        // world_angle = direccion absoluta del rayo del ToF en cancha [0,360).
        // Es el mismo calculo que hace classify_wall internamente; lo
        // recomputamos aca (1 linea, barato) para no cambiar su firma.
        int world_angle = normalize_angle_deg(
            static_cast<int>(heading_deg) + static_cast<int>(cfg.tof_mount_angle_deg[i]));

        Wall w = classify_wall(heading_deg, cfg.tof_mount_angle_deg[i]);

        // F1a (+r, radio del robot) + F1b (*cos(alpha), proyeccion perpendicular)
        // combinados: distancia perpendicular del CENTRO del robot a la pared.
        int dperp = center_perp_distance_mm(static_cast<int>(d), world_angle, cfg.tof_offset_mm);

        switch (w) {
            case WALL_NORTH:
                y_estimates[y_count++] = { static_cast<int16_t>(cfg.field_height_mm - dperp), i };
                break;
            case WALL_SOUTH:
                y_estimates[y_count++] = { static_cast<int16_t>(dperp), i };
                break;
            case WALL_EAST:
                x_estimates[x_count++] = { static_cast<int16_t>(cfg.field_width_mm - dperp), i };
                break;
            case WALL_WEST:
                x_estimates[x_count++] = { static_cast<int16_t>(dperp), i };
                break;
            case WALL_NONE:
                break;
        }
    }

    // Outlier rejection por consistencia entre estimaciones del mismo eje.
    // Si 2 estimaciones difieren mas que el umbral, descartar la mas lejana
    // del pose anterior (si hay pose anterior valido).
    auto reject_outliers = [&](Estimate* arr, int& count, int16_t prev_value) {
        if (!cfg.prev_valid) return;
        // Loop hasta que no haya mas outliers. Cada iteracion del while
        // puede descartar a lo sumo uno; con max 4 estimaciones, terminan.
        bool changed = true;
        while (changed && count >= 2) {
            changed = false;
            for (int i = 0; i < count - 1 && !changed; ++i) {
                for (int j = i + 1; j < count && !changed; ++j) {
                    int diff = arr[i].value - arr[j].value;
                    if (diff < 0) diff = -diff;
                    if (diff > cfg.outlier_threshold_mm) {
                        // Descartar el mas lejano de prev_value.
                        int dist_i = arr[i].value - prev_value; if (dist_i < 0) dist_i = -dist_i;
                        int dist_j = arr[j].value - prev_value; if (dist_j < 0) dist_j = -dist_j;
                        int reject = (dist_i > dist_j) ? i : j;
                        // Shift left para eliminar el rechazado.
                        for (int k = reject; k < count - 1; ++k) {
                            arr[k] = arr[k + 1];
                        }
                        --count;
                        changed = true;  // reiniciar el escaneo.
                    }
                }
            }
        }
    };

    reject_outliers(x_estimates, x_count, cfg.prev_x_mm);
    reject_outliers(y_estimates, y_count, cfg.prev_y_mm);

    if (x_count > 0 && y_count > 0) {
        int32_t sum_x = 0; int32_t sum_y = 0;
        for (int i = 0; i < x_count; ++i) {
            sum_x += x_estimates[i].value;
            pose.source_flags |= (1 << x_estimates[i].tof_idx);
        }
        for (int i = 0; i < y_count; ++i) {
            sum_y += y_estimates[i].value;
            pose.source_flags |= (1 << y_estimates[i].tof_idx);
        }
        pose.x_mm = static_cast<int16_t>(sum_x / x_count);
        pose.y_mm = static_cast<int16_t>(sum_y / y_count);
        pose.heading_centideg = in.bno_heading_centideg - cfg.bno_offset_centideg;
        pose.valid = true;
    }

    return pose;
}

}  // namespace iitasoccer
