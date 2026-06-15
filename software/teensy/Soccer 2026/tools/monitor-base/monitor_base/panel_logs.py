"""panel_logs.py — Panel de LOGS del monitor unificado.

Muestra el historial de eventos (conexión, comandos, anomalías) y un latido
periódico de los datos recibidos, coloreado por nivel, con filtros y una franja
con el último frame. Lee el LogBuffer y el historial de frames que le ENTREGA el
shell (attach), no abre nada por su cuenta.
"""
from __future__ import annotations

import time
import tkinter as tk
from tkinter import ttk
from typing import Deque, Optional

from .panel import LEVELS, LogBuffer, Panel
from .protocol_top import TopFrame
from .shell_theme import (BAD, BG, FG, INFO, LINE, MONO, MONO_SM, MUTED, OK,
                          PANEL, WARN)

LEVEL_COLOR = {"info": FG, "ok": OK, "warn": WARN, "bad": BAD, "cmd": INFO}


class LogsPanel(Panel):
    title = "Logs"
    key = "logs"
    icon = "▤"

    def __init__(self, parent, ctx):
        self._logbuf: Optional[LogBuffer] = None
        self._hist: Optional[Deque] = None
        self._last_seq = -1
        self._filter: Optional[str] = None
        self._autoscroll = True
        super().__init__(parent, ctx)

    def attach(self, logbuf: LogBuffer, frame_hist: Deque) -> None:
        self._logbuf = logbuf
        self._hist = frame_hist

    def build(self, parent: tk.Frame) -> None:
        parent.columnconfigure(0, weight=1)
        parent.rowconfigure(2, weight=1)

        # Franja del último frame.
        self.live = tk.Label(parent, bg=PANEL, fg=FG, font=MONO, anchor="w",
                             padx=8, pady=4, text="sin datos")
        self.live.grid(row=0, column=0, sticky="we", padx=6, pady=(6, 0))

        # Toolbar: filtros + acciones.
        bar = ttk.Frame(parent, style="Bar.TFrame")
        bar.grid(row=1, column=0, sticky="we", padx=6, pady=4)
        ttk.Label(bar, text="filtro:", style="Bar.TLabel").pack(side="left", padx=(4, 4))
        self._fbtns = {}
        for lab, lvl in (("TODO", None), ("info", "info"), ("ok", "ok"),
                         ("warn", "warn"), ("bad", "bad"), ("cmd", "cmd")):
            b = ttk.Button(bar, text=lab, width=6,
                           command=lambda l=lvl: self._set_filter(l))
            b.pack(side="left", padx=1)
            self._fbtns[lvl] = b
        self._auto_btn = ttk.Button(bar, text="⏸ autoscroll", command=self._toggle_auto)
        self._auto_btn.pack(side="right", padx=2)
        ttk.Button(bar, text="🗑 limpiar", command=self._clear).pack(side="right", padx=2)
        ttk.Button(bar, text="⎘ exportar", command=self._export).pack(side="right", padx=2)

        # Texto de logs.
        wrap = tk.Frame(parent, bg=BG)
        wrap.grid(row=2, column=0, sticky="nsew", padx=6, pady=(0, 6))
        wrap.rowconfigure(0, weight=1); wrap.columnconfigure(0, weight=1)
        self.text = tk.Text(wrap, bg=PANEL, fg=FG, font=MONO_SM, relief="flat",
                            insertbackground=FG, wrap="none", state="disabled",
                            highlightthickness=0)
        self.text.grid(row=0, column=0, sticky="nsew")
        sb = ttk.Scrollbar(wrap, command=self.text.yview)
        sb.grid(row=0, column=1, sticky="ns")
        self.text.configure(yscrollcommand=sb.set)
        for lvl, col in LEVEL_COLOR.items():
            self.text.tag_configure(lvl, foreground=col)
        self.text.tag_configure("ts", foreground=MUTED)
        self._refresh_filter_buttons()

    # ── Acciones ────────────────────────────────────────────────────────────
    def _set_filter(self, lvl):
        self._filter = lvl
        self._last_seq = -1            # forzar redibujo
        self._refresh_filter_buttons()

    def _refresh_filter_buttons(self):
        for lvl, b in self._fbtns.items():
            b.configure(style="NavActive.TButton" if lvl == self._filter else "TButton")

    def _toggle_auto(self):
        self._autoscroll = not self._autoscroll
        self._auto_btn.configure(text=("⏸ autoscroll" if self._autoscroll else "▶ autoscroll"))

    def _clear(self):
        if self._logbuf:
            self._logbuf.clear()
        self._last_seq = -1

    def _export(self):
        if not self._logbuf:
            return
        fn = time.strftime("monitor-log-%Y%m%d-%H%M%S.txt")
        try:
            with open(fn, "w", encoding="utf-8") as f:
                for t, lvl, msg in self._logbuf.entries():
                    f.write(f"{time.strftime('%H:%M:%S', time.localtime(t))} [{lvl}] {msg}\n")
            self.ctx.log("ok", f"logs exportados a {fn}")
        except Exception as e:  # noqa: BLE001
            self.ctx.log("bad", f"no pude exportar: {e}")

    # ── Render ──────────────────────────────────────────────────────────────
    def on_show(self):
        self._last_seq = -1            # redibujar al entrar

    def render(self, f: TopFrame) -> None:
        # Franja del último frame.
        s = f.snap
        self.live.configure(text=(
            f"seq {f.seq:<6} t={f.t_ms:<8} v{f.v}   hdg={f.imu.heading_deg:+6.1f}° "
            f"valid={'Y' if f.imu.heading_valid else 'N'}   snap={'OK' if s.valid else '--'}   "
            f"pelota={'SÍ' if s.ball_visible else 'no'}   "
            f"min_obst={'--' if s.min_obstacle_mm is None else str(s.min_obstacle_mm)+'mm'}   "
            f"árbitro={s.referee_name}"))
        # Log (solo si cambió).
        if self._logbuf is None or self._logbuf.seq == self._last_seq:
            return
        self._last_seq = self._logbuf.seq
        self.text.configure(state="normal")
        self.text.delete("1.0", "end")
        for t, lvl, msg in self._logbuf.entries(self._filter, limit=400):
            ts = time.strftime("%H:%M:%S", time.localtime(t))
            self.text.insert("end", f"{ts} ", ("ts",))
            self.text.insert("end", f"{lvl:>4} ", (lvl,))
            self.text.insert("end", f"{msg}\n", (lvl,))
        if self._autoscroll:
            self.text.see("end")
        self.text.configure(state="disabled")
