// sensor_geometry.cpp — Definición de SENSOR_POS[32] + helpers.
// Ver sensor_geometry.h para el contrato. Fuente canónica de las coords:
// hardware/electronics/down-board-pack/01-pinout-y-posiciones.md §5b.

#include "sensor_geometry.h"
#include <cmath>

namespace iitasoccer {

const SensorPos2D SENSOR_POS[SENSOR_COUNT] = {
    // S1-S8 — Mux U1 (anillo externo frontal, Y ≈ +75-82 mm)
    {-36.28f, +82.04f},  // S1   F1
    {-30.57f, +75.06f},  // S2   F2
    {-20.28f, +74.17f},  // S3   F3
    {-10.12f, +74.17f},  // S4   F4
    {+10.20f, +74.17f},  // S5   F5
    {+20.36f, +74.17f},  // S6   F6
    {+30.52f, +75.18f},  // S7   F7
    {+36.49f, +82.04f},  // S8   F8

    // S9-S16 — Mux U2 (anillo externo izquierdo + trasero-izq, X ≈ -81 a -86)
    {-86.32f, +12.19f},  // S9   F9
    {-86.32f,  -0.51f},  // S10  F10
    {-84.92f, -13.21f},  // S11  F11
    {-81.24f, -26.04f},  // S12  F12
    {-67.65f, -50.04f},  // S13  F13
    {-60.03f, -60.20f},  // S14  F14
    {-48.98f, -69.09f},  // S15  F15
    {-36.28f, -76.71f},  // S16  F16

    // S17-S24 — Mux U3 (anillo externo derecho + trasero-der, X ≈ +36 a +87)
    {+36.36f, -76.71f},  // S17  F17
    {+49.31f, -69.09f},  // S18  F18
    {+60.87f, -60.20f},  // S19  F19
    {+69.63f, -50.04f},  // S20  F20
    {+82.21f, -25.91f},  // S21  F21
    {+85.64f, -13.21f},  // S22  F22
    {+87.29f,  -0.51f},  // S23  F23
    {+86.40f, +12.06f},  // S24  F24

    // S25-S28 — Mux U4 ch3/0/1/2 (anillo interno frontal, R≈54mm, Y ≈ +51)
    {-22.82f, +50.93f},  // S25  F25
    {-12.66f, +50.93f},  // S26  F26
    {+12.74f, +51.05f},  // S27  F27
    {+22.90f, +50.93f},  // S28  F28

    // S29-S32 — Mux U4 ch5/7/6/4 (anillo interno central/trasero, R≈37mm)
    {+34.55f,  -9.51f},  // S29  F29
    {+34.55f, -19.67f},  // S30  F30
    {-33.25f,  -9.51f},  // S31  F31
    {-33.25f, -19.80f},  // S32  F32
};

float sg_angle_deg(int i) {
    if (i < 0 || i >= SENSOR_COUNT) return 0.0f;
    const float x = SENSOR_POS[i].x_mm;
    const float y = SENSOR_POS[i].y_mm;
    if (x == 0.0f && y == 0.0f) return 0.0f;
    // atan2(x, y) — NO el atan2(y, x) habitual — porque queremos:
    //   ángulo = 0 cuando (x=0, y>0)   → frente
    //   ángulo > 0 cuando rotamos hacia +X (derecha) → horario positivo
    // atan2(x, y) cumple exactamente eso: es el ángulo del vector (x,y)
    // respecto al eje +Y, midiendo positivo hacia +X.
    float ang = std::atan2(x, y) * 180.0f / (float)M_PI;
    // Normalización a (-180°, +180°] por las dudas (atan2 ya devuelve [-π, π]).
    while (ang > 180.0f)   ang -= 360.0f;
    while (ang <= -180.0f) ang += 360.0f;
    return ang;
}

float sg_radius_mm(int i) {
    if (i < 0 || i >= SENSOR_COUNT) return 0.0f;
    const float x = SENSOR_POS[i].x_mm;
    const float y = SENSOR_POS[i].y_mm;
    return std::sqrt(x*x + y*y);
}

void sg_fill_angles_deg(float* out, int n) {
    if (!out) return;
    for (int i = 0; i < n; ++i) {
        out[i] = (i < SENSOR_COUNT) ? sg_angle_deg(i) : 0.0f;
    }
}

}  // namespace iitasoccer
