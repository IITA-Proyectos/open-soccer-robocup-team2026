"""Test de la grabación → replay (round-trip por archivo)."""
from monitor_base.protocol import parse_line
from monitor_base.recorder import Recorder
from monitor_base.simulator import Simulator
from monitor_base.sources import read_replay_file


def test_record_then_replay_roundtrip(tmp_path):
    path = tmp_path / "grab.jsonl"
    sim = Simulator(noise=0)
    written = [parse_line(sim.next_line()) for _ in range(25)]
    rec = Recorder(str(path))
    for f in written:
        rec.write(f)
    rec.close()

    back = read_replay_file(str(path))
    assert rec.count == 25
    assert len(back) == 25
    assert [f.seq for f in back] == [f.seq for f in written]
    # contenido exacto: el raw_json se preserva 1:1
    assert back[0].raw_json == written[0].raw_json


def test_recorder_context_manager(tmp_path):
    path = tmp_path / "ctx.jsonl"
    sim = Simulator(noise=0)
    with Recorder(str(path)) as rec:
        rec.write(parse_line(sim.next_line()))
    assert rec.count == 1
    assert read_replay_file(str(path))[0].seq == 0


def test_double_close_is_safe(tmp_path):
    path = tmp_path / "x.jsonl"
    rec = Recorder(str(path))
    rec.close()
    rec.close()  # no debe explotar
    rec.write(parse_line(Simulator(noise=0).next_line()))  # no-op tras cerrar
    assert rec.count == 0
