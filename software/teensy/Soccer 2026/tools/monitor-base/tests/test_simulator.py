"""Tests del simulador + integración con el parser y la salud."""
from monitor_base.protocol import parse_line
from monitor_base.sensor_health import Health, SensorHealthTracker
from monitor_base.simulator import Simulator


def test_lines_parse_and_schema():
    sim = Simulator(noise=0)
    for _ in range(50):
        f = parse_line(sim.next_line())
        assert f.v == 1
        assert f.ring.n == 32
        assert len(f.ring.raw) == 32
        assert f.otos.n == 2


def test_deterministic_without_noise():
    a = Simulator(noise=0)
    b = Simulator(noise=0)
    for _ in range(20):
        assert a.next_line() == b.next_line()


def test_line_sweeps_across_ring():
    # En una corrida larga, en algún momento hay línea presente con varios
    # sensores (la banda barre el anillo).
    sim = Simulator(noise=0)
    max_non = 0
    saw_present = False
    for _ in range(120):
        f = parse_line(sim.next_line())
        if f.line.present:
            saw_present = True
            max_non = max(max_non, f.line.sensors_on_line)
    assert saw_present
    assert max_non >= 2


def test_injected_dead_sensor_is_detected():
    sim = Simulator(noise=0, dead_sensors=[7])
    health = SensorHealthTracker(n=32, min_samples=64)
    for _ in range(160):
        f = parse_line(sim.next_line())
        health.update(f.ring.raw)
    problems = [s.index for s in health.problems()]
    assert problems == [7]
    assert health.status_of(7).health == Health.DEAD


def test_white_bitmask_matches_white_list():
    sim = Simulator(noise=0)
    for _ in range(30):
        f = parse_line(sim.next_line())
        # cada sensor "white" en raw debería ser alto (~820) salvo muertos.
        for i, w in enumerate(f.ring.white):
            if w:
                assert f.ring.raw[i] >= 700
