"""Smoke headless del CentralFieldPanel: construye + renderiza frames del
simulator CENTRAL (incluyendo el caso "POSE NO ANCLADA") sin crashear.

Se saltea si no hay display (CI headless puro).
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
    logs = []

    def _send(_cmd: str) -> None:
        pass

    def _log(level: str, msg: str) -> None:
        logs.append((level, msg))

    return PanelContext(send=_send, log=_log, is_sim=True, config_path=None), logs


def test_central_field_panel_constructs_and_renders_with_pose():
    tk, root = _withdraw_root()
    try:
        from monitor_base.panel_central_field import CentralFieldPanel
        from monitor_base.protocol_central import parse_line_central
        from monitor_base.simulator_central import SimulatorCentral

        ctx, _ = _make_ctx()
        panel = CentralFieldPanel(root, ctx)
        sim = SimulatorCentral(rate_hz=20.0)

        # 12 frames cubren tanto pose-anclada como el flanco a conf=0 (sim
        # parpadea cada 200 seq, pero también ejercitamos al menos una pasada).
        any_with_pose = False
        for _ in range(12):
            f = parse_line_central(sim.next_line())
            panel.render(f)
            if f.snap.has_pose:
                any_with_pose = True
            root.update_idletasks()

        # Hicimos al menos un render con pose y el trail acumuló puntos.
        assert any_with_pose
        assert len(panel.robot_trail) >= 1
        # El canvas tiene items (la cancha base + capa dinámica).
        assert len(panel.canvas.find_all()) > 4
    finally:
        try:
            root.destroy()
        except Exception:
            pass


def test_central_field_panel_handles_no_pose_without_crashing():
    tk, root = _withdraw_root()
    try:
        from dataclasses import replace

        from monitor_base.panel_central_field import CentralFieldPanel
        from monitor_base.protocol_central import parse_line_central
        from monitor_base.simulator_central import SimulatorCentral

        ctx, _ = _make_ctx()
        panel = CentralFieldPanel(root, ctx)
        sim = SimulatorCentral(rate_hz=20.0)
        f = parse_line_central(sim.next_line())
        # Forzar caso conf=0 → has_pose False.
        no_pose_snap = replace(f.snap, my_pose_conf=0, my_x=0, my_y=0)
        f_no_pose = replace(f, snap=no_pose_snap)
        assert not f_no_pose.snap.has_pose
        # Render no debe lanzar.
        panel.render(f_no_pose)
        root.update_idletasks()
        # Sin pose, NO se acumula trail.
        assert len(panel.robot_trail) == 0
        # El aviso "POSE NO ANCLADA" se renderizó.
        warn_text = panel.pose_warn.cget("text")
        assert "POSE NO ANCLADA" in warn_text or "NO ANCLADA" in warn_text
    finally:
        try:
            root.destroy()
        except Exception:
            pass


def test_central_field_panel_clears_trail_via_button_handler():
    tk, root = _withdraw_root()
    try:
        from monitor_base.panel_central_field import CentralFieldPanel
        from monitor_base.protocol_central import parse_line_central
        from monitor_base.simulator_central import SimulatorCentral

        ctx, logs = _make_ctx()
        panel = CentralFieldPanel(root, ctx)
        sim = SimulatorCentral(rate_hz=20.0)
        for _ in range(8):
            panel.render(parse_line_central(sim.next_line()))
        had_points = len(panel.robot_trail)
        panel._clear_trail()
        assert len(panel.robot_trail) == 0
        # Si había puntos, debe haberse loggeado el evento.
        if had_points > 0:
            assert any("estela" in m for _, m in logs)
    finally:
        try:
            root.destroy()
        except Exception:
            pass
