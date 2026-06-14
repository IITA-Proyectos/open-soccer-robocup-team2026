"""Smoke de la GUI de salud: que construya y renderice frames del simulador sin
crashear. Se saltea si no hay display (CI headless)."""
import pytest


def test_gui_health_constructs_and_renders():
    tk = pytest.importorskip("tkinter")
    try:
        root = tk.Tk()
    except tk.TclError:
        pytest.skip("sin display (headless)")
    root.withdraw()
    try:
        from monitor_base.gui_top_health import MonitorTopHealthApp
        from monitor_base.protocol_top import parse_line_top
        from monitor_base.simulator_top import SimulatorTop
        from monitor_base.sources import SimTopSource

        app = MonitorTopHealthApp(root, SimTopSource())
        sim = SimulatorTop()
        for _ in range(6):
            app._render(parse_line_top(sim.next_line()))
        root.update_idletasks()
        # Se crearon tiles (uno por sensor) y grillas de zonas (4 ToF).
        assert len(app._tiles) >= 14
        assert len(app._zone_cv) == 4
        app._on_close()
    finally:
        try:
            root.destroy()
        except Exception:
            pass
