"""Tests del simulador CENTRAL + integración con el parser CENTRAL."""
from monitor_base.protocol_central import parse_line_central
from monitor_base.simulator_central import SimulatorCentral


def test_lines_parse_and_schema():
    sim = SimulatorCentral()
    for _ in range(120):
        f = parse_line_central(sim.next_line())
        assert f.v == 1
        assert len(f.pwm) == 3
        assert f.fsm.role in ("ATK", "GK")


def test_deterministic():
    a = SimulatorCentral()
    b = SimulatorCentral()
    for _ in range(40):
        assert a.next_line() == b.next_line()


def test_seq_and_time_advance():
    sim = SimulatorCentral(rate_hz=20.0)
    f0 = parse_line_central(sim.next_line())
    f1 = parse_line_central(sim.next_line())
    assert f1.seq == f0.seq + 1
    assert f1.t_ms == f0.t_ms + 50   # 1000/20


def test_ball_blinks():
    sim = SimulatorCentral()
    seen_vis, seen_invis = False, False
    for _ in range(60):
        f = parse_line_central(sim.next_line())
        if f.snap.ball_vis:
            seen_vis = True
        else:
            seen_invis = True
    assert seen_vis and seen_invis


def test_links_blink_to_stale():
    # El sim hace parpadear los enlaces a "viejo" (fresh=0) para ejercitar el
    # semáforo de salud sin robot.
    sim = SimulatorCentral()
    saw_top_stale = saw_down_stale = saw_otos_stale = False
    for _ in range(120):
        f = parse_line_central(sim.next_line())
        if not f.top.fresh:
            saw_top_stale = True
        if not f.down.line_fresh:
            saw_down_stale = True
        if not f.otos.fresh:
            saw_otos_stale = True
    assert saw_top_stale and saw_down_stale and saw_otos_stale


def test_match_toggles():
    sim = SimulatorCentral()
    seen_play, seen_stop = False, False
    for _ in range(120):
        f = parse_line_central(sim.next_line())
        if f.fsm.match:
            seen_play = True
        else:
            seen_stop = True
    assert seen_play and seen_stop


def test_pwm_within_range():
    sim = SimulatorCentral()
    for _ in range(120):
        f = parse_line_central(sim.next_line())
        for p in f.pwm:
            assert -255 <= p <= 255


def test_central_record_roundtrip(tmp_path):
    # El Recorder graba el raw_json del CentralFrame; se relee con el parser CENTRAL.
    from monitor_base.recorder import Recorder
    path = tmp_path / "central.jsonl"
    sim = SimulatorCentral()
    frames = [parse_line_central(sim.next_line()) for _ in range(12)]
    rec = Recorder(str(path))
    for f in frames:
        rec.write(f)
    rec.close()
    with open(path, encoding="utf-8") as fh:
        back = [parse_line_central(ln) for ln in fh if ln.strip()]
    assert rec.count == 12
    assert [b.seq for b in back] == [f.seq for f in frames]
