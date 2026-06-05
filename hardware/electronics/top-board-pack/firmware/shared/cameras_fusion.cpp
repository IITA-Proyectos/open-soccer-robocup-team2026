// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#include "cameras_fusion.h"
#include <cmath>

namespace iitasoccer {

namespace {
constexpr float PI_F = 3.14159265358979323846f;

// Confianza ficticia para el protocolo viejo (no envía área de blob).
// Cuando migremos al protocolo nuevo, esto vendrá del propio packet.
constexpr float CONF_SINGLE_CAMERA = 80.0f;
constexpr float CONF_CONSENSUS     = 95.0f;  // bonus cuando 2 cámaras coinciden
}  // namespace

CamObs cam_obs_to_robot_frame(int16_t x_raw,
                              int16_t y_raw,
                              bool visible,
                              uint8_t cam_id,
                              float unit_to_mm) {
    CamObs o{};
    o.camera_id = cam_id;
    o.visible = visible;
    o.x_mm = static_cast<float>(x_raw) * unit_to_mm;
    o.y_mm = static_cast<float>(y_raw) * unit_to_mm;
    if (cam_id == 1) {
        // Cámara trasera: el "frente" de la cámara apunta al "atrás" del robot.
        // Rotación 180° en el plano XY → invertir signo de ambas coords.
        o.x_mm = -o.x_mm;
        o.y_mm = -o.y_mm;
    }
    return o;
}

BallFused fuse_ball_dual(const CamObs& front,
                         const CamObs& back,
                         bool front_alive,
                         bool back_alive) {
    BallFused out{};

    const bool f_ok = front_alive && front.visible;
    const bool b_ok = back_alive  && back.visible;

    if (f_ok && b_ok) {
        // Promedio ponderado por confianza. Como el protocolo viejo no manda
        // confianza, las pesamos igual (CONF_SINGLE_CAMERA cada una).
        const float w_f = CONF_SINGLE_CAMERA;
        const float w_b = CONF_SINGLE_CAMERA;
        out.x_mm = static_cast<int16_t>((front.x_mm * w_f + back.x_mm * w_b)
                                        / (w_f + w_b));
        out.y_mm = static_cast<int16_t>((front.y_mm * w_f + back.y_mm * w_b)
                                        / (w_f + w_b));
        out.confidence = static_cast<uint8_t>(CONF_CONSENSUS);
        out.visible = true;
    } else if (f_ok) {
        out.x_mm = static_cast<int16_t>(front.x_mm);
        out.y_mm = static_cast<int16_t>(front.y_mm);
        out.confidence = static_cast<uint8_t>(CONF_SINGLE_CAMERA);
        out.visible = true;
    } else if (b_ok) {
        out.x_mm = static_cast<int16_t>(back.x_mm);
        out.y_mm = static_cast<int16_t>(back.y_mm);
        out.confidence = static_cast<uint8_t>(CONF_SINGLE_CAMERA);
        out.visible = true;
    } else {
        out.visible = false;
        out.confidence = 0;
        out.x_mm = 0;
        out.y_mm = 0;
    }
    return out;
}

GoalFused fuse_goal_dual(const CamObs& front,
                        const CamObs& back,
                        bool front_alive,
                        bool back_alive) {
    GoalFused out{};
    const bool f_ok = front_alive && front.visible;
    const bool b_ok = back_alive  && back.visible;

    float x = 0.0f, y = 0.0f;
    if (f_ok && b_ok) {
        x = (front.x_mm + back.x_mm) * 0.5f;
        y = (front.y_mm + back.y_mm) * 0.5f;
        out.visible = true;
    } else if (f_ok) {
        x = front.x_mm;
        y = front.y_mm;
        out.visible = true;
    } else if (b_ok) {
        x = back.x_mm;
        y = back.y_mm;
        out.visible = true;
    }

    if (out.visible) {
        // Convención: atan2(x, y) con +y = frente del robot, +x = lateral.
        // Devolvemos centideg para encajar en el campo de WorldSnapshot.
        const float angle_rad = std::atan2(x, y);
        out.angle_centideg = static_cast<int16_t>(angle_rad * (18000.0f / PI_F));
        out.distance_mm = static_cast<int16_t>(std::sqrt(x * x + y * y));
    } else {
        out.angle_centideg = 0;
        out.distance_mm = 0;
    }
    return out;
}

}  // namespace iitasoccer
