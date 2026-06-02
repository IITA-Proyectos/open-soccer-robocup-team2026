// pose_view.h — interpretación pura de Pose2D / Velocity2D (DOWN → CENTRAL/TOP).
//
// Espejo de line_view.h: helpers SIN estado y SIN Arduino, compartidos entre el
// firmware y los tests host-native. Pose2D/Velocity2D NO tienen schema_version,
// así que se valida solo tipo + tamaño exacto (un payload del tamaño/tipo
// equivocado se RECHAZA en vez de reinterpretarse como basura).
#pragma once
#include <stdint.h>
#include <string.h>   // memcpy
#include "types.h"
#include "proto.h"

namespace iitasoccer {

inline bool pose_from_frame(const Frame& f, Pose2D& out) {
    if (f.type != MsgType::DOWN_OTOS_POSE) return false;
    if (f.payload_len != sizeof(Pose2D)) return false;
    memcpy(&out, f.payload, sizeof(Pose2D));
    return true;
}

inline bool vel_from_frame(const Frame& f, Velocity2D& out) {
    if (f.type != MsgType::DOWN_OTOS_VEL) return false;
    if (f.payload_len != sizeof(Velocity2D)) return false;
    memcpy(&out, f.payload, sizeof(Velocity2D));
    return true;
}

}  // namespace iitasoccer
