"""Tests del módulo PURO rate_meter (FrameRateMeter + ChangeRateMeter + BoardRateMeters).

Sin tiempo real ni RNG: el caller siempre pasa now_ms explícito. TDD: cubre Hz al
régimen estable, vacío, stale, skew de reloj, cambio de valor, y agregación
multi-placa.
"""
from __future__ import annotations

import pytest

from monitor_base.rate_meter import (
    BoardRateMeters, ChangeRateMeter, FrameRateMeter,
)


# ── FrameRateMeter ────────────────────────────────────────────────────────────

def test_frame_meter_empty_is_stale():
    m = FrameRateMeter()
    assert m.is_stale is True
    assert m.hz(100) == 0.0
    assert m.count == 0


def test_frame_meter_single_tick_yields_zero_hz():
    """Una sola muestra no permite estimar Hz (necesita ≥2 puntos)."""
    m = FrameRateMeter()
    m.tick(0)
    assert m.is_stale is False
    assert m.hz(10) == 0.0
    assert m.count == 1


def test_frame_meter_20hz_stream_reports_about_20():
    """50 ms entre ticks → Hz ~ 20."""
    m = FrameRateMeter(window_ms=1000)
    for i in range(21):
        m.tick(i * 50)
    hz = m.hz(1000)
    assert hz == pytest.approx(20.0, rel=0.05)


def test_frame_meter_100hz_stream_reports_about_100():
    m = FrameRateMeter(window_ms=1000)
    for i in range(101):
        m.tick(i * 10)
    hz = m.hz(1000)
    assert hz == pytest.approx(100.0, rel=0.05)


def test_frame_meter_window_drops_old_samples():
    """Tras silencio > window_ms, samples viejos ya no cuentan."""
    m = FrameRateMeter(window_ms=1000, stale_after_ms=10_000)
    for i in range(10):
        m.tick(i * 100)   # 10 muestras hasta t=900
    # Avanzo el reloj 5 s — todos los samples viejos caen fuera de la ventana
    m.tick(5_900)
    # Ahora solo hay 1 muestra reciente → Hz=0 hasta que llegue otra
    assert m.hz(6000) == 0.0


def test_frame_meter_stale_after_silence():
    """Sin tick en > stale_after_ms → reporta 0 aunque tuviera buena tasa antes."""
    m = FrameRateMeter(window_ms=1000, stale_after_ms=2000)
    for i in range(10):
        m.tick(i * 50)
    # Justo después: Hz≈20
    assert m.hz(450) > 10
    # Espero 3 s sin ticks: stale → Hz=0
    assert m.hz(3_500) == 0.0


def test_frame_meter_ignores_clock_skew_backwards():
    """Si llega un tick con timestamp viejo, lo ignora (no rompe el cálculo)."""
    m = FrameRateMeter()
    m.tick(1000)
    m.tick(900)   # ¡skew! → ignorado
    assert m.count == 1


def test_frame_meter_reset_clears_state():
    m = FrameRateMeter()
    for i in range(10):
        m.tick(i * 100)
    m.reset()
    assert m.is_stale is True
    assert m.count == 0


# ── ChangeRateMeter ────────────────────────────────────────────────────────────

def test_change_meter_first_observe_is_baseline():
    """El primer observe no cuenta como cambio (no hay base previa)."""
    m = ChangeRateMeter()
    changed = m.observe(42, 0)
    assert changed is False
    assert m.change_count == 0


def test_change_meter_same_value_no_change():
    m = ChangeRateMeter()
    m.observe(42, 0)
    assert m.observe(42, 100) is False
    assert m.observe(42, 200) is False
    assert m.change_count == 0


def test_change_meter_counts_only_changes():
    m = ChangeRateMeter()
    m.observe(0, 0)
    assert m.observe(1, 100) is True
    assert m.observe(1, 200) is False
    assert m.observe(0, 300) is True
    assert m.change_count == 2


def test_change_meter_hz_steady_flapping():
    """Valor alterna 0/1 cada 100 ms → 10 cambios/s."""
    m = ChangeRateMeter(window_ms=1000)
    for i in range(11):
        m.observe(i % 2, i * 100)
    hz = m.hz(1000)
    assert hz == pytest.approx(10.0, rel=0.05)


def test_change_meter_epsilon_filters_noise():
    """Con epsilon=0.5, ruido pequeño no cuenta como cambio."""
    m = ChangeRateMeter(epsilon=0.5)
    m.observe(10.0, 0)
    assert m.observe(10.2, 100) is False   # dentro de epsilon
    assert m.observe(11.0, 200) is True    # fuera
    assert m.change_count == 1


def test_change_meter_last_value_exposed():
    m = ChangeRateMeter()
    assert m.last_value is None
    m.observe("PATROL", 0)
    assert m.last_value == "PATROL"
    m.observe("INTERCEPT", 100)
    assert m.last_value == "INTERCEPT"


def test_change_meter_handles_tuples():
    m = ChangeRateMeter()
    m.observe((100, 200), 0)
    assert m.observe((100, 200), 100) is False
    assert m.observe((100, 201), 200) is True


# ── BoardRateMeters ────────────────────────────────────────────────────────────

def test_board_meters_frame_hz_after_observes():
    b = BoardRateMeters()
    for i in range(21):
        b.on_frame(now_ms=i * 50)
    assert b.frame_hz(1000) == pytest.approx(20.0, rel=0.05)


def test_board_meters_watch_field_then_change_hz():
    b = BoardRateMeters()
    b.watch_field("ball_x")
    # ball_x alterna entre 100 y -100 cada 100 ms → 10 cambios/s
    for i in range(11):
        b.on_frame(now_ms=i * 100, ball_x=(100 if i % 2 == 0 else -100))
    assert b.change_hz("ball_x", 1000) == pytest.approx(10.0, rel=0.05)


def test_board_meters_unwatched_field_returns_zero():
    """change_hz de un campo no vigilado retorna 0, no excepción."""
    b = BoardRateMeters()
    b.on_frame(now_ms=0)
    assert b.change_hz("inexistente", 100) == 0.0


def test_board_meters_is_stale_after_creation():
    b = BoardRateMeters()
    assert b.is_stale is True
    b.on_frame(now_ms=0)
    assert b.is_stale is False


def test_board_meters_reset_clears_all():
    b = BoardRateMeters()
    b.watch_field("ball_x")
    for i in range(10):
        b.on_frame(now_ms=i * 100, ball_x=i)
    b.reset()
    assert b.is_stale is True
    assert b.change_hz("ball_x", 1000) == 0.0
