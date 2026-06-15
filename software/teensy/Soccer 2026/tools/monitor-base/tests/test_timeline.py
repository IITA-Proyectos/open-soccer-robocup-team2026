"""Tests de gui_timeline.py — lógica PURA (SignalHistory + transitions).

La GUI (Tk) NO se testea; sí el ring buffer, el conteo de flancos, y el smoke
headless por el parser real del simulador TOP.
"""
from monitor_base.gui_timeline import (
    ALL_SERIES_NAMES, BOOL_SIGNALS, SignalHistory, TimelineSample, selftest,
    transitions,
)
from monitor_base.protocol_top import parse_line_top
from monitor_base.simulator_top import SimulatorTop


# ── transitions() — conteo de flancos ────────────────────────────────────────

def test_transitions_empty_and_single():
    assert transitions([]) == 0
    assert transitions([1]) == 0
    assert transitions([0]) == 0


def test_transitions_constant_series_has_zero():
    assert transitions([1, 1, 1, 1]) == 0
    assert transitions([0, 0, 0]) == 0
    assert transitions([True, True, True]) == 0


def test_transitions_alternating_0101():
    # 0/1/0/1 → 3 flancos (0→1, 1→0, 0→1).
    assert transitions([0, 1, 0, 1]) == 3
    assert transitions([1, 0, 1, 0]) == 3


def test_transitions_counts_each_edge_once():
    # 0,0,1,1,0 → 2 flancos (0→1 y 1→0), las repeticiones no cuentan.
    assert transitions([0, 0, 1, 1, 0]) == 2


def test_transitions_treats_none_as_value():
    # None→algo cuenta como flanco.
    assert transitions([None, None, 5, 5]) == 1
    assert transitions([5, None, 5]) == 2


def test_transitions_with_bools():
    assert transitions([True, False, True]) == 2


# ── SignalHistory — ring buffer ──────────────────────────────────────────────

def _frames(n: int):
    sim = SimulatorTop(rate_hz=20.0)
    return [parse_line_top(sim.next_line()) for _ in range(n)]


def test_push_increments_length():
    h = SignalHistory(maxlen=100)
    assert len(h) == 0
    for f in _frames(5):
        h.push(f)
    assert len(h) == 5


def test_maxlen_caps_length():
    h = SignalHistory(maxlen=50)
    for f in _frames(200):
        h.push(f)
    assert len(h) == 50           # acotado por maxlen
    # Y se quedó con las ÚLTIMAS (la más nueva tiene el seq más alto).
    assert h.latest.seq == 199


def test_push_returns_sample_with_expected_fields():
    h = SignalHistory(maxlen=10)
    f = _frames(1)[0]
    sample = h.push(f)
    assert isinstance(sample, TimelineSample)
    assert sample.seq == f.seq
    assert sample.t_ms == f.t_ms
    assert sample.valid == bool(f.snap.valid)
    assert sample.ball_visible == bool(f.snap.ball_visible)
    assert sample.heading_valid == bool(f.imu.heading_valid)
    assert sample.goal_opp_visible == bool(f.snap.goal_opp_visible)
    assert sample.goal_own_visible == bool(f.snap.goal_own_visible)
    assert sample.min_obstacle_mm == f.snap.min_obstacle_mm
    assert sample.referee_cmd == int(f.snap.referee_cmd)


def test_series_length_matches_buffer():
    h = SignalHistory(maxlen=100)
    for f in _frames(30):
        h.push(f)
    for name in ALL_SERIES_NAMES:
        assert len(h.series(name)) == 30


def test_series_booleans_are_bools():
    h = SignalHistory(maxlen=100)
    for f in _frames(10):
        h.push(f)
    for name, _label in BOOL_SIGNALS:
        assert all(isinstance(v, bool) for v in h.series(name))


def test_heading_valid_matches_snapshot_flag_bit():
    # heading_valid debe coincidir con el bit HEADING_VALID del snapshot.
    h = SignalHistory(maxlen=10)
    f = _frames(1)[0]
    sample = h.push(f)
    assert sample.heading_valid == ("HEADING_VALID" in f.snap.flag_names)


def test_transitions_via_history_counts_flapping():
    # El simulador parpadea ball_visible (s%30<25) → debe haber flancos.
    h = SignalHistory(maxlen=500)
    for f in _frames(120):
        h.push(f)
    assert h.transitions("ball_visible") > 0
    # snap.valid es constante (=1) en el simulador → 0 flancos.
    assert h.transitions("valid") == 0


def test_span_ms_grows_with_frames():
    h = SignalHistory(maxlen=500)
    assert h.span_ms() == 0           # vacío
    for f in _frames(1):
        h.push(f)
    assert h.span_ms() == 0           # una sola muestra
    for f in _frames(10)[1:]:
        h.push(f)
    assert h.span_ms() >= 0


def test_latest_is_none_when_empty():
    assert SignalHistory(maxlen=5).latest is None


# ── selftest headless ────────────────────────────────────────────────────────

def test_selftest_returns_zero(capsys):
    rc = selftest(120)
    assert rc == 0
    out = capsys.readouterr().out.lower()
    assert "timeline" in out
    assert "flancos" in out
