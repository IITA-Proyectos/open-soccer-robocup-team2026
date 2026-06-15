"""Tests de la geometría PURA de gui_polar.py (vista polar TOP). Sin Tk."""
from __future__ import annotations

from monitor_base.gui_polar import (TOF_HFOV_DEG, column_azimuths,
                                     polar_xy, position_bearing, to_xy,
                                     zone_polar_points)


def test_position_bearing():
    assert position_bearing("FRONT") == 0.0
    assert position_bearing("RIGHT") == 90.0
    assert position_bearing("BACK") == 180.0
    assert position_bearing("LEFT") == 270.0
    assert position_bearing("???") == 0.0      # fallback


def test_column_azimuths_centered_and_span():
    az = column_azimuths(0.0, hfov=45.0, w=4)
    assert len(az) == 4
    # simétrico alrededor del bearing
    assert az[0] == -az[3]
    assert az[1] == -az[2]
    # span entre columnas extremas = 3/4 del FOV (4 columnas → 3 pasos)
    assert abs((az[3] - az[0]) - 45.0 * 3 / 4) < 1e-9


def test_column_azimuths_offset_by_bearing():
    az0 = column_azimuths(0.0)
    az90 = column_azimuths(90.0)
    assert all(abs((b - a) - 90.0) < 1e-9 for a, b in zip(az0, az90))


def test_zone_points_skips_none_and_vetoed():
    # 16 zonas: pongo distancias = índice*100 mm, pero algunas None y otras vetadas.
    cells = [i * 100 for i in range(16)]
    cells[0] = None                # sin lectura
    enabled = [True] * 16
    enabled[5] = False             # zona 5 vetada
    pts = zone_polar_points(cells, bearing=0.0, enabled=enabled)
    zs = sorted(z for _, _, z in pts)
    assert 0 not in zs             # None saltada
    assert 5 not in zs             # vetada saltada
    assert len(pts) == 14          # 16 - 1 None - 1 vetada


def test_zone_points_column_maps_to_azimuth():
    cells = [1000] * 16
    az = column_azimuths(0.0)
    pts = zone_polar_points(cells, bearing=0.0)
    for azimuth, depth, z in pts:
        assert depth == 1000
        assert abs(azimuth - az[z % 4]) < 1e-9    # azimut = columna de la zona


def test_zone_points_all_enabled_default():
    cells = [500] * 16
    pts = zone_polar_points(cells, bearing=90.0)   # enabled=None → todas
    assert len(pts) == 16


def test_polar_xy_front_is_up():
    cx = cy = 100.0
    x, y = polar_xy(0.0, 1000.0, scale=0.05, cx=cx, cy=cy)
    assert abs(x - cx) < 1e-6      # frente = derecho arriba (sin componente x)
    assert y < cy                  # hacia ARRIBA en pantalla


def test_polar_xy_right_is_right():
    cx = cy = 100.0
    x, y = polar_xy(90.0, 1000.0, scale=0.05, cx=cx, cy=cy)
    assert x > cx                  # 90° = a la derecha
    assert abs(y - cy) < 1e-6


def test_to_xy_axes():
    cx = cy = 100.0
    # +y (frente) sube; +x (derecha) va a la derecha.
    _, y = to_xy(0, 1000, 0.05, cx, cy)
    x, _ = to_xy(1000, 0, 0.05, cx, cy)
    assert y < cy
    assert x > cx
