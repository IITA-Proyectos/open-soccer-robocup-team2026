"""Tests de la geometría del anillo (espejo de sensor_geometry.cpp)."""
import pytest

from monitor_base import geometry


def test_sensor_count():
    assert geometry.SENSOR_COUNT == 32
    assert len(geometry.SENSOR_POS) == 32


def test_angle_convention_front():
    # S0 = (-36.28, +82.04) → frente-izquierda ≈ -23.9° (sg_angle_deg .h ejemplo).
    assert geometry.angle_deg(0) == pytest.approx(-23.86, abs=0.1)
    # Frente-centro: S4 (izq) negativo chico, S5 (der) positivo chico.
    assert -15 < geometry.angle_deg(3) < 0
    assert 0 < geometry.angle_deg(4) < 15


def test_angle_convention_back_right():
    # index 20 = (+82.21, -25.91) → atrás-derecha ≈ +107.5°.
    assert geometry.angle_deg(20) == pytest.approx(107.5, abs=0.5)


def test_radius_bands():
    # Anillos externos ~80-88 mm; internos ~37-54 mm.
    assert geometry.radius_mm(0) == pytest.approx(89.7, abs=1.0)
    assert 80 < geometry.radius_mm(8) < 90    # externo izq
    assert 35 < geometry.radius_mm(28) < 40   # interno central (R≈37)
    assert 50 < geometry.radius_mm(24) < 58   # interno frontal (R≈54)


def test_out_of_range():
    assert geometry.angle_deg(-1) == 0.0
    assert geometry.angle_deg(99) == 0.0
    assert geometry.radius_mm(99) == 0.0


def test_bounds():
    min_x, min_y, max_x, max_y = geometry.bounds()
    assert min_x < -80 and max_x > 80
    assert min_y < -75 and max_y > 80
