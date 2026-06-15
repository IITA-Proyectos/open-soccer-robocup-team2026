"""gui_shell.py — Monitor unificado de la placa TOP (consola industrial).

UNA sola ventana con: barra de estado (conexión, seq, Hz, perdidos, grabar/pausa),
NAVEGACIÓN lateral entre vistas (paneles), área de contenido, y un DOCK de logs.
Una sola fuente (conexión) y un solo loop alimentan a todos los paneles; se cambia
de vista sin reconectar. Cada vista es un Panel (panel.py).

Entradas:
  python -m monitor_base --monitor --sim          # consola completa, simulador
  python -m monitor_base --monitor --port auto     # consola completa, robot real
  python -m monitor_base --monitor --selftest      # smoke headless (CI)
Los flags por-vista (--field, --polar, …) siguen andando: abren la MISMA consola
arrancando en esa vista.
"""
from __future__ import annotations

import time
import tkinter as tk
from collections import deque
from tkinter import ttk
from typing import Deque, List, Optional, Type

from .panel import LogBuffer, Panel, PanelContext
from .protocol_top import TopFrame
from .shell_theme import (ACCENT, BAD, BG, BG2, FG, LINE, MONO, MUTED, OK,
                          PANEL, TITLE, UI_B, WARN, apply_theme)

HEARTBEAT_S = 2.0       # cada cuánto el log registra un latido de datos
PHANTOM_MM = 500.0      # |camf.ball - camb.ball| para sospechar pelota fantasma


def _registry() -> List[Type[Panel]]:
    """Todas las vistas del monitor (import perezoso para no cargar Tk en tests
    de lógica pura)."""
    from .panel_logs import LogsPanel
    panels: List[Type[Panel]] = []
    for modname, clsname in (
            ("panel_field", "FieldPanel"),
            ("panel_polar", "PolarPanel"),
            ("panel_tof_360", "Tof360Panel"),
            ("panel_health", "HealthPanel"),
            ("panel_cam_fusion", "CamFusionPanel"),
            ("panel_timeline", "TimelinePanel"),
            ("panel_tof_setup", "TofSetupPanel"),
    ):
        try:
            mod = __import__(f"monitor_base.{modname}", fromlist=[clsname])
            panels.append(getattr(mod, clsname))
        except Exception:  # noqa: BLE001 — un panel que falte no tumba el monitor
            pass
    panels.append(LogsPanel)
    return panels


