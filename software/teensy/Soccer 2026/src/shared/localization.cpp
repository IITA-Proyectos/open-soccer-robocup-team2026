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
// IMPORTANTE: esto asume que el BNO da heading positivo = giro CCW (sentido
// antihorario visto desde arriba). Si el BNO del proyecto da heading CW
// positivo, hay que invertir el signo en la suma.
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

    // Acumular estimaciones de X y de Y por separado.
    int32_t sum_x = 0; int sum_x_count = 0;
    int32_t sum_y = 0; int sum_y_count = 0;

    // Convertir heading de centideg a deg (sin float).
    int16_t heading_deg = (in.bno_heading_centideg - cfg.bno_offset_centideg) / 100;

    for (int i = 0; i < 4; ++i) {
        if (!in.tof_valid[i]) continue;
        uint16_t d = in.tof_distance_mm[i];
        // Descarte por rango: si la lectura es fisicamente imposible (mayor
        // que la dimension de la cancha en cualquier eje), descartar.
        const uint16_t max_dim = (cfg.field_width_mm > cfg.field_height_mm)
            ? cfg.field_width_mm : cfg.field_height_mm;
        if (d > max_dim) continue;
        // Descarte por minimo: el VL53L7CX no es fiable bajo 10 mm.
        if (d < 10) continue;
        Wall w = classify_wall(heading_deg, cfg.tof_mount_angle_deg[i]);

        switch (w) {
            case WALL_NORTH:
                sum_y += static_cast<int32_t>(cfg.field_height_mm) - d;
                ++sum_y_count;
                pose.source_flags |= (1 << i);
                break;
            case WALL_SOUTH:
                sum_y += d;
                ++sum_y_count;
                pose.source_flags |= (1 << i);
                break;
            case WALL_EAST:
                sum_x += static_cast<int32_t>(cfg.field_width_mm) - d;
                ++sum_x_count;
                pose.source_flags |= (1 << i);
                break;
            case WALL_WEST:
                sum_x += d;
                ++sum_x_count;
                pose.source_flags |= (1 << i);
                break;
            case WALL_NONE:
                break;
        }
    }

    if (sum_x_count > 0 && sum_y_count > 0) {
        pose.x_mm = static_cast<int16_t>(sum_x / sum_x_count);
        pose.y_mm = static_cast<int16_t>(sum_y / sum_y_count);
        pose.heading_centideg = in.bno_heading_centideg - cfg.bno_offset_centideg;
        pose.valid = true;
    }

    return pose;
}

}  // namespace iitasoccer
