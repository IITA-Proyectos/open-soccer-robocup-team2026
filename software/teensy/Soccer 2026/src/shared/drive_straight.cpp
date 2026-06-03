// drive_straight.cpp — implementación PURA (sin Arduino). Ver drive_straight.h
// para la convención de signos y la ley de control completas.

#include "drive_straight.h"

namespace iitasoccer {

// angdiff a [-180, +180] — idéntico a pids.h::wrap_diff_deg pero local para que
// el módulo no dependa del link order. Resta y normaliza por el camino corto.
float drive_straight_angdiff_deg(float a, float b) {
    float d = a - b;
    while (d > 180.0f)  d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

DriveStraightCmd drive_straight_compute(const DriveStraightIn& in,
                                        const DriveStraightCfg& cfg) {
    DriveStraightCmd cmd;

    // Heading: P puro sobre el error normalizado. error>0 (target adelante en
    // CCW) -> omega>0 -> gira CCW para alcanzar el target (reduce el error).
    const float heading_err = drive_straight_angdiff_deg(in.target_heading_deg,
                                                         in.cur_heading_deg);
    cmd.omega_deg_s = cfg.kp_heading * heading_err;

    // Lateral: cancela la deriva medida por OTOS. Signo opuesto a otos_vy.
    cmd.vy_mm_s = -cfg.kp_lateral * in.otos_vy_mm_s;

    // Avance: se respeta tal cual (la corrección no lo altera).
    cmd.vx_mm_s = in.fwd_speed_mm_s;

    return cmd;
}

}  // namespace iitasoccer