class MonitorShell:
    def __init__(self, root: tk.Tk, source, panel_classes: Optional[List[Type[Panel]]] = None,
                 recorder=None, config_path: Optional[str] = None,
                 start_key: Optional[str] = None):
        self.root = root
        self.source = source
        self.recorder = recorder
        self.is_sim = getattr(source, "is_sim", False)
        self.logbuf = LogBuffer()
        self.frame_hist: Deque[TopFrame] = deque(maxlen=600)
        self.ctx = PanelContext(send=self._send, log=self.logbuf.add,
                                is_sim=self.is_sim, config_path=config_path)
        self.paused = False
        # métricas
        self.frame_count = 0
        self.last_seq = -1
        self.dropped = 0
        self._rate_hz = 0.0
        self._last_t_ms: Optional[int] = None
        self._last_beat = 0.0
        # estados para anomalías
        self._prev_hv: Optional[bool] = None
        self._prev_valid: Optional[bool] = None
        self._prev_ref: Optional[int] = None

        apply_theme(root)
        root.title("IITA Soccer — Monitor TOP")
        root.configure(bg=BG)
        self._build_layout(panel_classes or _registry())
        self.logbuf.add("ok", f"conectado a {self.source.describe()}"
                              + ("  (SIMULADOR)" if self.is_sim else ""))
        self.source.start()
        self._select(start_key or self.panels[0].key)
        self.root.after(60, self._tick)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ── Layout ──────────────────────────────────────────────────────────────
    def _build_layout(self, panel_classes: List[Type[Panel]]) -> None:
        r = self.root
        r.columnconfigure(1, weight=1)
        r.rowconfigure(1, weight=1)

        # Barra superior.
        top = tk.Frame(r, bg=BG2)
        top.grid(row=0, column=0, columnspan=2, sticky="we")
        tk.Label(top, text="◆ MONITOR TOP", bg=BG2, fg=ACCENT, font=TITLE,
                 padx=12, pady=8).pack(side="left")
        self.conn_dot = tk.Label(top, text="●", bg=BG2, fg=MUTED, font=("Segoe UI", 14))
        self.conn_dot.pack(side="left", padx=(8, 2))
        self.conn_lbl = tk.Label(top, text="…", bg=BG2, fg=FG, font=MONO)
        self.conn_lbl.pack(side="left")
        self.metrics = tk.Label(top, text="", bg=BG2, fg=MUTED, font=MONO)
        self.metrics.pack(side="left", padx=16)
        self.rec_btn = tk.Button(top, text="⏺ grabar", command=self._toggle_record,
                                 bg=PANEL, fg=FG, relief="flat", padx=8)
        self.rec_btn.pack(side="right", padx=4, pady=4)
        self.pause_btn = tk.Button(top, text="⏸ pausa", command=self._toggle_pause,
                                   bg=PANEL, fg=FG, relief="flat", padx=8)
        self.pause_btn.pack(side="right", padx=4, pady=4)

        # Navegación lateral.
        nav = tk.Frame(r, bg=BG2, width=170)
        nav.grid(row=1, column=0, sticky="ns")
        nav.grid_propagate(False)
        tk.Label(nav, text="VISTAS", bg=BG2, fg=MUTED, font=("Segoe UI", 8, "bold"),
                 anchor="w", padx=12, pady=8).pack(fill="x", pady=(6, 2))

        # Contenido (paneles apilados).
        content = tk.Frame(r, bg=BG)
        content.grid(row=1, column=1, sticky="nsew")
        content.rowconfigure(0, weight=1); content.columnconfigure(0, weight=1)

        self.panels: List[Panel] = []
        self._nav_btns = {}
        self.active: Optional[Panel] = None
        for cls in panel_classes:
            try:
                p = cls(content, self.ctx)
            except Exception as e:  # noqa: BLE001
                self.logbuf.add("bad", f"panel '{getattr(cls,'key','?')}' no cargó: {e}")
                continue
            p.container.grid(row=0, column=0, sticky="nsew")
            # Logs necesita el buffer + historial.
            if hasattr(p, "attach"):
                try:
                    p.attach(self.logbuf, self.frame_hist)
                except Exception:  # noqa: BLE001
                    pass
            self.panels.append(p)
            b = tk.Button(nav, text=f" {p.icon}  {p.title}", anchor="w", bg=BG2, fg=FG,
                          relief="flat", font=UI_B, padx=12, pady=7,
                          activebackground="#1d2a36", activeforeground=ACCENT,
                          command=lambda k=p.key: self._select(k))
            b.pack(fill="x")
            self._nav_btns[p.key] = b

        self.status = tk.Label(r, text="iniciando…", bg=BG2, fg=MUTED, font=MONO,
                               anchor="w", padx=8, pady=3)
        self.status.grid(row=2, column=0, columnspan=2, sticky="we")

    # ── Navegación ──────────────────────────────────────────────────────────
    def _select(self, key: str) -> None:
        for p in self.panels:
            self._nav_btns[p.key].configure(
                bg=("#1d2a36" if p.key == key else BG2),
                fg=(ACCENT if p.key == key else FG))
        new = next((p for p in self.panels if p.key == key), None)
        if new is None:
            return
        if self.active is not None and self.active is not new:
            try:
                self.active.on_hide()
            except Exception:  # noqa: BLE001
                pass
        self.active = new
        new.container.tkraise()
        try:
            new.on_show()
            if self.frame_hist:
                new.render(self.frame_hist[-1])
        except Exception as e:  # noqa: BLE001
            self.logbuf.add("bad", f"render '{key}': {e}")
        self._set_status(f"vista: {new.title}")

    # ── Comandos ────────────────────────────────────────────────────────────
    def _send(self, cmd: str) -> None:
        if self.is_sim:
            self.logbuf.add("warn", f"SIM: comando NO enviado ({cmd})")
            return
        try:
            self.source.send(cmd)
            self.logbuf.add("cmd", f"→ {cmd}")
        except Exception as e:  # noqa: BLE001
            self.logbuf.add("bad", f"fallo enviando '{cmd}': {e}")

    # ── Loop ────────────────────────────────────────────────────────────────
    def _tick(self) -> None:
        frames = self.source.poll() if not self.paused else []
        for f in frames:
            self._pump(f)
        self._drain_errors()
        self._update_bar(bool(frames))
        self.root.after(60, self._tick)

    def _pump(self, f: TopFrame) -> None:
        """Procesa un frame: métricas, historial, anomalías al log, render activo."""
        self.frame_count += 1
        if self.last_seq >= 0:
            gap = f.seq - self.last_seq - 1
            if gap > 0:
                self.dropped += gap
        self.last_seq = f.seq
        if self._last_t_ms is not None and f.t_ms > self._last_t_ms:
            inst = 1000.0 / (f.t_ms - self._last_t_ms)
            self._rate_hz = inst if self._rate_hz == 0 else 0.85 * self._rate_hz + 0.15 * inst
        self._last_t_ms = f.t_ms
        self.frame_hist.append(f)
        if self.recorder is not None:
            self.recorder.write(f)
        self._log_anomalies(f)
        if self.active is not None:
            try:
                self.active.render(f)
            except Exception as e:  # noqa: BLE001
                self.logbuf.add("bad", f"render '{self.active.key}': {e}")

    def _log_anomalies(self, f: TopFrame) -> None:
        hv = f.imu.heading_valid
        if self._prev_hv is not None and hv != self._prev_hv:
            self.logbuf.add("ok" if hv else "warn",
                            "heading recuperado" if hv else "heading INVÁLIDO (sin rumbo fiable)")
        self._prev_hv = hv
        sv = f.snap.valid
        if self._prev_valid is not None and sv != self._prev_valid:
            self.logbuf.add("ok" if sv else "warn",
                            "snapshot VÁLIDO" if sv else "snapshot inválido (TOP no manda pose)")
        self._prev_valid = sv
        ref = f.snap.referee_cmd
        if self._prev_ref is not None and ref != self._prev_ref:
            self.logbuf.add("info", f"árbitro → {f.snap.referee_name}")
        self._prev_ref = ref
        # Pelota fantasma: ambas cámaras ven pelota y discrepan mucho.
        cf, cb = f.camf, f.camb
        if cf.ball_visible and cb.ball_visible:
            d = ((cf.ball_x_mm - cb.ball_x_mm) ** 2 + (cf.ball_y_mm - cb.ball_y_mm) ** 2) ** 0.5
            if d > PHANTOM_MM:
                self.logbuf.add("warn", f"posible PELOTA FANTASMA (Δfront/back={d:.0f}mm)")
        # Latido de datos.
        now = time.time()
        if now - self._last_beat >= HEARTBEAT_S:
            self._last_beat = now
            self.logbuf.add("info",
                f"datos seq={f.seq} {self._rate_hz:.0f}Hz hdg={f.imu.heading_deg:+.1f}° "
                f"snap={'OK' if sv else '--'} pelota={'sí' if f.snap.ball_visible else 'no'}")

    def _drain_errors(self) -> None:
        errs = getattr(self.source, "errors", None)
        if errs is None:
            return
        while True:
            try:
                self.logbuf.add("bad", errs.get_nowait())
            except Exception:  # noqa: BLE001
                break

    def _update_bar(self, got: bool) -> None:
        live = got and not self.paused
        self.conn_dot.configure(fg=(OK if live else (WARN if self.paused else MUTED)))
        self.conn_lbl.configure(text=self.source.describe() + (" · SIM" if self.is_sim else ""))
        self.metrics.configure(
            text=f"seq {self.last_seq}   {self._rate_hz:.0f} Hz   frames {self.frame_count}   "
                 f"perdidos {self.dropped}" + ("   ⏸ PAUSA" if self.paused else ""))

    # ── Barra: acciones ─────────────────────────────────────────────────────
    def _toggle_pause(self) -> None:
        self.paused = not self.paused
        self.pause_btn.configure(text=("▶ reanudar" if self.paused else "⏸ pausa"))
        self.logbuf.add("info", "pausado" if self.paused else "reanudado")

    def _toggle_record(self) -> None:
        if self.recorder is None:
            from .recorder import Recorder
            fn = time.strftime("monitor-%Y%m%d-%H%M%S.jsonl")
            try:
                self.recorder = Recorder(fn)
                self.rec_btn.configure(text="⏹ detener", fg=BAD)
                self.logbuf.add("ok", f"grabando en {fn}")
            except Exception as e:  # noqa: BLE001
                self.logbuf.add("bad", f"no pude grabar: {e}")
        else:
            try:
                self.recorder.close()
            except Exception:  # noqa: BLE001
                pass
            self.recorder = None
            self.rec_btn.configure(text="⏺ grabar", fg=FG)
            self.logbuf.add("info", "grabación detenida")

    def _set_status(self, text: str) -> None:
        self.status.configure(text=text)

    def _on_close(self) -> None:
        try:
            self.source.stop()
            if self.recorder is not None:
                self.recorder.close()
        finally:
            self.root.destroy()


