// localization.cpp — Trilateracion geometrica directa.
// Ver localization.h para la API publica y la spec para el algoritmo.

#include "localization.h"

namespace iitasoccer {

// Identificadores internos de las 4 paredes de la cancha.
enum Wall { WALL_NORTH, WALL_SOUTH, WALL_EAST, WALL_WEST, WALL_NONE };

namespace {

// Clasifica a que pared apunta un TOF dado su angulo en el frame mundo
// (heading + angulo de montaje). Usa convencion:
//   +Y = norte (al arco rival)
//   +X = este (lateral derecho)
//   angulo 0 = mira a +Y (norte)
//   angulo 90 = mira a +X (este)  ⚠ ojo: convencion robotic usa +Y como front,
//                                      pero el robot que rota CCW va a -X. Hay que
//                                      tener cuidado con el sentido de rotacion del BNO.
// Por ahora: implementacion minima para el caso heading=0, no rotacion.
Wall classify_wall_simple(uint16_t mount_angle_deg) {
    // Mapeo directo cuando heading=0 (no rotacion):
    //   angulo 0   → frontal → mira +Y → pared NORTH
    //   angulo 180 → trasero → mira -Y → pared SOUTH
    //   angulo 90  → izq → mira -X → pared WEST  (izquierda del robot apuntando +Y)
    //   angulo 270 → der → mira +X → pared EAST  (derecha del robot apuntando +Y)
    if (mount_angle_deg == 0)   return WALL_NORTH;
    if (mount_angle_deg == 180) return WALL_SOUTH;
    if (mount_angle_deg == 90)  return WALL_WEST;
    if (mount_angle_deg == 270) return WALL_EAST;
    return WALL_NONE;
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

    for (int i = 0; i < 4; ++i) {
        if (!in.tof_valid[i]) continue;
        uint16_t d = in.tof_distance_mm[i];
        Wall w = classify_wall_simple(cfg.tof_mount_angle_deg[i]);

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
