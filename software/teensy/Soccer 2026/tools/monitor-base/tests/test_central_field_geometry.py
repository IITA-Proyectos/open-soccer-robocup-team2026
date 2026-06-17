"""Tests de la geometría PURA del panel de cancha de la CENTRAL (sin Tk).

Convención (CONVENCION-EJES-ROBOT.md, canónica del repo):
  • Marco robot: +X = DERECHA del robot, +Y = FRENTE del robot.
  • Marco cancha: +X = DERECHA mirando al arco rival, +Y = hacia el arco rival.
  • heading 0° → el robot mira a +Y (al arco rival).
  • heading creciente = CCW (antihorario, visto desde arriba).
"""
from __future__ import annotations

import math

import pytest

from monitor_base.central_field_geometry import (
    ball_velocity_vector_in_field,
    cmd_vector_in_field,
    polar_to_field,
    rotate_robot_frame_to_field,
)

EPS = 1e-6


# ── rotate_robot_frame_to_field ─────────────────────────────────────────────
def test_rotate_heading_zero_is_identity_for_front():
    # Vector "al frente" del robot (rel_y=+1, rel_x=0) con heading=0 → +Y cancha.
    dx, dy = rotate_robot_frame_to_field(0.0, 100.0, 0.0)
    assert abs(dx - 0.0) < EPS
    assert abs(dy - 100.0) < EPS


def test_rotate_heading_zero_right_stays_right():
    # Vector "a la derecha" del robot (rel_x=+1) con heading=0 → +X cancha.
    dx, dy = rotate_robot_frame_to_field(100.0, 0.0, 0.0)
    assert abs(dx - 100.0) < EPS
    assert abs(dy - 0.0) < EPS


def test_rotate_heading_90_ccw_front_goes_to_minus_x():
    # CCW+: girar 90° a la izquierda → el frente del robot apunta a -X cancha.
    dx, dy = rotate_robot_frame_to_field(0.0, 100.0, 90.0)
    assert abs(dx - (-100.0)) < EPS
    assert abs(dy - 0.0) < EPS


def test_rotate_heading_90_ccw_right_goes_to_plus_y():
    # heading=90° CCW: la "derecha del robot" (+rel_x) apunta a +Y cancha.
    dx, dy = rotate_robot_frame_to_field(100.0, 0.0, 90.0)
    assert abs(dx - 0.0) < EPS
    assert abs(dy - 100.0) < EPS


def test_rotate_heading_180_flips_front():
    dx, dy = rotate_robot_frame_to_field(0.0, 100.0, 180.0)
    assert abs(dx - 0.0) < EPS
    assert abs(dy - (-100.0)) < EPS


def test_rotate_heading_minus_90_cw_front_goes_to_plus_x():
    # heading=-90° (= 90° CW) → el frente apunta a +X cancha.
    dx, dy = rotate_robot_frame_to_field(0.0, 100.0, -90.0)
    assert abs(dx - 100.0) < EPS
    assert abs(dy - 0.0) < EPS


# ── polar_to_field ──────────────────────────────────────────────────────────
def test_polar_to_field_own_goal_in_front_when_robot_at_own_area():
    # Robot en (910, 200) mirando al arco rival (heading=0). Arco PROPIO atrás
    # (ang=180° rel al robot) a 200 mm → cae en (910, 0).
    x, y = polar_to_field(robot_x=910.0, robot_y=200.0, robot_heading_deg=0.0,
                          angle_deg=180.0, dist_mm=200.0)
    assert abs(x - 910.0) < 1e-3
    assert abs(y - 0.0) < 1e-3


def test_polar_to_field_opp_goal_in_front():
    # Robot en (910, 200), heading=0; arco rival al frente (ang=0°) a 2110 mm
    # → cae en (910, 2310) (boca del arco rival).
    x, y = polar_to_field(robot_x=910.0, robot_y=200.0, robot_heading_deg=0.0,
                          angle_deg=0.0, dist_mm=2110.0)
    assert abs(x - 910.0) < 1e-3
    assert abs(y - 2310.0) < 1e-3


