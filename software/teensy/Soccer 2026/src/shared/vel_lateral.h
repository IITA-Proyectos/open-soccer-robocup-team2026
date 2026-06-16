// vel_lateral.h — componente LATERAL (strafe) de la velocidad, para el gate anti-strafe
// de la cámara en la cross-validación del heading (TASK-213). PURO, host-testeable.
//
// Por qué (review adversarial de TASK-212): la cámara mide d(bearing_arco)/dt como proxy
// de la rotación. Pero si el arquero TRASLADA (strafe) con el arco cerca, ese d(bearing)/dt
// está dominado por la traslación, no por la rotación → cross-validar con eso da un
// FALSO-VETO del BNO sano. Solución: invalidar la ref de cámara cuando |velocidad lateral|
// no es ~0. Este helper da esa velocidad lateral desde la velocidad de la OTOS.
//
// MATEMÁTICA (des-rotación por −heading): dada (vx,vy) y el heading (centideg), proyecta
// a lateral/forward:
//   lateral = −vx·sin(θ) + vy·cos(θ)
//   forward =  vx·cos(θ) + vy·sin(θ)
// PROPIEDAD CLAVE: con heading=0 → lateral==vy, forward==vx (IDENTIDAD).
//
// ⚠️ ASUNCIÓN DE MARCO (a confirmar en banco — TASK-213 pre-req 4): si el OTOS entrega la
// velocidad en marco CANCHA, llamar con el heading real; si la entrega en marco ROBOT,
// llamar con heading=0 (identidad → lateral=vy). `types.h` NO documenta el marco de
// Velocity2D, así que el cableado lo gatea con OTOS_VEL_FRAME_IS_FIELD (default OFF =
// identidad = hipótesis conservadora de odometría). La matemática de acá es correcta para
// AMBOS marcos; cuál entrega el OTOS es dato de banco, no se decide en este módulo.
//
// PURO: <math.h> sinf/cosf + enteros, sin Arduino. Mismo patrón que line_early_escape.h.

#pragma once
#include <stdint.h>
#include <math.h>

namespace iitasoccer {

struct VelLateral {
    int16_t lateral_mm_s;   // componente de strafe (eje lateral)
    int16_t forward_mm_s;   // componente de avance
};

namespace vel_lateral_detail {
// Saturación + redondeo a int16 (sin UB del cast directo de un float fuera de rango).
inline int16_t clamp_i16(float v) {
    if (v >=  32767.0f) return  32767;
    if (v <= -32768.0f) return -32768;
    return static_cast<int16_t>(v >= 0.0f ? v + 0.5f : v - 0.5f);
}
}  // namespace vel_lateral_detail

// Des-rota (vx,vy) por −heading → {lateral, forward}. heading=0 ⇒ {vy, vx} (identidad).
inline VelLateral vel_lateral_from_otos(int16_t vx_mm_s, int16_t vy_mm_s,
                                        int16_t heading_centideg) {
    const float rad = static_cast<float>(heading_centideg) * 3.14159265358979f / 18000.0f;
    const float s = sinf(rad);
    const float c = cosf(rad);
    const float vx = static_cast<float>(vx_mm_s);
    const float vy = static_cast<float>(vy_mm_s);
    VelLateral out;
    out.lateral_mm_s = vel_lateral_detail::clamp_i16(-vx * s + vy * c);
    out.forward_mm_s = vel_lateral_detail::clamp_i16( vx * c + vy * s);
    return out;
}

// ¿El robot está trasladando lateralmente más que el umbral? Corte ESTRICTO (>): el borde
// |lateral|==thresh NO cuenta como strafe. El caller usa esto para invalidar la cámara.
inline bool vel_lateral_is_strafing(int16_t lateral_mm_s, int16_t thresh_mm_s) {
    const int32_t a = lateral_mm_s < 0 ? -static_cast<int32_t>(lateral_mm_s)
                                       :  static_cast<int32_t>(lateral_mm_s);
    return a > static_cast<int32_t>(thresh_mm_s);
}

}  // namespace iitasoccer
