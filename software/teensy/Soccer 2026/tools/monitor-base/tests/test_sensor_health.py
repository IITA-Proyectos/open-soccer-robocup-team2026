"""Tests del detector de salud de sensores."""
from monitor_base.sensor_health import Health, SensorHealthTracker


def _feed(tracker, frames, builder):
    for k in range(frames):
        tracker.update(builder(k))


def test_oscillating_is_ok_constant_is_dead():
    n = 4
    t = SensorHealthTracker(n=n, window=256, min_samples=64, dead_span=6)

    def builder(k):
        osc = 150 if (k % 2 == 0) else 820   # sensores 0..2 oscilan
        return [osc, osc, osc, 512]          # sensor 3 pegado en 512

    _feed(t, 100, builder)
    st = t.status()
    assert st[0].health == Health.OK
    assert st[1].health == Health.OK
    assert st[2].health == Health.OK
    assert st[3].health == Health.DEAD
    assert st[3].span == 0
    assert [s.index for s in t.problems()] == [3]


def test_saturation_high_and_low():
    t = SensorHealthTracker(n=2, min_samples=64)
    _feed(t, 80, lambda k: [1023, 0])
    st = t.status()
    assert st[0].health == Health.SAT_HIGH
    assert st[1].health == Health.SAT_LOW


def test_unknown_before_min_samples():
    t = SensorHealthTracker(n=1, min_samples=64)
    _feed(t, 10, lambda k: [512])
    assert t.status_of(0).health == Health.UNKNOWN
    assert t.status_of(0).samples == 10


def test_window_limits_memory():
    t = SensorHealthTracker(n=1, window=50, min_samples=10)
    _feed(t, 200, lambda k: [k % 2 * 800])
    # span sigue capturando la oscilación, pero solo 50 muestras retenidas.
    assert t.status_of(0).samples == 50
    assert t.status_of(0).health == Health.OK


def test_reset_clears():
    t = SensorHealthTracker(n=1, min_samples=5)
    _feed(t, 10, lambda k: [100])
    t.reset()
    assert t.status_of(0).samples == 0
    assert t.status_of(0).health == Health.UNKNOWN
