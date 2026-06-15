"""Tests de la geometría PURA de gui_field.py (vista de cancha). Sin Tk."""
from __future__ import annotations

from monitor_base.gui_field import (Trail, fade_palette, field_to_px, lerp_hex,
                                    pose_gap_mm, robot_rel_to_field)


def test_pose_gap_mm():
    assert pose_gap_mm(0, 0, 3, 4) == 5.0          # 3-4-5
    assert pose_gap_mm(10, 10, 10, 10) == 0.0
    assert abs(pose_gap_mm(0, 0, 0, 100) - 100.0) < 1e-9


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


# ── Estela atenuada: lerp_hex + fade_palette (parte pura del cometa) ─────────
def test_lerp_hex_endpoints():
    assert lerp_hex("#000000", "#ffffff", 0.0) == "#000000"
    assert lerp_hex("#000000", "#ffffff", 1.0) == "#ffffff"


def test_lerp_hex_midpoint():
    assert lerp_hex("#000000", "#ffffff", 0.5) == "#7f7f7f"


def test_lerp_hex_clamps_out_of_range():
    assert lerp_hex("#000000", "#ffffff", -1.0) == "#000000"
    assert lerp_hex("#000000", "#ffffff", 2.0) == "#ffffff"


def test_lerp_hex_accepts_shorthand():
    # '#0f0' == '#00ff00'; a t=1 devuelve el destino expandido.
    assert lerp_hex("#000", "#0f0", 1.0) == "#00ff00"


def test_fade_palette_oldest_is_bg_newest_is_fg():
    pal = fade_palette(10, "#000000", "#ffffff")
    assert pal[0] == "#000000"          # más viejo = fondo (desaparece)
    assert pal[-1] == "#ffffff"         # más nuevo = color pleno (cabeza)
    assert len(pal) == 10


def test_fade_palette_is_monotonic_brightness():
    # con gamma>0 el brillo crece del viejo al nuevo (cola que se apaga).
    pal = fade_palette(8, "#000000", "#ffffff", gamma=2.2)
    vals = [int(c[1:3], 16) for c in pal]
    assert vals == sorted(vals)
    assert vals[0] <= vals[-1]


def test_fade_palette_degenerate_sizes():
    assert fade_palette(0, "#000000", "#ffffff") == []
    assert fade_palette(1, "#000000", "#ffffff") == ["#ffffff"]
