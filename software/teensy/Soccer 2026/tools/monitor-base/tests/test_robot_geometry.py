"""Tests de robot_geometry.py — espejo de la geometría del firmware (src/shared/
robot_geometry.h). Verifica que los números y la math de rotación coincidan con el header
(paridad documentada) y los casos canónicos de posición de sensor."""
from __future__ import annotations

import math

from monitor_base.robot_geometry import (
    ROBOT_RADIUS_MM, TOF_GEOM, CAM_GEOM, HCSR04_GEOM,
    rotate_offset, sensor_field_pos, topdown_layout,
)


# ── Paridad de constantes con el header del firmware ─────────────────────────
def test_constants_match_firmware():
    assert ROBOT_RADIUS_MM == 85
    # 4 ToF: ~60 mm del centro, h 170 (mismos valores que TOF_GEOM[] del header).
    assert len(TOF_GEOM) == 4
    for g in TOF_GEOM:
        r = math.hypot(g.off_x_mm, g.off_y_mm)
        assert abs(r - 60.0) < 1e-6
        assert r < ROBOT_RADIUS_MM
        assert g.height_mm == 170.0
    assert [g.bearing_deg for g in TOF_GEOM] == [0, 180, 90, 270]
    # 2 cámaras: ~70 mm, h 120.
    assert len(CAM_GEOM) == 2
    for g in CAM_GEOM:
        assert math.isclose(math.hypot(g.off_x_mm, g.off_y_mm), 70.0)
        assert g.height_mm == 120.0
    assert HCSR04_GEOM.height_mm == 100.0


# ── Rotación: mismos casos que el test del firmware ──────────────────────────
def test_rotate_heading_zero_identity():
    fx, fy = rotate_offset(0.0, 60.0, 0.0)
    assert abs(fx) < 1e-6 and abs(fy - 60.0) < 1e-6


def test_rotate_90_ccw():
    fx, fy = rotate_offset(0.0, 60.0, math.pi / 2)
    assert abs(fx + 60.0) < 1e-4 and abs(fy) < 1e-4      # (0,60) -> (-60,0)


def test_rotate_180():
    fx, fy = rotate_offset(0.0, 60.0, math.pi)
    assert abs(fx) < 1e-4 and abs(fy + 60.0) < 1e-4       # (0,60) -> (0,-60)


# ── Posición del sensor en cancha (origen del rayo para el render) ───────────
def test_sensor_field_pos_heading_zero():
    # ToF frontal (0,+60) con el robot en (1000,1500) y heading 0 → 60 mm adelante (+Y).
    x, y = sensor_field_pos(TOF_GEOM[0], 1000.0, 1500.0, 0.0)
    assert math.isclose(x, 1000.0, abs_tol=1e-6)
    assert math.isclose(y, 1560.0, abs_tol=1e-6)


def test_sensor_field_pos_with_heading():
    # Mismo ToF frontal pero el robot mira a +90° CCW: el sensor sale a la izquierda (-X).
    x, y = sensor_field_pos(TOF_GEOM[0], 1000.0, 1500.0, math.pi / 2)
    assert math.isclose(x, 940.0, abs_tol=1e-3)           # 1000 + (-60)
    assert math.isclose(y, 1500.0, abs_tol=1e-3)


# ── Diagrama top-down estático (marco propio) ────────────────────────────────
def test_topdown_layout_basic():
    lay = topdown_layout(200.0, 200.0)
    assert lay["scale"] > 0 and lay["robot_radius_px"] > 0
    assert math.isclose(lay["cx"], 100.0) and math.isclose(lay["cy"], 100.0)
    assert len(lay["sensors"]) == 7                       # 4 ToF + 2 cám + 1 US


def test_topdown_layout_sensor_placement_own_frame():
    lay = topdown_layout(200.0, 200.0)
    cx, cy = lay["cx"], lay["cy"]
    by_label = {s["label"]: s for s in lay["sensors"]}
    # ToF frontal: sobre el centro en X, ARRIBA (py<cy), rayo hacia arriba.
    f = by_label["ToF0 frente"]
    assert math.isclose(f["px"], cx, abs_tol=1e-6) and f["py"] < cy
    assert f["ray_dy"] < 0 and math.isclose(f["ray_dx"], 0.0, abs_tol=1e-6)
    # ToF derecho: a la DERECHA (px>cx), a la altura del centro, rayo hacia +x.
    r = by_label["ToF2 derecha"]
    assert r["px"] > cx and math.isclose(r["py"], cy, abs_tol=1e-6)
    assert r["ray_dx"] > 0
    # ToF atrás: ABAJO (py>cy), rayo hacia abajo.
    b = by_label["ToF1 atrás"]
    assert b["py"] > cy and b["ray_dy"] > 0
    # ToF izquierdo: a la IZQUIERDA (px<cx).
    lft = by_label["ToF3 izquierda"]
    assert lft["px"] < cx and lft["ray_dx"] < 0
