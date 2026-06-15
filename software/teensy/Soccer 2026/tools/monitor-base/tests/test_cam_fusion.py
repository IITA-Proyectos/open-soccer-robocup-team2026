"""Tests de gui_cam_fusion.py — lógica PURA de la fusión de cámara (anti-fantasma).

Sólo la lógica testeable (sin Tk): ball_disagreement_mm / is_phantom /
fusion_verdict, y el selftest headless por el parser real del simulador TOP.
"""
import math

import pytest

from monitor_base.gui_cam_fusion import (
    DEFAULT_PHANTOM_THRESH_MM, ball_disagreement_mm, fusion_verdict, is_phantom,
    selftest,
)
from monitor_base.protocol_top import CamPer


def _cam(visible, x=0, y=0):
    """CamPer mínimo: lo único que importa para la fusión de pelota es ball_*."""
    return CamPer(ball_visible=visible, ball_x_mm=x, ball_y_mm=y,
                  yellow_visible=False, yellow_angle_deg=0.0, yellow_distance_mm=0,
                  blue_visible=False, blue_angle_deg=0.0, blue_distance_mm=0)


# ── ball_disagreement_mm ─────────────────────────────────────────────────────

def test_disagreement_none_when_front_blind():
    assert ball_disagreement_mm(_cam(False, 0, 0), _cam(True, 100, 100)) is None


def test_disagreement_none_when_back_blind():
    assert ball_disagreement_mm(_cam(True, 100, 100), _cam(False, 0, 0)) is None


def test_disagreement_none_when_both_blind():
    assert ball_disagreement_mm(_cam(False), _cam(False)) is None


def test_disagreement_zero_when_identical():
    assert ball_disagreement_mm(_cam(True, 200, -50), _cam(True, 200, -50)) == 0.0


def test_disagreement_euclidean():
    # (0,0) vs (300,400) → hipotenusa 500.
    d = ball_disagreement_mm(_cam(True, 0, 0), _cam(True, 300, 400))
    assert d == pytest.approx(500.0)


def test_disagreement_phantom_case_is_2000():
    # caso fantasma del enunciado: camf a +1000, camb a -1000 → delta 2000.
    d = ball_disagreement_mm(_cam(True, 1000, 0), _cam(True, -1000, 0))
    assert d == pytest.approx(2000.0)


# ── is_phantom ───────────────────────────────────────────────────────────────

def test_agreement_not_phantom():
    # delta chico (50 mm) con ambas visibles → NO fantasma.
    assert is_phantom(_cam(True, 0, 0), _cam(True, 30, 40)) is False  # delta 50


def test_phantom_true_when_far_apart():
    # camf +1000, camb -1000 → delta 2000 >> 400 → fantasma.
    assert is_phantom(_cam(True, 1000, 0), _cam(True, -1000, 0)) is True


def test_phantom_false_when_one_camera_only():
    # una sola cámara ve la pelota → no juzgable → no fantasma.
    assert is_phantom(_cam(True, 1000, 0), _cam(False)) is False
    assert is_phantom(_cam(False), _cam(True, -1000, 0)) is False


def test_phantom_threshold_boundary():
    # exactamente en el umbral NO es fantasma (es estricto >), justo por encima sí.
    th = DEFAULT_PHANTOM_THRESH_MM
    assert is_phantom(_cam(True, 0, 0), _cam(True, int(th), 0), thresh_mm=th) is False
    assert is_phantom(_cam(True, 0, 0), _cam(True, int(th) + 1, 0), thresh_mm=th) is True


def test_phantom_custom_threshold():
    # con umbral chico, un delta moderado ya dispara.
    assert is_phantom(_cam(True, 0, 0), _cam(True, 100, 0), thresh_mm=50.0) is True
    assert is_phantom(_cam(True, 0, 0), _cam(True, 100, 0), thresh_mm=200.0) is False


# ── fusion_verdict ───────────────────────────────────────────────────────────

def test_verdict_no_comparison_when_blind():
    assert fusion_verdict(_cam(True, 0, 0), _cam(False)) == "SIN COMPARACIÓN"


def test_verdict_ok_when_agree():
    assert fusion_verdict(_cam(True, 0, 0), _cam(True, 30, 40)) == "FUSIÓN OK"


def test_verdict_suspicious_when_phantom():
    assert fusion_verdict(_cam(True, 1000, 0), _cam(True, -1000, 0)) == "FUSIÓN SOSPECHOSA"


# ── selftest headless ────────────────────────────────────────────────────────

def test_selftest_returns_zero():
    assert selftest(frames=120) == 0
