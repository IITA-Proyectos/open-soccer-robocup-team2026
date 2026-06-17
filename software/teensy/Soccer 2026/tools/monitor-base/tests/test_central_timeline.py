"""Tests del módulo PURO central_timeline (sin Tk).

Espejo de la suite del timeline TOP, con los campos del CentralFrame: sample
desde frame, ring buffer maxlen, transitions cuenta flancos, cambios de fsm_state.
Usa el SimulatorCentral + parse_line_central reales (mismo camino que la fuente
de sim) para no inventar frames a mano.
"""
from __future__ import annotations

from dataclasses import replace

from monitor_base.central_timeline import (ALL_SERIES_NAMES, BOOL_SIGNALS,
                                           NUM_SIGNALS, CentralSignalHistory,
                                           CentralTimelineSample, transitions)
from monitor_base.protocol_central import parse_line_central
from monitor_base.simulator_central import SimulatorCentral


def _frame(sim: SimulatorCentral):
    return parse_line_central(sim.next_line())


# ── CentralTimelineSample.from_frame ─────────────────────────────────────────
def test_sample_from_frame_maps_all_fields():
    sim = SimulatorCentral(rate_hz=20.0)
    f = _frame(sim)
    s = CentralTimelineSample.from_frame(f)
    assert s.seq == f.seq
    assert s.t_ms == f.t_ms
    assert s.match == bool(f.fsm.match)
    assert s.heading_valid == bool(f.snap.heading_valid)
    assert s.has_pose == bool(f.snap.has_pose)
    assert s.ball_vis == bool(f.snap.ball_vis)
    assert s.line_valid == bool(f.down.valid)
    assert s.top_fresh == bool(f.top.fresh)
    assert s.down_fresh == bool(f.down.line_fresh)
    assert s.fsm_state == f.fsm.state
    assert s.my_x == f.snap.my_x
    assert s.my_y == f.snap.my_y
    assert s.loop_ema_us == f.loop.ema_us
    assert s.cmd_vx == f.cmd.vx
    assert s.cmd_vy == f.cmd.vy
    assert s.cmd_w == f.cmd.w


def test_sample_fsm_state_is_str():
    sim = SimulatorCentral()
    s = CentralTimelineSample.from_frame(_frame(sim))
    assert isinstance(s.fsm_state, str)
    assert s.fsm_state in ("ATTACK", "SEARCH")


def test_sample_has_pose_reflects_conf_zero():
    sim = SimulatorCentral()
    f = _frame(sim)
    f_no = replace(f, snap=replace(f.snap, my_pose_conf=0))
    assert CentralTimelineSample.from_frame(f_no).has_pose is False
    f_yes = replace(f, snap=replace(f.snap, my_pose_conf=80))
    assert CentralTimelineSample.from_frame(f_yes).has_pose is True


# ── CentralSignalHistory ring buffer ─────────────────────────────────────────
def test_history_push_grows_until_maxlen():
    h = CentralSignalHistory(maxlen=10)
    sim = SimulatorCentral()
    for _ in range(5):
        h.push(_frame(sim))
    assert len(h) == 5


def test_history_respects_maxlen():
    h = CentralSignalHistory(maxlen=50)
    sim = SimulatorCentral()
    for _ in range(200):
        h.push(_frame(sim))
    assert len(h) == 50
    assert h.maxlen == 50


def test_history_latest_is_last_pushed():
    h = CentralSignalHistory(maxlen=300)
    sim = SimulatorCentral()
    last_sample = None
    for _ in range(7):
        last_sample = h.push(_frame(sim))
    assert h.latest is last_sample
    assert h.latest.seq == 6


def test_history_latest_none_when_empty():
    h = CentralSignalHistory()
    assert h.latest is None
    assert len(h) == 0


def test_history_series_returns_chronological_values():
    h = CentralSignalHistory(maxlen=300)
    sim = SimulatorCentral()
    for _ in range(10):
        h.push(_frame(sim))
    seqs = h.series("seq")
    assert seqs == list(range(10))


def test_history_series_all_names_present():
    h = CentralSignalHistory(maxlen=300)
    sim = SimulatorCentral()
    for _ in range(3):
        h.push(_frame(sim))
    for name in ALL_SERIES_NAMES:
        assert len(h.series(name)) == 3


def test_history_span_ms_tracks_window():
    h = CentralSignalHistory(maxlen=300)
    sim = SimulatorCentral(rate_hz=20.0)  # 50 ms / frame
    for _ in range(11):
        h.push(_frame(sim))
    # 11 muestras → span = (11-1) * 50 ms = 500 ms.
    assert h.span_ms() == 500


def test_history_span_ms_zero_with_under_two():
    h = CentralSignalHistory()
    assert h.span_ms() == 0
    h.push(_frame(SimulatorCentral()))
    assert h.span_ms() == 0


# ── transitions / flapping ───────────────────────────────────────────────────
def test_transitions_counts_edges():
    assert transitions([0, 1, 0, 1]) == 3
    assert transitions([1, 1, 1]) == 0
    assert transitions([]) == 0
    assert transitions([5]) == 0


def test_transitions_treats_none_as_value():
    assert transitions([None, 1, None]) == 2
    assert transitions([None, None]) == 0


def test_transitions_works_on_strings():
    assert transitions(["A", "A", "B", "A"]) == 2


def test_history_transitions_ball_vis_flaps():
    # El simulador parpadea ball_vis (s%30 < 24) → debe haber flancos.
    h = CentralSignalHistory(maxlen=300)
    sim = SimulatorCentral()
    for _ in range(120):
        h.push(_frame(sim))
    assert h.transitions("ball_vis") > 0


def test_history_transitions_fsm_state_changes():
    # fsm_state alterna ATTACK/SEARCH con ball_vis → debe cambiar.
    h = CentralSignalHistory(maxlen=300)
    sim = SimulatorCentral()
    for _ in range(120):
        h.push(_frame(sim))
    assert h.transitions("fsm_state") > 0


def test_history_transitions_match_flaps():
    # match alterna por ventana (s%80 < 60).
    h = CentralSignalHistory(maxlen=300)
    sim = SimulatorCentral()
    for _ in range(200):
        h.push(_frame(sim))
    assert h.transitions("match") > 0


# ── tablas de configuración ──────────────────────────────────────────────────
def test_bool_signals_have_expected_set():
    names = {n for n, _ in BOOL_SIGNALS}
    assert names == {"match", "has_pose", "heading_valid", "ball_vis",
                     "line_valid", "top_fresh", "down_fresh"}


def test_num_signals_have_expected_set():
    names = {n for n, _ in NUM_SIGNALS}
    assert names == {"loop_ema_us", "my_x", "my_y"}
