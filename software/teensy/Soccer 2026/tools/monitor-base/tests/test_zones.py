"""Tests de zones.py — grilla de zonas de los ToF (modelo puro)."""
import pytest

from monitor_base.zones import ZoneGrid, zone_color


def test_zonegrid_from_flat_4x4():
    cells = [100, 200, 300, 400, 500, 600, 700, 800,
             900, 1000, 1100, 1200, 1300, 1400, 1500, 1600]
    g = ZoneGrid.from_flat(cells, width=4)
    assert g.width == 4 and g.height == 4
    assert g.valid_count == 16
    assert g.min_mm == 100 and g.max_mm == 1600
    rows = list(g.rows())
    assert rows[0] == [100, 200, 300, 400]
    assert rows[3] == [1300, 1400, 1500, 1600]


def test_zonegrid_with_none_cells():
    cells = [None] * 16
    cells[5] = 250
    g = ZoneGrid.from_flat(cells, width=4)
    assert g.valid_count == 1
    assert g.min_mm == 250 and g.max_mm == 250


def test_zonegrid_all_none():
    g = ZoneGrid.from_flat([None] * 16, width=4)
    assert g.valid_count == 0
    assert g.min_mm is None and g.max_mm is None


def test_zone_color_near_vs_far_differ_and_clamp():
    c_below = zone_color(50, near=100, far=2000)    # < near → clampa a near
    c_at_near = zone_color(100, near=100, far=2000)
    c_far = zone_color(5000, near=100, far=2000)     # > far → clampa a far
    assert c_below == c_at_near                       # clamp inferior
    assert c_below.startswith("#") and len(c_below) == 7
    assert c_far.startswith("#") and len(c_far) == 7
    assert c_below != c_far                            # cerca y lejos distinguibles
