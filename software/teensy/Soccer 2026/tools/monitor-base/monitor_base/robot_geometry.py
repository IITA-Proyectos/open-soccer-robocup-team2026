"""robot_geometry.py — Geometría física de los sensores de la placa TOP (ToF / cámaras /
ultrasónico) respecto del CENTRO del robot. ESPEJO de src/shared/robot_geometry.h.

Misma idea que geometry.py (anillo DOWN): los números físicos viven en UNA fuente (el
header del firmware + este espejo + docs/firmware/ROBOT-GEOMETRY.md). El monitor lo usa
para dibujar el robot a TAMAÑO REAL y poner el origen de cada rayo en la ubicación REAL
del sensor (no en el centro).

Frame robot (CONVENCION-EJES): origen = centro; +X = derecha, +Y = frente; +heading = CCW.
heading 0 = frente mira al arco rival (+Y cancha).

⚠️ VALORES APROXIMADOS (Gustavo: "unos 6/7 cm", Ø "unos 17 cm", alturas "12/17 cm"). Para
precisión: MEDIR con calibre. Si cambian, editar el header del firmware Y este espejo igual
(test de paridad en tests/test_robot_geometry.py).
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from typing import List


ROBOT_RADIUS_MM = 85          # Ø ~170 mm (aprox) — medir


@dataclass(frozen=True)
class SensorGeom:
    off_x_mm: float           # +X = derecha
    off_y_mm: float           # +Y = frente
    height_mm: float          # altura sobre el piso
    bearing_deg: float        # 0=frente, 90=derecha, 180=atrás, 270=izquierda
    label: str = ""


# Mismo orden/valores que TOF_GEOM[] del header (0=frente,1=atrás,2=derecha,3=izquierda).
TOF_GEOM: List[SensorGeom] = [
    SensorGeom(0.0,  60.0, 170.0,   0.0, "ToF0 frente"),
    SensorGeom(0.0, -60.0, 170.0, 180.0, "ToF1 atrás"),
    SensorGeom(60.0,  0.0, 170.0,  90.0, "ToF2 derecha"),
    SensorGeom(-60.0, 0.0, 170.0, 270.0, "ToF3 izquierda"),
]

CAM_GEOM: List[SensorGeom] = [
    SensorGeom(0.0,  70.0, 120.0,   0.0, "Cam frontal"),
    SensorGeom(0.0, -70.0, 120.0, 180.0, "Cam trasera"),
]

HCSR04_GEOM = SensorGeom(0.0, 70.0, 100.0, 0.0, "HC-SR04")


def rotate_offset(off_x: float, off_y: float, heading_rad: float) -> tuple[float, float]:
    """Lleva un offset del marco ROBOT al marco CANCHA rotándolo por el heading (CCW+).
    Espejo EXACTO de geom_rotate_offset() del header."""
    c = math.cos(heading_rad)
    s = math.sin(heading_rad)
    return (off_x * c - off_y * s, off_x * s + off_y * c)


def sensor_field_pos(g: SensorGeom, center_x: float, center_y: float,
                     heading_rad: float) -> tuple[float, float]:
    """Posición (x,y) del sensor en marco cancha, dado el centro del robot y el heading.
    Para dibujar el origen del rayo en la ubicación REAL del sensor."""
    dx, dy = rotate_offset(g.off_x_mm, g.off_y_mm, heading_rad)
    return (center_x + dx, center_y + dy)


# ── Diagrama top-down ESTÁTICO (marco del robot, heading=0) ─────────────────
# Layout PURO (en px) para dibujar el robot a TAMAÑO REAL con cada sensor en su
# posición física + la dirección de su rayo (bearing). En el marco PROPIO del robot
# (heading=0) → NO usa el heading vivo, así que ESQUIVA el conflicto de convención
# CCW/CW. Pantalla: +X (derecha) → +px ; +Y (frente) → ARRIBA (−py).
def topdown_layout(canvas_w: float, canvas_h: float, margin_px: float = 16.0) -> dict:
    reach_mm = ROBOT_RADIUS_MM + 60.0          # robot + holgura para el stub/etiqueta del rayo
    span_px = min(canvas_w, canvas_h) - 2.0 * margin_px
    scale = (span_px / (2.0 * reach_mm)) if (reach_mm > 0 and span_px > 0) else 1.0
    cx, cy = canvas_w / 2.0, canvas_h / 2.0

    def place(g: SensorGeom, kind: str) -> dict:
        px = cx + g.off_x_mm * scale
        py = cy - g.off_y_mm * scale           # Y invertido en pantalla
        b = math.radians(g.bearing_deg)
        # frente(b=0) → arriba (0,−1) ; derecha(b=90) → +x (1,0)
        return {"label": g.label, "kind": kind, "px": px, "py": py,
                "ray_dx": math.sin(b), "ray_dy": -math.cos(b), "height_mm": g.height_mm}

    sensors = ([place(g, "tof") for g in TOF_GEOM]
               + [place(g, "cam") for g in CAM_GEOM]
               + [place(HCSR04_GEOM, "us")])
    return {"scale": scale, "cx": cx, "cy": cy,
            "robot_radius_px": ROBOT_RADIUS_MM * scale, "sensors": sensors}
