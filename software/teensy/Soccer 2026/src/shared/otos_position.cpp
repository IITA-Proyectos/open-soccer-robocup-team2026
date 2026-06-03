// otos_position.cpp — controlador de posición/distancia por odometría OTOS.
// Implementación PURA (solo <math.h>); contrato definido en otos_position.h
// y verificado por test/test_otos_position/test_main.cpp.
#include "otos_position.h"
#include <math.h>

namespace iitasoccer {

namespace {

constexpr float kDegToRad = (float)(M_PI / 180.0);

inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

}  // namespace

OtosMoveParams otos_move_default_params() {
    OtosMoveParams p;
    p.max_speed_mm_s = 600.0f;
    p.decel_zone_mm  = 300.0f;
    p.arrive_tol_mm  = 15.0f;
    p.min_speed_mm_s = 60.0f;
    return p;
}

void otos_move_set_target(OtosMoveTarget& t,
                          float cur_x_mm, float cur_y_mm, float cur_heading_deg,
                          float dx_robot_mm, float dy_robot_mm) {
    // target_world = cur_pos + R(cur_heading) * (dx, dy)_robot.
    // R(h) = [[cos h, -sin h], [sin h, cos h]] (rotación CCW estándar).
    const float h   = cur_heading_deg * kDegToRad;
    const float ch  = cosf(h);
    const float sh  = sinf(h);
    const float wdx = ch * dx_robot_mm - sh * dy_robot_mm;
    const float wdy = sh * dx_robot_mm + ch * dy_robot_mm;
    t.tx_world_mm = cur_x_mm + wdx;
    t.ty_world_mm = cur_y_mm + wdy;
    t.active = true;
}

void otos_move_cancel(OtosMoveTarget& t) {
    t.active = false;
}

OtosMoveCmd otos_move_update(const OtosMoveTarget& t, const OtosMoveParams& p,
                             float cur_x_mm, float cur_y_mm, float cur_heading_deg) {
    OtosMoveCmd c;
    c.vx_mm_s = 0.0f;
    c.vy_mm_s = 0.0f;
    c.arrived = false;
    c.remaining_mm = 0.0f;

    // Error en marco MUNDO y distancia restante.
    const float ex_world = t.tx_world_mm - cur_x_mm;
    const float ey_world = t.ty_world_mm - cur_y_mm;
    const float remaining = sqrtf(ex_world * ex_world + ey_world * ey_world);
    c.remaining_mm = remaining;

    // Llegada (también cubre target inactivo + tol no nula sólo si remaining<tol;
    // si está inactivo el caller no debería llamar, pero degradamos a "quieto").
    if (remaining < p.arrive_tol_mm) {
        c.arrived = true;
        return c;
    }

    // Error en marco ROBOT: R(-heading) * err_world.
    // R(-h) = [[cos h, sin h], [-sin h, cos h]].
    const float h  = cur_heading_deg * kDegToRad;
    const float ch = cosf(h);
    const float sh = sinf(h);
    const float ex_robot =  ch * ex_world + sh * ey_world;   // componente derecha (+X)
    const float ey_robot = -sh * ex_world + ch * ey_world;   // componente adelante (+Y)

    // Rampa lineal de frenado + clamp a [min, max].
    float speed = p.max_speed_mm_s;
    if (p.decel_zone_mm > 0.0f) {
        speed = p.max_speed_mm_s * remaining / p.decel_zone_mm;
    }
    speed = clampf(speed, p.min_speed_mm_s, p.max_speed_mm_s);

    // Reparte la velocidad sobre el versor del error en marco robot.
    c.vx_mm_s = speed * ex_robot / remaining;
    c.vy_mm_s = speed * ey_robot / remaining;
    return c;
}

}  // namespace iitasoccer
