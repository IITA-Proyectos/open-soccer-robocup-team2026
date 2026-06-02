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
        if (d > max_dim || d < 10) continue;

        Wall w = classify_wall(heading_deg, cfg.tof_mount_angle_deg[i]);
        switch (w) {
            case WALL_NORTH:
                y_estimates[y_count++] = { static_cast<int16_t>(cfg.field_height_mm - d), i };
                break;
            case WALL_SOUTH:
                y_estimates[y_count++] = { static_cast<int16_t>(d), i };
                break;
            case WALL_EAST:
                x_estimates[x_count++] = { static_cast<int16_t>(cfg.field_width_mm - d), i };
                break;
            case WALL_WEST:
                x_estimates[x_count++] = { static_cast<int16_t>(d), i };
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