def test_polar_to_field_with_heading_rotates_target():
    # Robot en (910, 1215) (centro de cancha) con heading=90° CCW (mira a -X).
    # Un objeto al frente del robot (ang=0°) a 500 mm cae en (410, 1215).
    x, y = polar_to_field(robot_x=910.0, robot_y=1215.0,
                          robot_heading_deg=90.0,
                          angle_deg=0.0, dist_mm=500.0)
    assert abs(x - 410.0) < 1e-3
    assert abs(y - 1215.0) < 1e-3


def test_polar_to_field_angle_sign_matches_robot_frame():
    # Convención del repo: ángulo de un objeto = atan2(x, y) con x primero
    # (+ang = a la DERECHA del robot, ojo crece CW). Robot en origen mirando
    # +Y con heading=0: ang=+90° dist=100 cae a la derecha del robot (+X).
    x, y = polar_to_field(robot_x=0.0, robot_y=0.0, robot_heading_deg=0.0,
                          angle_deg=90.0, dist_mm=100.0)
    assert abs(x - 100.0) < 1e-3
    assert abs(y - 0.0) < 1e-3


# ── cmd_vector_in_field ─────────────────────────────────────────────────────
def test_cmd_vector_heading_zero_is_identity_scaled():
    # Comando vy=+200 mm/s (al frente) con heading=0 → +Y cancha, escalado.
    dx, dy = cmd_vector_in_field(robot_heading_deg=0.0, cmd_vx=0.0, cmd_vy=200.0,
                                 scale=0.5)
    assert abs(dx - 0.0) < EPS
    assert abs(dy - 100.0) < EPS


def test_cmd_vector_default_scale():
    dx, dy = cmd_vector_in_field(robot_heading_deg=0.0, cmd_vx=100.0, cmd_vy=0.0)
    # Con scale por defecto 0.5: 100 → 50 px.
    assert abs(dx - 50.0) < EPS
    assert abs(dy - 0.0) < EPS


def test_cmd_vector_heading_90_ccw_rotates():
    # vy=+200 con heading=90° CCW → vector apunta a -X cancha (escalado).
    dx, dy = cmd_vector_in_field(robot_heading_deg=90.0, cmd_vx=0.0, cmd_vy=200.0,
                                 scale=1.0)
    assert abs(dx - (-200.0)) < EPS
    assert abs(dy - 0.0) < EPS


def test_cmd_vector_zero_command_zero_vector():
    dx, dy = cmd_vector_in_field(robot_heading_deg=45.0, cmd_vx=0.0, cmd_vy=0.0)
    assert abs(dx) < EPS
    assert abs(dy) < EPS


# ── ball_velocity_vector_in_field ───────────────────────────────────────────
def test_ball_velocity_heading_zero_is_identity_scaled():
    # Pelota moviéndose al frente del robot (vy=+200 mm/s) con heading=0 →
    # +Y cancha, escalado por scale (0.15 px/mm/s) → 30 px, bajo el clamp.
    dx, dy = ball_velocity_vector_in_field(robot_heading_deg=0.0, ball_vx=0.0,
                                           ball_vy=200.0, scale=0.15)
    assert abs(dx - 0.0) < EPS
    assert abs(dy - 30.0) < EPS


def test_ball_velocity_heading_90_ccw_rotates():
    # vy=+200 con heading=90° CCW → apunta a -X cancha (escalado, bajo clamp).
    dx, dy = ball_velocity_vector_in_field(robot_heading_deg=90.0, ball_vx=0.0,
                                           ball_vy=200.0, scale=0.15)
    assert abs(dx - (-30.0)) < EPS
    assert abs(dy - 0.0) < EPS


def test_ball_velocity_clamps_to_max_len_preserving_direction():
    # Velocidad enorme (5000 mm/s al frente) con scale 0.15 → 750 px sin clamp.
    # Con max_len=90 debe recortarse a 90 px, dirección intacta (+Y cancha).
    dx, dy = ball_velocity_vector_in_field(robot_heading_deg=0.0, ball_vx=0.0,
                                           ball_vy=5000.0, scale=0.15,
                                           max_len=90.0)
    assert abs(math.hypot(dx, dy) - 90.0) < 1e-3
    assert dy > 0.0 and abs(dx) < EPS


def test_ball_velocity_zero_velocity_zero_vector():
    dx, dy = ball_velocity_vector_in_field(robot_heading_deg=33.0, ball_vx=0.0,
                                           ball_vy=0.0)
    assert abs(dx) < EPS
    assert abs(dy) < EPS
