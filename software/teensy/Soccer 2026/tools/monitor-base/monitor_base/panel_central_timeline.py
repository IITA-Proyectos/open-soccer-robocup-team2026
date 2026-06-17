"""panel_central_timeline.py — Panel TIMELINE / caja-negra de la CENTRAL.

Versión CENTRAL del TimelinePanel (panel_timeline.py): las mismas bandas
temporales apiladas + sparklines + tablero de FLAPPING, pero con las señales del
CentralFrame (el "cerebro"):

  • Bandas booleanas : match, has_pose, heading_valid, ball_vis, line_valid,
                       top_fresh, down_fresh.
  • Sparklines numéricos : loop_ema_us (carga del loop), my_x, my_y (pose XY).
  • Banda de texto : fsm_state (estado actual de la FSM).
  • Tablero FLAPPING : flancos por señal booleana + cambios de fsm_state.

La LÓGICA PURA (CentralSignalHistory, transitions, CentralTimelineSample, listas
de señales) se IMPORTA de central_timeline (host-testeada, no se duplica). Este
módulo sólo arma los widgets Tk y dibuja. Reusa los colores de banda de
gui_timeline (mismo lenguaje visual que el timeline TOP).

Consume CentralFrame (protocol_central.py, schema v1). NO manda comandos.
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk
from typing import Dict, List

from .central_timeline import (BOOL_SIGNALS, NUM_SIGNALS, CentralSignalHistory)
from .gui_timeline import GAP_COLOR, NUM_LINE, _bool_color
from .panel import Panel
from .protocol_central import CentralFrame
from .shell_theme import BG, FG, LINE, MUTED, PANEL

# Carril/etiqueta sobre el tema oscuro del shell.
ROW_LABEL_FG = FG
GRID_LINE = LINE

# Color de fondo de la banda de texto (fsm_state).
FSM_BAND_BG = "#1d2a36"
FSM_TEXT_FG = "#39d3a0"


class CentralTimelinePanel(Panel):
    title = "Timeline CENTRAL"
    key = "central_timeline"
    icon = "≣"
    board = "central"
    sends_commands = False

    # Geometría del dibujo.
    ROW_H = 22              # alto de cada banda booleana
    NUM_H = 40             # alto de cada carril numérico
    TXT_H = 26             # alto del carril de texto (fsm_state)
    LABEL_W = 130          # ancho de la columna de etiquetas
    PLOT_W = 900           # ancho del área de dibujo
    PAD = 4

    def build(self, parent: tk.Frame) -> None:
        # Estado (vive en la instancia).
        self.hist = CentralSignalHistory(maxlen=300)
        self.frame_count = 0
        self.last_seq = -1
        self.dropped = 0
        self._row_y: Dict[str, float] = {}
        self._num_y: Dict[str, float] = {}
        self._flap_labels: Dict[str, ttk.Label] = {}
        self._dyn_tag = "dyn"

        self.header = tk.Label(parent, text="esperando datos…", anchor="w",
                               font=("Consolas", 11, "bold"), bg=BG, fg=FG,
                               padx=8, pady=6)
        self.header.pack(fill="x")

        body = ttk.Frame(parent, padding=8)
        body.pack(fill="both", expand=True)

        # Altura total = N bandas bool + M carriles numéricos + 1 banda fsm_state.
        n_bool = len(BOOL_SIGNALS)
        n_num = len(NUM_SIGNALS)
        self._plot_h = (n_bool * self.ROW_H + n_num * self.NUM_H
                        + self.TXT_H + 2 * self.PAD)
        total_w = self.LABEL_W + self.PLOT_W + 2 * self.PAD
        # width/height fijos + pack_propagate(False): anti-parpadeo.
        wrap = ttk.Frame(body, width=total_w, height=self._plot_h)
        wrap.pack(anchor="nw")
        wrap.pack_propagate(False)

        self.canvas = tk.Canvas(wrap, width=total_w, height=self._plot_h,
                                bg=PANEL, highlightthickness=0)
        self.canvas.pack()
        self._build_lanes()

        # Tablero de flapping (transiciones por señal booleana + fsm_state).
        flap = ttk.LabelFrame(
            body, text="FLAPPING (flancos en la ventana — más alto = más parpadeo)",
            padding=6)
        flap.pack(fill="x", pady=(8, 0))
        flap_cols: List = list(BOOL_SIGNALS) + [("fsm_state", "fsm_state")]
        for col, (name, label) in enumerate(flap_cols):
            cell = ttk.Frame(flap)
            cell.grid(row=0, column=col, padx=8, sticky="w")
            ttk.Label(cell, text=label, font=("Segoe UI", 8)).pack()
            lbl = ttk.Label(cell, text="0", font=("Consolas", 14, "bold"))
            lbl.pack()
            self._flap_labels[name] = lbl

    def _build_lanes(self) -> None:
        """Chasis estático: etiquetas + divisores. Guarda el y0 de cada carril."""
        y = self.PAD
        # Bandas booleanas.
        for name, label in BOOL_SIGNALS:
            self._row_y[name] = y
            self.canvas.create_text(self.PAD, y + self.ROW_H / 2, anchor="w",
                                    text=label, fill=ROW_LABEL_FG,
                                    font=("Consolas", 9))
            self.canvas.create_line(self.LABEL_W, y, self.LABEL_W + self.PLOT_W, y,
                                    fill=GRID_LINE)
            y += self.ROW_H
        # Carriles numéricos (loop_ema_us, my_x, my_y).
        for name, label in NUM_SIGNALS:
            self._num_y[name] = y
            self.canvas.create_text(self.PAD, y + self.NUM_H / 2, anchor="w",
                                    text=label, fill=ROW_LABEL_FG,
                                    font=("Consolas", 9))
            self.canvas.create_line(self.LABEL_W, y, self.LABEL_W + self.PLOT_W, y,
                                    fill=GRID_LINE)
            y += self.NUM_H
        # Carril de texto (fsm_state actual).
        self._fsm_y = y
        self.canvas.create_text(self.PAD, y + self.TXT_H / 2, anchor="w",
                                text="fsm_state", fill=ROW_LABEL_FG,
                                font=("Consolas", 9))
        self.canvas.create_line(self.LABEL_W, y, self.LABEL_W + self.PLOT_W, y,
                                fill=GRID_LINE)

    # ── Render ────────────────────────────────────────────────────────────────
    def render(self, f: CentralFrame) -> None:
        # El ring buffer vive acá: cada frame se apila.
        self.frame_count += 1
        seq = int(getattr(f, "seq", 0))
        if self.last_seq >= 0:
            gap = seq - self.last_seq - 1
            if gap > 0:
                self.dropped += gap
        self.last_seq = seq
        self.hist.push(f)
        self._render()

    def _render(self) -> None:
        self.canvas.delete(self._dyn_tag)
        samples = self.hist.samples
        n = len(samples)
        if n == 0:
            return
        x0 = self.LABEL_W
        plot_w = self.PLOT_W
        denom = max(1, self.hist.maxlen)
        col_w = plot_w / denom
        # Las muestras se alinean a la DERECHA (lo más nuevo a la derecha).
        x_start = x0 + plot_w - n * col_w

        # Bandas booleanas.
        for name, _label in BOOL_SIGNALS:
            y = self._row_y[name]
            self._draw_bool_band(self.hist.series(name), x_start, y, col_w)

        # Sparklines numéricos.
        for name, _label in NUM_SIGNALS:
            self._draw_num_sparkline(self.hist.series(name), x_start,
                                     self._num_y[name], col_w)

        # Banda de texto: el fsm_state ACTUAL (último).
        self._draw_fsm_band(x_start, col_w)

        # Tablero de flapping.
        for name in self._flap_labels:
            self._flap_labels[name].configure(text=str(self.hist.transitions(name)))

        last = self.hist.latest
        span = self.hist.span_ms() / 1000.0
        rate = (n - 1) / span if span > 0 else 0.0
        state = last.fsm_state if last else "?"
        match = "PLAY" if (last and last.match) else "STOP"
        self.header.configure(
            text=(f"seq={self.last_seq}  {rate:.0f} Hz  frames={self.frame_count}  "
                  f"perdidos={self.dropped}  muestras={n}/{self.hist.maxlen}  "
                  f"ventana={span:.1f}s  estado={state}  árbitro={match}"))

    def _draw_bool_band(self, series, x_start: float, y: float, col_w: float) -> None:
        h = self.ROW_H - 4
        for i, v in enumerate(series):
            x = x_start + i * col_w
            self.canvas.create_rectangle(
                x, y + 2, x + col_w + 0.5, y + 2 + h,
                fill=_bool_color(bool(v)), width=0, tags=self._dyn_tag)

    def _draw_fsm_band(self, x_start: float, col_w: float) -> None:
        """Banda de texto: una franja con el estado FSM actual + flancos."""
        y = self._fsm_y
        h = self.TXT_H - 4
        last = self.hist.latest
        state = last.fsm_state if last else "—"
        self.canvas.create_rectangle(
            x_start, y + 2, x_start + (self.LABEL_W + self.PLOT_W) - x_start, y + 2 + h,
            fill=FSM_BAND_BG, width=0, tags=self._dyn_tag)
        self.canvas.create_text(
            x_start + 8, y + 2 + h / 2, anchor="w",
            text=f"{state}  ({self.hist.transitions('fsm_state')} cambios)",
            fill=FSM_TEXT_FG, font=("Consolas", 11, "bold"), tags=self._dyn_tag)

    def _draw_num_sparkline(self, series, x_start: float, y: float,
                            col_w: float) -> None:
        h = self.NUM_H - 6
        vals = [v for v in series if v is not None]
        if vals:
            lo, hi = min(vals), max(vals)
            if hi <= lo:
                hi = lo + 1
        else:
            lo, hi = 0, 1
        pts: list = []
        for i, v in enumerate(series):
            x = x_start + i * col_w + col_w / 2
            if v is None:
                if len(pts) >= 4:
                    self.canvas.create_line(*pts, fill=NUM_LINE, width=1,
                                            tags=self._dyn_tag)
                pts = []
                self.canvas.create_oval(x - 1, y + h, x + 1, y + h + 2,
                                        fill=GAP_COLOR, width=0, tags=self._dyn_tag)
                continue
            frac = (v - lo) / (hi - lo)
            yy = y + 3 + (1 - frac) * h
            pts.extend([x, yy])
        if len(pts) >= 4:
            self.canvas.create_line(*pts, fill=NUM_LINE, width=1, tags=self._dyn_tag)
        # Etiqueta de rango a la derecha (lo/hi).
        self.canvas.create_text(x_start - 2, y + 8, anchor="e",
                                text=f"{hi}", fill=MUTED,
                                font=("Consolas", 7), tags=self._dyn_tag)
        self.canvas.create_text(x_start - 2, y + h, anchor="e",
                                text=f"{lo}", fill=MUTED,
                                font=("Consolas", 7), tags=self._dyn_tag)

    # on_show: redibujar al entrar (los datos viejos del ring siguen vigentes).
    def on_show(self) -> None:
        if len(self.hist):
            self._render()
