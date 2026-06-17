"""central_field_geometry.py — Geometría PURA del panel de cancha de la CENTRAL.

Sin Tk, sin dependencias externas — solo `math`. Testeable host-native.

Convención canónica del repo (docs/CONVENCION-EJES-ROBOT.md):
  • Marco ROBOT: +X = DERECHA del robot, +Y = FRENTE del robot.
  • Marco CANCHA: +Y crece hacia el arco RIVAL (lado largo, 2430 mm).
                  +X crece hacia la DERECHA mirando al arco rival (lado corto, 1820 mm).
  • heading 0° → el robot mira a +Y (arco rival).
  • heading creciente = CCW (antihorario, visto desde arriba).
  • Ángulo de un objeto relativo al robot: atan2(rel_x, rel_y) → +ang = a la
    DERECHA del robot (signo CRECE CW; es la convención del polar de las cámaras
    y del snapshot del TOP).

OJO: la versión "TOP" de la rotación que vive en gui_field.robot_rel_to_field
trata el heading como CW+ (legacy del radar). Acá se usa CCW+ (la canónica del
doc), por eso NO se reusa — son matemáticas distintas.
"""
from __future__ import annotations

import math
from typing import Tuple


def rotate_robot_frame_to_field(rel_x: float, rel_y: float,
                                heading_deg: float) -> Tuple[float, float]:
    """Rota un vector del marco ROBOT (+x=derecha, +y=frente) al marco CANCHA
    (+x=derecha mirando al arco rival, +y=al arco rival).

    Convención CCW+: heading creciente = giro antihorario visto desde arriba.
    Heading 0° → el frente del robot es +Y de cancha (identidad).

    Devuelve el DESPLAZAMIENTO en marco cancha (mm o cualquier unidad lineal:
    es una rotación, no compone traslación). Para componer con la pose del
    robot, sumar (robot_x + dx, robot_y + dy).
    """
    a = math.radians(heading_deg)
    c, s = math.cos(a), math.sin(a)
    # Rotación CCW+ por `a` aplicada al vector (rel_x, rel_y):
    #   [c -s] [rel_x]
    #   [s  c] [rel_y]
    dx = c * rel_x - s * rel_y
    dy = s * rel_x + c * rel_y
    return dx, dy


def polar_to_field(robot_x: float, robot_y: float, robot_heading_deg: float,
                   angle_deg: float, dist_mm: float) -> Tuple[float, float]:
    """Proyecta un punto POLAR visto por el robot (típicamente un arco) al marco
    cancha.

    `angle_deg` es el ángulo de la convención del repo: `atan2(rel_x, rel_y)`,
    es decir +ang = a la DERECHA del robot. Por eso:
      • angle_deg = 0   → el objeto está al FRENTE del robot (+rel_y).
      • angle_deg = +90 → el objeto está a la DERECHA del robot (+rel_x).
      • angle_deg = ±180 → el objeto está ATRÁS.

    Devuelve (x, y) en marco cancha (mm). Si dist_mm es 0 devuelve la pose del
    robot.
    """
    a = math.radians(angle_deg)
    # angle_deg = atan2(rel_x, rel_y) → rel_x = dist*sin(a), rel_y = dist*cos(a).
    rel_x = dist_mm * math.sin(a)
    rel_y = dist_mm * math.cos(a)
    dx, dy = rotate_robot_frame_to_field(rel_x, rel_y, robot_heading_deg)
    return robot_x + dx, robot_y + dy


def cmd_vector_in_field(robot_heading_deg: float, cmd_vx: float, cmd_vy: float,
                        scale: float = 0.5) -> Tuple[float, float]:
    """Vector de COMANDO (vx, vy en marco robot, mm/s) llevado al marco cancha
    y escalado para dibujarlo como flecha desde el robot.

    El comando vx/vy del CentralFrame.cmd está en marco ROBOT (vx=lateral
    derecha+, vy=frente+) — ver protocol_central.Cmd. Acá se rota al marco
    cancha y se escala para que quepa en el canvas (scale ~0.5 mm/s → px de
    vector en una cancha de ~500 px de alto).
    """
    dx, dy = rotate_robot_frame_to_field(cmd_vx, cmd_vy, robot_heading_deg)
    return dx * scale, dy * scale
