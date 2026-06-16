// robot_geometry.h — Geometría FÍSICA del robot: FUENTE ÚNICA de dónde está cada sensor
// respecto del CENTRO, su altura sobre el piso, y hacia dónde mira. PURO, host-testeable.
//
// Por qué (pedido Gustavo 2026-06-16, lente odometría/trigonometría)
// ----------------------------------------------------------------------------------
// Los sensores NO están en el centro del robot. Un ToF a ~6 cm del centro que mide d a
// una pared NO da la distancia del CENTRO: hay que corregir por el offset (que ROTA con
// el heading). Para que el cálculo de posición y el DIBUJO del monitor usen la MISMA
// geometría, vive en UN solo lugar: este header (firmware) + su espejo en el monitor
// (tools/monitor-base/monitor_base/robot_geometry.py) + el doc canónico
// docs/firmware/ROBOT-GEOMETRY.md. Si cambia el montaje, se edita ACÁ (y se espeja).
//
// Frame robot (docs/CONVENCION-EJES-ROBOT.md): origen = CENTRO; +X = derecha, +Y = frente,
// +heading = CCW (antihorario). heading 0 = el frente (+Y robot) mira al arco rival (+Y
// cancha). El offset de un sensor (marco robot) se lleva al marco cancha rotándolo por el
// heading: ver geom_rotate_offset().
//
// ⚠️ VALORES APROXIMADOS (Gustavo dijo "unos 6/7 cm", "unos 17 cm de diámetro", alturas
// "unos 12/17 cm"). Son una SEMILLA editable. Para la PRECISIÓN que se busca hay que
// MEDIR con calibre los offsets, las alturas y (Fase B) los ángulos/tilt reales. Mientras
// no se midan, la corrección es tan buena como estos números.
//
// PURO: solo <stdint.h> + <math.h>. Sin Arduino. Compila g++ host. Hoy es DECLARACIÓN +
// math host-testeada: el cableado a la trilateración (corrección offset→centro) es un paso
// aparte (gateado + banco); el monitor ya lo usa para el render a tamaño real.

#pragma once
#include <stdint.h>
#include <math.h>

namespace iitasoccer {

// Radio del robot (Ø ~170 mm). Para dibujar el robot a tamaño real y acotar offsets.
constexpr int16_t ROBOT_RADIUS_MM = 85;     // ⚠️ aprox (Ø ~17 cm) — medir

// Geometría de UN sensor en el marco ROBOT.
struct SensorGeom {
    int16_t off_x_mm;     // offset desde el centro, +X = derecha
    int16_t off_y_mm;     // offset desde el centro, +Y = frente
    int16_t height_mm;    // altura del sensor sobre el piso
    int16_t bearing_deg;  // hacia dónde mira: 0=frente, 90=derecha, 180=atrás, 270=izquierda
};

// --- 4 ToF (VL53L7CX). Mapeo CONVENCION-EJES: 0=frente,1=atrás,2=derecha,3=izquierda.
//     Offset ~60 mm a lo largo de su bearing; altura ~170 mm. ⚠️ MEDIR. ----------------
constexpr SensorGeom TOF_GEOM[4] = {
    {   0,  60, 170,   0 },   // TOF0 — frente
    {   0, -60, 170, 180 },   // TOF1 — atrás
    {  60,   0, 170,  90 },   // TOF2 — derecha
    { -60,   0, 170, 270 },   // TOF3 — izquierda (montado a 180°; ver tof_zone_orient.h)
};

// --- 2 cámaras OpenMV (front/back). Offset ~70 mm; altura ~120 mm. ⚠️ MEDIR. ----------
constexpr SensorGeom CAM_GEOM[2] = {
    {   0,  70, 120,   0 },   // cámara frontal
    {   0, -70, 120, 180 },   // cámara trasera
};

// --- HC-SR04 ultrasónico frontal. Offset ~70 mm. Altura no dada por el equipo → semilla
//     ~100 mm; ⚠️ MEDIR. ----------------------------------------------------------------
constexpr SensorGeom HCSR04_GEOM = { 0, 70, 100, 0 };

// geom_rotate_offset — lleva un offset del marco ROBOT al marco CANCHA, rotándolo por el
// heading. Rotación CCW estándar: (fx,fy) = (ox·cosθ − oy·sinθ, ox·sinθ + oy·cosθ).
// A heading 0 el marco robot == marco cancha (front=+Y) → (fx,fy)=(ox,oy).
// `heading_rad` en radianes, CCW+ (mismo signo que el heading del WorldSnapshot).
inline void geom_rotate_offset(float ox, float oy, float heading_rad, float& fx, float& fy) {
    const float c = cosf(heading_rad);
    const float s = sinf(heading_rad);
    fx = ox * c - oy * s;
    fy = ox * s + oy * c;
}

// geom_center_dist_to_wall — corrige la distancia MEDIDA por el sensor a la distancia del
// CENTRO del robot a esa pared.
//   sensor_dist_mm : lo que mide el sensor (perpendicular a la pared, asumido)
//   off_fx, off_fy : el offset del sensor YA rotado al marco cancha (geom_rotate_offset)
//   nx, ny         : normal INTERIOR de la pared (unitaria, apunta de la pared hacia
//                    adentro de la cancha)
// Derivación: dist(P) crece en +n̂ (hacia adentro). sensor = centro + Δ →
//   sensor_dist = center_dist + Δ·n̂  ⇒  center_dist = sensor_dist − Δ·n̂.
// Ej.: ToF frontal Δ=(0,+60) mira al arco rival (+Y); su pared (al frente) tiene n̂=(0,−1)
//   (apunta hacia adentro = −Y). Δ·n̂ = −60 → center_dist = sensor_dist + 60 (el centro
//   está 60 mm MÁS LEJOS de la pared que el sensor adelantado). Correcto.
// ⚠️ El signo de n̂ debe casar con la convención de paredes de localization.cpp al cablear.
inline float geom_center_dist_to_wall(float sensor_dist_mm, float off_fx, float off_fy,
                                      float nx, float ny) {
    return sensor_dist_mm - (off_fx * nx + off_fy * ny);
}

}  // namespace iitasoccer
