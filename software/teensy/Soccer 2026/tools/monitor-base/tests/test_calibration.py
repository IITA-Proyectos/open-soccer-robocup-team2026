"""Tests del asistente de calibración."""
from monitor_base.calibration import (
    CalibrationAssistant, SensorCalib, calib_from_ring,
)


def test_captures_min_max_threshold_margin():
    c = CalibrationAssistant(n=2)
    c.start()
    c.update([300, 510])
    c.update([100, 505])   # sensor0 baja a 100
    c.update([800, 520])   # sensor0 sube a 800; sensor1 oscila poco
    c.stop()
    res = c.result()
    assert res[0].carpet == 100 and res[0].white == 800
    assert res[0].threshold == 450
    assert res[0].margin == 700
    assert res[1].carpet == 505 and res[1].white == 520
    assert res[1].margin == 15
    assert res[0].samples == 3


def test_suspects_by_margin():
    c = CalibrationAssistant(n=2)
    c.start()
    for _ in range(5):
        c.update([100, 500])
        c.update([800, 515])
    c.stop()
    sus = c.suspects(min_margin=40)
    assert [s.index for s in sus] == [1]   # sensor1 margin=15 < 40


def test_inactive_update_is_noop():
    c = CalibrationAssistant(n=1)
    c.update([500])          # sin start() → no captura
    assert c.result()[0].samples == 0
    assert c.result()[0].is_suspect()   # sin muestras = sospechoso


def test_start_resets_previous():
    c = CalibrationAssistant(n=1)
    c.start()
    c.update([100])
    c.update([900])
    c.start()                # reinicia
    c.update([400])
    assert c.result()[0].carpet == 400 and c.result()[0].white == 400


def test_calib_from_ring():
    res = calib_from_ring([150, 160], [800, 810])
    assert isinstance(res[0], SensorCalib)
    assert res[0].threshold == 475
    assert res[1].margin == 650
