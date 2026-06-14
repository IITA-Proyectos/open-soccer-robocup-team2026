"""Tests del simulador TOP + integración con el parser TOP."""
from monitor_base.protocol_top import parse_line_top
from monitor_base.simulator_top import SimulatorTop


def test_lines_parse_and_schema():
    sim = SimulatorTop()
    for _ in range(60):
        f = parse_line_top(sim.next_line())
        assert f.v == 2
        assert f.tof.n == 4
        assert len(f.tof.distances_mm) == 4


def test_deterministic():
    a = SimulatorTop()
    b = SimulatorTop()
    for _ in range(25):
        assert a.next_line() == b.next_line()


def test_ball_blinks():
    sim = SimulatorTop()
    seen_vis, seen_invis = False, False
    for _ in range(60):
        f = parse_line_top(sim.next_line())
        if f.cam.ball_visible:
            seen_vis = True
        else:
            seen_invis = True
    assert seen_vis and seen_invis


def test_tof_no_reading_appears():
    sim = SimulatorTop()
    saw_none = False
    for _ in range(60):
        f = parse_line_top(sim.next_line())
        if None in f.tof.distances_mm:
            saw_none = True
    assert saw_none


def test_top_record_roundtrip(tmp_path):
    # El Recorder graba el raw_json del TopFrame; se relee con el parser TOP.
    from monitor_base.recorder import Recorder
    path = tmp_path / "top.jsonl"
    sim = SimulatorTop()
    frames = [parse_line_top(sim.next_line()) for _ in range(12)]
    rec = Recorder(str(path))
    for f in frames:
        rec.write(f)
    rec.close()
    with open(path, encoding="utf-8") as fh:
        back = [parse_line_top(ln) for ln in fh if ln.strip()]
    assert rec.count == 12
    assert [b.seq for b in back] == [f.seq for f in frames]
