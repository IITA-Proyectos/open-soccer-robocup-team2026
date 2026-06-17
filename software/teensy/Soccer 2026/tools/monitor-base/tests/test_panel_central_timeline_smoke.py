"""Smoke headless del CentralTimelinePanel: construye + renderiza frames del
simulator CENTRAL sin crashear. Se saltea si no hay display (CI headless puro).
"""
from __future__ import annotations

import pytest


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


def test_central_timeline_panel_constructs_and_renders():
    tk, root = _withdraw_root()
    try:
        from monitor_base.panel_central_timeline import CentralTimelinePanel
        from monitor_base.protocol_central import parse_line_central
        from monitor_base.simulator_central import SimulatorCentral

        panel = CentralTimelinePanel(root, _make_ctx())
        sim = SimulatorCentral(rate_hz=20.0)
        for _ in range(40):
            panel.render(parse_line_central(sim.next_line()))
            root.update_idletasks()

        # El ring acumuló muestras y el canvas tiene items (chasis + dinámico).
        assert len(panel.hist) == 40
        assert len(panel.canvas.find_all()) > 8
        # Tablero de flapping armado (7 bool + fsm_state = 8 labels).
        assert len(panel._flap_labels) == 8
    finally:
        try:
            root.destroy()
        except Exception:
            pass


def test_central_timeline_panel_ring_buffer_caps_at_maxlen():
    tk, root = _withdraw_root()
    try:
        from monitor_base.panel_central_timeline import CentralTimelinePanel
        from monitor_base.protocol_central import parse_line_central
        from monitor_base.simulator_central import SimulatorCentral

        panel = CentralTimelinePanel(root, _make_ctx())
        sim = SimulatorCentral(rate_hz=20.0)
        for _ in range(panel.hist.maxlen + 50):
            panel.render(parse_line_central(sim.next_line()))
        assert len(panel.hist) == panel.hist.maxlen
        root.update_idletasks()
    finally:
        try:
            root.destroy()
        except Exception:
            pass


def test_central_timeline_panel_on_show_redraws_without_crash():
    tk, root = _withdraw_root()
    try:
        from monitor_base.panel_central_timeline import CentralTimelinePanel
        from monitor_base.protocol_central import parse_line_central
        from monitor_base.simulator_central import SimulatorCentral

        panel = CentralTimelinePanel(root, _make_ctx())
        # on_show con buffer vacío no debe crashear.
        panel.on_show()
        sim = SimulatorCentral(rate_hz=20.0)
        for _ in range(10):
            panel.render(parse_line_central(sim.next_line()))
        # on_show con datos redibuja.
        panel.on_show()
        root.update_idletasks()
        assert len(panel.hist) == 10
    finally:
        try:
            root.destroy()
        except Exception:
            pass


def test_central_timeline_panel_window_does_not_resize_between_frames():
    # REGRESIÓN del PARPADEO: la ventana NO debe cambiar de tamaño entre frames
    # (pack_propagate(False) + header de ancho fijo por fuente monospace).
    tk, root = _withdraw_root()
    try:
        from monitor_base.panel_central_timeline import CentralTimelinePanel
        from monitor_base.protocol_central import parse_line_central
        from monitor_base.simulator_central import SimulatorCentral

        panel = CentralTimelinePanel(root, _make_ctx())
        panel.container.pack(fill="both", expand=True)
        sim = SimulatorCentral(rate_hz=20.0)
        widths = []
        for i in range(16):
            panel.render(parse_line_central(sim.next_line()))
            root.update_idletasks()
            if i >= 5:
                widths.append(panel.canvas.winfo_reqwidth())
        assert len(set(widths)) == 1, f"el canvas oscila: {sorted(set(widths))}"
    finally:
        try:
            root.destroy()
        except Exception:
            pass
