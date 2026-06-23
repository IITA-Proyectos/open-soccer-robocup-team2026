"""Tests de gui_tof8x8 — VISOR 8×8 de los 4 ToF (solo lectura).

Se testea SOLO la lógica PURA (parser del mensaje ZN8, estado, medidor de fps) y
el selftest headless. La capa Tk y el serial no se testean (no hay display ni
placa en CI).
"""
import pytest

from monitor_base.gui_tof8x8 import (
    GRID_N,
    GRID_W,
    TOF_NO_READING,
    FpsMeter,
    Zn8Frame,
    Zn8State,
    _synthetic_zn8,
    is_zn8_line,
    parse_zn8_line,
    selftest,
)
from monitor_base.zones import ZoneGrid


# ── Detección de la línea ────────────────────────────────────────────────────
def test_is_zn8_line():
    assert is_zn8_line("ZN8,0,64,5000," + ",".join(["100"] * 64))
    assert is_zn8_line("  ZN8,1,64,123,...")          # tolera espacios
    assert not is_zn8_line('{"v":2,"seq":1}')          # JSON no es ZN8
    assert not is_zn8_line("ZN8")                       # sin coma no es válido
    assert not is_zn8_line("[DOWN] boot")               # print de boot


# ── Parser del contrato ZN8 ──────────────────────────────────────────────────
def test_parse_zn8_basic_64_cells():
    vals = list(range(64))
    line = "ZN8,2,64,4200," + ",".join(str(v) for v in vals)
    fr = parse_zn8_line(line)
    assert isinstance(fr, Zn8Frame)
    assert fr.idx == 2 and fr.res == 64 and fr.dt_us == 4200
    assert len(fr.cells) == GRID_N == 64
    assert fr.cells == vals


def test_parse_zn8_no_reading_becomes_none():
    vals = [100] * 64
    vals[10] = TOF_NO_READING      # 65535 → None
    line = "ZN8,0,64,1000," + ",".join(str(v) for v in vals)
    fr = parse_zn8_line(line)
    assert fr.cells[10] is None
    assert fr.valid_count == 63


def test_parse_zn8_grid_is_8x8():
    fr = parse_zn8_line(_synthetic_zn8(0, dt_us=5000, base_mm=200))
    g = fr.grid
    assert isinstance(g, ZoneGrid)
    assert g.width == GRID_W == 8
    assert g.height == 8
    assert len(g.cells) == 64


def test_parse_zn8_rejects_wrong_count():
    with pytest.raises(ValueError):
        parse_zn8_line("ZN8,0,64,1000,1,2,3")        # faltan valores


def test_parse_zn8_rejects_bad_prefix():
    with pytest.raises(ValueError):
        parse_zn8_line("XX,0,64,1000," + ",".join(["1"] * 64))


def test_parse_zn8_rejects_idx_out_of_range():
    with pytest.raises(ValueError):
        parse_zn8_line("ZN8,9,64,1000," + ",".join(["1"] * 64))


def test_parse_zn8_rejects_non_numeric():
    vals = ["100"] * 64
    vals[3] = "abc"
    with pytest.raises(ValueError):
        parse_zn8_line("ZN8,0,64,1000," + ",".join(vals))


# ── Medidor de fps (puro, tiempo inyectado) ──────────────────────────────────
def test_fps_meter_measures_rate():
    m = FpsMeter()
    # Frames a período fijo de 0.05 s → 20 fps.
    t = 0.0
    for _ in range(20):
        m.tick(0, t)
        t += 0.05
    fps = m.fps(0)
    assert fps is not None
    assert 19 <= fps <= 21
    assert m.count[0] == 20


def test_fps_meter_unknown_sensor_is_none():
    m = FpsMeter()
    m.tick(1, 0.0)            # un solo frame: no hay período aún
    assert m.fps(1) is None
    assert m.fps(2) is None   # nunca visto


# ── Estado del visor (alimentado con líneas crudas) ──────────────────────────
def test_state_feed_keeps_last_frame_per_sensor():
    st = Zn8State()
    for idx in range(4):
        upd = st.feed(_synthetic_zn8(idx, dt_us=3000, base_mm=100), now=idx * 0.02)
        assert upd == idx
    assert sorted(st.frames) == [0, 1, 2, 3]
    assert st.total == 4
    assert st.parse_errors == 0


def test_state_feed_ignores_non_zn8():
    st = Zn8State()
    assert st.feed('{"v":2,"seq":1}') is None
    assert st.feed("[DOWN] boot ok") is None
    assert st.total == 0 and st.parse_errors == 0


def test_state_feed_counts_malformed_zn8():
    st = Zn8State()
    assert st.feed("ZN8,0,64,1000,1,2,3") is None   # ZN8 pero malformado
    assert st.parse_errors == 1
    assert st.total == 0


def test_state_feed_updates_last_frame():
    st = Zn8State()
    st.feed(_synthetic_zn8(0, dt_us=1000, base_mm=100), now=0.0)
    st.feed(_synthetic_zn8(0, dt_us=9999, base_mm=500), now=0.02)
    assert st.frames[0].dt_us == 9999          # quedó el último
    assert st.frames[0].cells[0] == 500


# ── Selftest headless ────────────────────────────────────────────────────────
def test_selftest_returns_zero(capsys):
    rc = selftest(120)
    assert rc == 0
    out = capsys.readouterr().out.lower()
    assert "8x8" in out
    assert "ok" in out
