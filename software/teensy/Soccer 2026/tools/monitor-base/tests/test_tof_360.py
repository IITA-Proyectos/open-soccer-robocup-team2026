"""Tests de gui_tof_360 — VISTA V3: tira 360 de los 4 ToF + cámaras.

Solo se testea la LÓGICA PURA (orden de paneles, orientación del izquierdo,
resumen de cámaras) y el selftest headless. La capa Tk no se testea.
"""
import pytest

from monitor_base.gui_tof_360 import (
    PANEL_ORDER_360,
    cam_summary,
    oriented_grid_for_sensor,
    panel_label_for_column,
    panel_order_labels,
    panel_sensor_for_column,
    selftest,
)
from monitor_base.protocol_top import CamPer
from monitor_base.zones import ZoneGrid, raw_zone_for_canonical


# ── Orden físico 360 de los paneles ──────────────────────────────────────────
def test_panel_order_is_left_front_right_back():
    # El orden de columnas, de izquierda a derecha, DEBE ser este.
    assert panel_order_labels() == ["IZQUIERDO", "FRONTAL", "DERECHO", "TRASERO"]


def test_panel_sensor_mapping_matches_physical_positions():
    # IZQUIERDO=3 · FRONTAL=0 · DERECHO=2 · TRASERO=1 (espejo de TOF_LABELS).
    assert panel_sensor_for_column(0) == 3   # IZQUIERDO
    assert panel_sensor_for_column(1) == 0   # FRONTAL
    assert panel_sensor_for_column(2) == 2   # DERECHO
    assert panel_sensor_for_column(3) == 1   # TRASERO


def test_panel_labels_per_column():
    assert panel_label_for_column(0) == "IZQUIERDO"
    assert panel_label_for_column(3) == "TRASERO"


def test_panel_order_has_four_unique_sensors():
    sensors = [idx for _, idx in PANEL_ORDER_360]
    assert sorted(sensors) == [0, 1, 2, 3]


# ── Orientación: el ToF izquierdo (col 0 → sensor 3) queda 180° ──────────────
def test_left_tof_is_rotated_180():
    raw = list(range(16))
    left_idx = panel_sensor_for_column(0)        # = 3
    grid = oriented_grid_for_sensor(raw, left_idx)
    # 180° = canon 0 ← raw 15, canon 15 ← raw 0 (reusa raw_zone_for_canonical).
    assert grid.cells[0] == raw[raw_zone_for_canonical(3, 0, 4)] == 15
    assert grid.cells[15] == raw[raw_zone_for_canonical(3, 15, 4)] == 0
    # Coincide con ZoneGrid.from_sensor (la fuente canónica).
    assert grid.cells == ZoneGrid.from_sensor(raw, 3, width=4).cells


def test_non_left_panels_are_identity():
    raw = list(range(16))
    for col in (1, 2, 3):                         # FRONTAL/DERECHO/TRASERO
        idx = panel_sensor_for_column(col)
        grid = oriented_grid_for_sensor(raw, idx)
        assert grid.cells == raw                  # identidad (no van rotados)


def test_oriented_grid_with_none_cells():
    raw = [None] * 16
    raw[15] = 250
    grid = oriented_grid_for_sensor(raw, 3)       # izquierdo: 15 → posición 0
    assert grid.cells[0] == 250
    assert grid.valid_count == 1


# ── BONUS: respetar la config del operador (tof_layout) ──────────────────────
def test_config_orientation_overrides_default():
    pytest.importorskip("monitor_base.tof_layout")
    from monitor_base.tof_layout import TofLayout
    cfg = TofLayout()
    raw = list(range(16))
    # FRONTAL (idx 0) con config = identidad por defecto (rotación 0).
    g0 = oriented_grid_for_sensor(raw, 0, cfg=cfg)
    assert g0.cells == cfg.oriented_cells(raw, 0)
    # IZQUIERDO (idx 3): la config trae rotación 180° por defecto → mismo efecto
    # que from_sensor, pero por el camino de la config.
    g3 = oriented_grid_for_sensor(raw, 3, cfg=cfg)
    assert g3.cells == cfg.oriented_cells(raw, 3)


# ── Resumen de cámaras (solo campos que existen en CamPer) ───────────────────
def _camper(**kw) -> CamPer:
    base = dict(ball_visible=False, ball_x_mm=0, ball_y_mm=0,
                yellow_visible=False, yellow_angle_deg=0.0, yellow_distance_mm=0,
                blue_visible=False, blue_angle_deg=0.0, blue_distance_mm=0)
    base.update(kw)
    return CamPer(**base)


def test_cam_summary_ball_and_goal():
    cam = _camper(ball_visible=True, ball_x_mm=120, ball_y_mm=-30,
                  yellow_visible=True, yellow_angle_deg=5.0, yellow_distance_mm=1200)
    s = cam_summary(cam)
    assert "120" in s and "-30" in s          # pelota en x/y
    assert "1200" in s                         # arco amarillo a distancia


def test_cam_summary_nothing_visible():
    s = cam_summary(_camper())
    assert "—" in s                            # no inventa detecciones


def test_cam_summary_handles_none():
    assert cam_summary(None) == "sin datos"


# ── Selftest headless (pipeline real del simulador) ──────────────────────────
def test_selftest_returns_zero(capsys):
    rc = selftest(120)
    assert rc == 0
    out = capsys.readouterr().out.lower()
    assert "360" in out
    assert "presentes" in out                  # el simulador manda zonas
