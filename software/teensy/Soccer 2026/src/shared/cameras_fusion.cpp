#include "cameras_fusion.h"
#include <cmath>

namespace iitasoccer {

namespace {
constexpr float PI_F = 3.14159265358979323846f;

// Confianza ficticia: el packet cámara→TOP v2 no transporta área de blob, así
// que la fusión asigna una confianza fija. Esta capa es agnóstica al wire (las
// coords llegan ya decodificadas v2); si un futuro packet trae el área, vendrá
// del propio packet en vez de esta constante.
constexpr float CONF_SINGLE_CAMERA = 80.0f;
constexpr float CONF_CONSENSUS     = 95.0f;  // bonus cuando 2 cámaras coinciden

// SB-4 (eval 2026-06-04): clamp del cast float→int16 en la fusión.
// HOY es no-op: el parser da ~[-128,127] units * CAMERA_UNIT_TO_MM (10) =>
// ~[-1280,1280] mm/eje (dist <~1810), muy adentro de int16. Pero si se sube
// UNIT_TO_MM al recalibrar la homografía (TASK-022) el cast crudo desbordaría
// int16 y wrappearía de signo (un arco a la derecha lejano se reportaría a la
// IZQUIERDA), igual que el bug de omega que motivó telemetry_sat.h.
//
// OJO: usamos TRUNCAMIENTO (no el redondeo simétrico de sat_i16) a propósito:
// el código previo era `static_cast<int16_t>(float)` (trunca hacia 0), y este
// cambio debe ser BIT-IDÉNTICO en el rango de hoy para no mover el gate host
// verde (no debe cambiar ningún test de test_cameras_fusion). Solo agregamos la
// saturación en los extremos; el valor en rango queda exactamente igual.
inline int16_t clamp_to_i16_trunc(float v) {
    if (v >= 32767.0f)  return 32767;
    if (v <= -32768.0f) return -32768;
    return static_cast<int16_t>(v);  // trunca hacia 0, idéntico al cast previo
}
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
        // Sin confianza por cámara en el protocolo, el "promedio ponderado" con
        // pesos iguales ES una media simple (SB-3, eval 2026-06-04): la
        // escribimos así — numéricamente idéntica y consistente con fuse_goal_dual.
        out.x_mm = clamp_to_i16_trunc((front.x_mm + back.x_mm) * 0.5f);
        out.y_mm = clamp_to_i16_trunc((front.y_mm + back.y_mm) * 0.5f);
        out.confidence = static_cast<uint8_t>(CONF_CONSENSUS);
        out.visible = true;
    } else if (f_ok) {
        out.x_mm = clamp_to_i16_trunc(front.x_mm);
        out.y_mm = clamp_to_i16_trunc(front.y_mm);
        out.confidence = static_cast<uint8_t>(CONF_SINGLE_CAMERA);
        out.visible = true;
    } else if (b_ok) {
        out.x_mm = clamp_to_i16_trunc(back.x_mm);
        out.y_mm = clamp_to_i16_trunc(back.y_mm);
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
        // Convención: atan2(x, y) con +y = frente del robot, +x = DERECHA.
        // => +90° = arco a la derecha del robot. Ver docs/CONVENCION-EJES-ROBOT.md.
        // Devolvemos centideg para encajar en el campo de WorldSnapshot.
        const float angle_rad = std::atan2(x, y);
        // angle ∈ [-180,180]° => centideg ∈ [-18000,18000]: nunca desborda (el
        // clamp es defensivo). distance sí podría con UNIT_TO_MM grande.
        out.angle_centideg = clamp_to_i16_trunc(angle_rad * (18000.0f / PI_F));
        out.distance_mm = clamp_to_i16_trunc(std::sqrt(x * x + y * y));
    } else {
        out.angle_centideg = 0;
        out.distance_mm = 0;
    }
    return out;
}

}  // namespace iitasoccer
