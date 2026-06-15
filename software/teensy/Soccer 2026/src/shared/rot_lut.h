// rot_lut.h — sin/cos en Q12 (×4096) de CÍRCULO COMPLETO + rotación de vectores 2D
// en ENTEROS. PURO, host-testeable (sin Arduino, sin <cmath>/libm).
//
// Por qué existe (H1 / bug del estimador, 2026-06-15)
// ---------------------------------------------------
// `pose_fusion` integra el DELTA de la OTOS sumándolo CRUDO a la pose fusionada
// (`st.x_mm_q0 += dx`). Pero el delta de la OTOS está en el marco DE LA OTOS (su
// propio mundo, fijado al resetear), que NO es el marco de la cancha. Para integrarlo
// bien hay que ROTAR el delta por el ángulo entre los dos marcos (= heading del BNO −
// heading de la OTOS) antes de sumarlo. Sin esto, al GIRAR el robot la pose fusionada
// se va en la dirección equivocada. Este módulo da la rotación entera necesaria.
//
// Por qué una LUT NUEVA y no la de localization.cpp: `localization.cpp::kCosQ12[91]`
// solo cubre cos en [0,90] CLAMPEADO (no envuelve, no tiene seno, no maneja cuadrantes).
// Acá se necesita sin/cos de 0..359 con signos. Se embebe la misma tabla base de
// cos[0..90] (valores matemáticos idénticos) para que el módulo sea STANDALONE y PURO;
// los demás cuadrantes y el seno salen por identidades (sin duplicar la lógica).
//
// CONVENCIÓN: rotación CCW-positiva (igual que heading/gyro del firmware). El SIGNO
// físico final (si el net de rotación es bno−otos o al revés) se valida en BANCO —
// acá la MATEMÁTICA de rotar un vector por un ángulo es la que se testea host.
//
// RANGO: pensado para vectores chicos (deltas de pose, ≤ ~cientos de mm). x*4096 debe
// caber en int32 → |x|,|y| ≲ 500000. Los deltas de OTOS (clamp ~80 mm) entran de sobra.

#pragma once
#include <stdint.h>

namespace iitasoccer {

namespace rot_lut_detail {
// cos(d)·4096 (Q12) para d en [0,90]. Espejo EXACTO de localization.cpp::kCosQ12
// (constantes matemáticas; embebidas para mantener el módulo puro y standalone).
inline int32_t cos0_90_q12(int d) {
    static const int16_t T[91] = {
        4096, 4095, 4094, 4090, 4086, 4080, 4074, 4065, 4056, 4046,
        4034, 4021, 4006, 3991, 3974, 3956, 3937, 3917, 3896, 3873,
        3849, 3824, 3798, 3770, 3742, 3712, 3681, 3650, 3617, 3582,
        3547, 3511, 3474, 3435, 3396, 3355, 3314, 3271, 3228, 3183,
        3138, 3091, 3044, 2996, 2946, 2896, 2845, 2793, 2741, 2687,
        2633, 2578, 2522, 2465, 2408, 2349, 2290, 2231, 2171, 2110,
        2048, 1986, 1923, 1860, 1796, 1731, 1666, 1600, 1534, 1468,
        1401, 1334, 1266, 1198, 1129, 1060,  991,  921,  852,  782,
         711,  641,  570,  499,  428,  357,  286,  214,  143,   71,
           0
    };
    if (d < 0) d = 0;
    if (d > 90) d = 90;
    return T[d];
}
}  // namespace rot_lut_detail

// Normaliza grados a [0,360).
inline int rot_norm360(int deg) {
    deg %= 360;
    if (deg < 0) deg += 360;
    return deg;
}

// cos(deg)·4096 (Q12), círculo completo, por reflexión de cuadrantes.
inline int32_t rot_cos_q12(int deg) {
    deg = rot_norm360(deg);
    if (deg <= 90)  return  rot_lut_detail::cos0_90_q12(deg);          // Q1
    if (deg <= 180) return -rot_lut_detail::cos0_90_q12(180 - deg);    // Q2: cos(d)=-cos(180-d)
    if (deg <= 270) return -rot_lut_detail::cos0_90_q12(deg - 180);    // Q3: cos(d)=-cos(d-180)
    return                  rot_lut_detail::cos0_90_q12(360 - deg);    // Q4: cos(d)= cos(360-d)
}

// sin(deg)·4096 (Q12) = cos(90−deg).
inline int32_t rot_sin_q12(int deg) {
    return rot_cos_q12(90 - deg);
}

// Rota el vector (x,y) por `deg` grados CCW, en enteros Q12 con REDONDEO al más
// cercano (minimiza el sesgo del truncamiento /4096 al acumular muchos deltas):
//   [ox]   [cos −sin][x]
//   [oy] = [sin  cos][y]
inline void rot_rotate_q12(int32_t x, int32_t y, int deg, int32_t& ox, int32_t& oy) {
    const int32_t c = rot_cos_q12(deg);
    const int32_t s = rot_sin_q12(deg);
    const int32_t nx = x * c - y * s;
    const int32_t ny = x * s + y * c;
    // Redondeo simétrico: +2048 si ≥0, −2048 si <0, antes de /4096.
    ox = (nx >= 0 ? nx + 2048 : nx - 2048) / 4096;
    oy = (ny >= 0 ? ny + 2048 : ny - 2048) / 4096;
}

}  // namespace iitasoccer