def run_shell(source, panel_classes: Optional[List[Type[Panel]]] = None,
              recorder=None, config_path: Optional[str] = None,
              start_key: Optional[str] = None) -> None:
    root = tk.Tk()
    MonitorShell(root, source, panel_classes, recorder=recorder,
                 config_path=config_path, start_key=start_key)
    root.mainloop()


def smoke(frames: int = 60) -> int:
    """Smoke headless: construye el shell con TODOS los paneles, pasa frames del
    simulador por cada vista (build + render reales) y destruye. 0 si OK."""
    import sys
    from .protocol_top import parse_line_top
    from .simulator_top import SimulatorTop
    from .sources import SimTopSource
    if hasattr(sys.stdout, "reconfigure"):
        try:
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        except Exception:  # noqa: BLE001
            pass
    root = tk.Tk()
    root.withdraw()
    try:
        shell = MonitorShell(root, SimTopSource(rate_hz=50.0), config_path=None)
    except Exception as e:  # noqa: BLE001
        print(f"[smoke-monitor] FALLO construyendo el shell: {e}")
        root.destroy()
        return 1
    sim = SimulatorTop(rate_hz=50.0)
    names = []
    rc = 0
    for p in shell.panels:
        names.append(p.key)
        shell._select(p.key)
        for _ in range(max(3, frames // max(1, len(shell.panels)))):
            try:
                shell._pump(parse_line_top(sim.next_line()))
            except Exception as e:  # noqa: BLE001
                print(f"[smoke-monitor] FALLO en panel '{p.key}': {e}")
                rc = 1
                break
        root.update_idletasks()
    print(f"[smoke-monitor] paneles construidos+renderizados: {names}")
    print(f"[smoke-monitor] eventos de log generados: {len(shell.logbuf)}")
    try:
        shell.source.stop()
    except Exception:  # noqa: BLE001
        pass
    root.destroy()
    print("[smoke-monitor] " + ("OK" if rc == 0 else "FALLO"))
    return rc
