// otos_fusion.h — Matemática PURA de fusión de los 2 OTOS (host-testeable).
//
// Por qué existe
// --------------
// otos.cpp (wrapper Arduino) no se puede testear host: depende de Wire/SparkFun.
// Esta unidad extrae la matemática de fusión dual a funciones puras (sin Arduino)
// para poder cubrirla con tests host y arreglar dos bugs del audit 2026-06-03:
//
//   #7  Heading dual: otos.cpp hacía atan2(dy_acumulado, separación), que sólo
//       aproxima el heading para giros chicos y satura en ±90° (un giro real de
//       120° reportaba <90°), descartando el heading absoluto que la IMU del OTOS
//       ya integra. Fix: promedio VECTORIAL de los dos headings de IMU
//       (atan2(senL+senR, cosL+cosR)) → correcto y monótono en todo [-180,180].
//
//   #20 slip_estimate: otos.cpp hacía |x_der − x_izq| sobre POSICIONES acumuladas
//       (mm), que crece monótono por drift aunque no haya patinazo, viola el
//       contrato (types.h: "mm/s … más allá de lo esperable por la rotación") y no
//       resta la componente de rotación. Fix: trabajar sobre VELOCIDADES y restar
//       |ω|·half_separation, con un solo modelo de rotación (ω de las IMUs).

#pragma once
#include <cmath>

namespace iitasoccer {

// Fusión de heading dual-OTOS por promedio vectorial (maneja wraparound ±180).
// Entrada/salida en grados. Promediar 170 y -170 da +180 (no 0, como haría un
// promedio aritmético crudo). Si los dos vectores se cancelan exactamente
// (headings opuestos a 180°), atan2(0,0)=0 — caso degenerado sin solución única.
inline float fuse_dual_heading_deg(float left_h_deg, float right_h_deg) {
    constexpr float DEG2RAD = 3.14159265358979323846f / 180.0f;
    constexpr float RAD2DEG = 180.0f / 3.14159265358979323846f;
    const float lr = left_h_deg * DEG2RAD;
    const float rr = right_h_deg * DEG2RAD;
    const float s = std::sin(lr) + std::sin(rr);
    const float c = std::cos(lr) + std::cos(rr);
    return std::atan2(s, c) * RAD2DEG;
}

// Estima el slip lateral (mm/s) entre los 2 OTOS más allá de lo esperable por la
// rotación pura. Trabaja sobre VELOCIDADES (no posiciones) y resta la componente
// de rotación |ω|·half_sep. Nunca negativo (clamp a 0): el slip es una magnitud.
//   left_vx_mm_s / right_vx_mm_s : velocidad X de cada OTOS (marco robot, mm/s).
//   omega_rad_s                  : velocidad angular del robot (rad/s).
//   half_separation_mm           : media separación entre OTOS (OTOS_SEPARATION_MM/2).
// En rotación pura, la diferencia esperada de vx entre dos puntos separados por
// `sep` es ω·sep, así que |vR.x − vL.x| ≈ |ω|·sep y el exceso (= slip real) ~0.
inline float otos_slip_estimate(float left_vx_mm_s, float right_vx_mm_s,
                                float omega_rad_s, float half_separation_mm) {
    const float observed = std::fabs(right_vx_mm_s - left_vx_mm_s);
    const float expected = std::fabs(omega_rad_s) * (2.0f * half_separation_mm);
    const float excess = observed - expected;
    return excess > 0.0f ? excess : 0.0f;
}

}  // namespace iitasoccer
