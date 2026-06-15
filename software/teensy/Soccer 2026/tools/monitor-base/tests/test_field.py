"""Tests de la geometría PURA de gui_field.py (vista de cancha). Sin Tk."""
from __future__ import annotations

from monitor_base.gui_field import (Trail, field_to_px, robot_rel_to_field)


# ── robot_rel_to_field ──────────────────────────────────────────────────────
def test_rel_to_field_heading0_front_is_plus_y():
    # robot en (910,1000), heading 0 (frente = +Y), pelota 500mm AL FRENTE.
    x, y = robot_rel_to_field(0, 500, 910, 1000, 0.0)
    assert abs(x - 910) < 1e-6
    assert abs(y - 1500) < 1e-6


def test_rel_to_field_heading0_right_is_plus_x():
    x, y = robot_rel_to_field(500, 0, 910, 1000, 0.0)
    assert abs(x - 1410) < 1e-6
    assert abs(y - 1000) < 1e-6


def test_rel_to_field_heading90_front_is_plus_x():
    # heading 90 (CW desde +Y) → el frente apunta a +X.
    x, y = robot_rel_to_field(0, 500, 910, 1000, 90.0)
    assert abs(x - 1410) < 1e-6
    assert abs(y - 1000) < 1e-6


def test_rel_to_field_heading90_right_is_minus_y():
    x, y = robot_rel_to_field(500, 0, 910, 1000, 90.0)
    assert abs(x - 910) < 1e-6
    assert abs(y - 500) < 1e-6


# ── field_to_px ─────────────────────────────────────────────────────────────
def test_field_to_px_origin_is_bottom_left():
    px, py = field_to_px(0, 0, draw_w=182, draw_h=243, margin=10,
                         field_w=1820, field_h=2430)
    assert abs(px - 10) < 1e-6                 # X=0 → izquierda
    assert abs(py - (10 + 243)) < 1e-6         # Y=0 → ABAJO (Y invertido)


def test_field_to_px_farcorner_is_top_right():
    px, py = field_to_px(1820, 2430, draw_w=182, draw_h=243, margin=10,
                         field_w=1820, field_h=2430)
    assert abs(px - (10 + 182)) < 1e-6         # X=ancho → derecha
    assert abs(py - 10) < 1e-6                 # Y=largo → ARRIBA


# ── Trail (estela) ──────────────────────────────────────────────────────────
def test_trail_push_and_len():
    t = Trail(maxlen=5)
    t.push(0, 0); t.push(100, 0); t.push(200, 0)
    assert len(t) == 3
    assert t.points()[0] == (0, 0)


def test_trail_skips_near_duplicate():
    t = Trail()
    t.push(0, 0)
    t.push(1, 1)          # < 2mm de movimiento → no se agrega
    assert len(t) == 1
    t.push(50, 0)         # se mueve → se agrega
    assert len(t) == 2


def test_trail_maxlen_bounded():
    t = Trail(maxlen=3)
    for i in range(10):
        t.push(i * 100, 0)
    assert len(t) == 3
    assert t.points()[-1] == (900, 0)      # conserva las últimas


def test_trail_clear():
    t = Trail()
    t.push(0, 0); t.push(100, 0)
    t.clear()
    assert len(t) == 0
