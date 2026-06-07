"""geometry.py — Posiciones físicas reales de los 32 sensores del anillo DOWN.

Espejo EXACTO de src/shared/sensor_geometry.cpp (validado empíricamente
2026-05-24). Se embebe acá para que la GUI dibuje el anillo fiel al PCB sin
depender del firmware.

Sistema de referencia (igual que el firmware):
  +X = derecha del robot, +Y = adelante del robot. Origen = centro del PCB.
  Ángulo: 0° = frente (+Y), horario positivo, rango (-180°, +180°].
"""
from __future__ import annotations

import math
from typing import List, Tuple

SENSOR_COUNT = 32

# (x_mm, y_mm) por sensor, índice 0-based (S1..S32 del PCB → 0..31).
SENSOR_POS: List[Tuple[float, float]] = [
    # S1-S8 — Mux U1 (anillo externo frontal)
    (-36.28, +82.04), (-30.57, +75.06), (-20.28, +74.17), (-10.12, +74.17),
    (+10.20, +74.17), (+20.36, +74.17), (+30.52, +75.18), (+36.49, +82.04),
    # S9-S16 — Mux U2 (externo izquierdo + trasero-izq)
    (-86.32, +12.19), (-86.32,  -0.51), (-84.92, -13.21), (-81.24, -26.04),
    (-67.65, -50.04), (-60.03, -60.20), (-48.98, -69.09), (-36.28, -76.71),
    # S17-S24 — Mux U3 (externo derecho + trasero-der)
    (+36.36, -76.71), (+49.31, -69.09), (+60.87, -60.20), (+69.63, -50.04),
    (+82.21, -25.91), (+85.64, -13.21), (+87.29,  -0.51), (+86.40, +12.06),
    # S25-S28 — Mux U4 (interno frontal, R≈54mm)
    (-22.82, +50.93), (-12.66, +50.93), (+12.74, +51.05), (+22.90, +50.93),
    # S29-S32 — Mux U4 (interno central/trasero, R≈37mm)
    (+34.55,  -9.51), (+34.55, -19.67), (-33.25,  -9.51), (-33.25, -19.80),
]

assert len(SENSOR_POS) == SENSOR_COUNT, "SENSOR_POS debe tener 32 entradas"


def angle_deg(i: int) -> float:
    """Ángulo polar del sensor i (0°=frente, horario+). Espejo de sg_angle_deg."""
    if i < 0 or i >= SENSOR_COUNT:
        return 0.0
    x, y = SENSOR_POS[i]
    if x == 0.0 and y == 0.0:
        return 0.0
    # atan2(x, y): 0 cuando (x=0,y>0)=frente; crece hacia +X (derecha) = horario+.
    ang = math.degrees(math.atan2(x, y))
    while ang > 180.0:
        ang -= 360.0
    while ang <= -180.0:
        ang += 360.0
    return ang


def radius_mm(i: int) -> float:
    """Radio del sensor i desde el centro (mm). Espejo de sg_radius_mm."""
    if i < 0 or i >= SENSOR_COUNT:
        return 0.0
    x, y = SENSOR_POS[i]
    return math.hypot(x, y)


def bounds() -> Tuple[float, float, float, float]:
    """(min_x, min_y, max_x, max_y) de las posiciones, para escalar la GUI."""
    xs = [p[0] for p in SENSOR_POS]
    ys = [p[1] for p in SENSOR_POS]
    return min(xs), min(ys), max(xs), max(ys)
