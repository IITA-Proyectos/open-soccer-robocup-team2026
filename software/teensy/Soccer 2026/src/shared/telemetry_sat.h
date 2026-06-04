// telemetry_sat.h — Saturación de float → int16 para serialización de telemetría.
//
// Por qué existe (audit 2026-06-03 #14)
// -------------------------------------
// comm_top.cpp casteaba `otos_get_omega_rad_s() * (18000/π)` a int16_t SIN clamp.
// Factor = 5729.6 centideg/s por rad/s → umbral de overflow ~5.72 rad/s. Un omni
// rota fácil a 8–15 rad/s al alinear; el cast crudo desborda int16 y wrappea de
// signo → "gira a la derecha rápido" se reporta como velocidad angular IZQUIERDA.
// Este helper satura (clamp) ANTES del cast, espejando el patrón que ya existe en
// ball_velocity.cpp (clamp_to_i16), line_view.h (lsv2_penetration_u8) y el propio
// slip_estimate de comm_top. Es una función pura → host-testeable sin Arduino.
//
// NaN/inf: el cast directo de NaN/inf a int16 es UB; este helper los mapea a un
// extremo seguro (NaN→0, +inf→32767, -inf→-32768) para no propagar basura.

#pragma once
#include <stdint.h>
#include <cmath>

namespace iitasoccer {

// Satura un float a [-32768, 32767] con redondeo simétrico y manejo de NaN/inf.
inline int16_t sat_i16(float v) {
    if (std::isnan(v)) return 0;
    if (v >= 32767.0f)  return 32767;
    if (v <= -32768.0f) return -32768;
    return static_cast<int16_t>(v >= 0.0f ? v + 0.5f : v - 0.5f);  // redondeo simétrico
}

// Conversión rad/s → centideg/s SATURADA (el único campo de telemetría DOWN que
// físicamente puede desbordar int16). 1 rad/s = 18000/π centideg/s ≈ 5729.58.
inline int16_t omega_rad_s_to_centideg_s_sat(float omega_rad_s) {
    return sat_i16(omega_rad_s * (18000.0f / 3.14159265358979323846f));
}

}  // namespace iitasoccer
