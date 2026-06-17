"""Tests del CentralRatesPanel: lógica PURA (fields_to_watch) + smoke headless.

- Tests puros (sin Tk): que fields_to_watch mapee bien el CentralFrame al dict de
  campos a vigilar, y que las claves coincidan con lo que el panel registra.
- Smoke headless: construir el panel, renderizar ~40 frames del simulator (con
  t_ms avanzando) sin crashear, y verificar que el frame_hz interno > 0 tras
  acumular suficientes muestras.

Se saltea el smoke si no hay display (CI headless puro). Los tests puros corren
siempre.
"""
from __future__ import annotations

import pytest

from monitor_base.protocol_central import parse_line_central
from monitor_base.simulator_central import SimulatorCentral


# ── Tests PUROS (sin Tk) ─────────────────────────────────────────────────────

def test_fields_to_watch_maps_central_frame():
    """fields_to_watch extrae los campos clave del frame con los tipos correctos."""
    from monitor_base.panel_central_rates import fields_to_watch

    sim = SimulatorCentral(rate_hz=20.0)
    f = parse_line_central(sim.next_line())
    d = fields_to_watch(f)

    # Todas las claves esperadas presentes.
    assert set(d) == {
        "top_fr", "down_rx", "ball_x", "my_x", "my_y",
        "heading_valid", "down_valid", "fsm_state",
    }
    # Tipos: contadores/coords int, heading_valid/down_valid 0|1, state str.
    assert isinstance(d["top_fr"], int)
    assert isinstance(d["down_rx"], int)
    assert isinstance(d["ball_x"], int)
    assert d["heading_valid"] in (0, 1)
    assert d["down_valid"] in (0, 1)
    assert isinstance(d["fsm_state"], str)
    # Reflejan el frame real.
    assert d["top_fr"] == f.top.fr
    assert d["down_rx"] == f.down.rx
    assert d["ball_x"] == f.snap.ball_x


def test_fields_to_watch_links_track_link_arrival():
    """top_fr y down_rx incrementan frame a frame en el sim → su Hz de cambio es
    la tasa real del enlace. Verificamos el monotónico crecimiento."""
    from monitor_base.panel_central_rates import fields_to_watch

    sim = SimulatorCentral(rate_hz=20.0)
    prev_top = prev_down = -1
    for _ in range(10):
        f = parse_line_central(sim.next_line())
        d = fields_to_watch(f)
        assert d["top_fr"] > prev_top
        assert d["down_rx"] > prev_down
        prev_top, prev_down = d["top_fr"], d["down_rx"]


def test_fields_to_watch_defensive_on_short_frame():
    """Frame mínimo (objetos None) no debe lanzar: getattr con default."""
    from monitor_base.panel_central_rates import fields_to_watch

    class _Bare:
        snap = top = down = fsm = None

    d = fields_to_watch(_Bare())  # type: ignore[arg-type]
    assert d["top_fr"] == 0
    assert d["fsm_state"] == ""


# ── Smoke headless (con Tk) ──────────────────────────────────────────────────

def _withdraw_root():
    tk = pytest.importorskip("tkinter")
    try:
        root = tk.Tk()
    except tk.TclError:
        pytest.skip("sin display (headless)")
    root.withdraw()
    return tk, root


def _make_ctx():
    from monitor_base.panel import PanelContext

    def _send(_cmd: str) -> None:
        pass

    def _log(_level: str, _msg: str) -> None:
        pass

    return PanelContext(send=_send, log=_log, is_sim=True, config_path=None)


def test_central_rates_panel_constructs_and_renders():
    tk, root = _withdraw_root()
    try:
        from monitor_base.panel_central_rates import CentralRatesPanel

        ctx = _make_ctx()
        panel = CentralRatesPanel(root, ctx)
        sim = SimulatorCentral(rate_hz=20.0)

        last_now = 0
        for _ in range(40):
            f = parse_line_central(sim.next_line())
            panel.render(f)
            last_now = f.t_ms
            root.update_idletasks()

        # Tras 40 frames a 20 Hz, los medidores tienen flujo: frame_hz > 0.
        assert panel.meters.frame_hz(last_now) > 0
        # Y la tasa de llegada de los enlaces (cambio de top_fr/down_rx) > 0.
        assert panel.meters.change_hz("top_fr", last_now) > 0
        assert panel.meters.change_hz("down_rx", last_now) > 0
    finally:
        try:
            root.destroy()
        except Exception:
            pass


def test_central_rates_panel_frame_hz_near_20():
    """El simulator emite a 20 Hz → el panel debe medir ~20 Hz de telemetría."""
    tk, root = _withdraw_root()
    try:
        from monitor_base.panel_central_rates import CentralRatesPanel

        ctx = _make_ctx()
        panel = CentralRatesPanel(root, ctx)
        sim = SimulatorCentral(rate_hz=20.0)

        last_now = 0
        for _ in range(40):
            f = parse_line_central(sim.next_line())
            panel.render(f)
            last_now = f.t_ms

        assert panel.meters.frame_hz(last_now) == pytest.approx(20.0, rel=0.10)
    finally:
        try:
            root.destroy()
        except Exception:
            pass
