"""panel_timeline.py — Panel TIMELINE / caja-negra del snapshot (embebible).

Refactor de gui_timeline (MonitorTimelineApp) a Panel: las mismas bandas
temporales apiladas (valid / ball_visible / heading_valid / arcos / min_obst /
referee) + el tablero de FLAPPING, pero sin ventana/loop/fuente/banner propios
(el shell se los da). El histórico vive ACÁ, en la instancia: un SignalHistory
(ring buffer) al que render(f) le hace push del frame.

La LÓGICA PURA (SignalHistory, transitions, TimelineSample, las listas de
señales y los colores de banda) se IMPORTA de gui_timeline — ya está host-testeada,
no se duplica. Este módulo sólo arma los widgets Tk y dibuja.
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk
from typing import Dict, Optional

from .gui_timeline import (BOOL_SIGNALS, GAP_COLOR, NUM_LINE, NUM_SIGNALS,
                           REF_COLORS, SignalHistory, _bool_color)
from .panel import Panel
from .protocol_top import REFEREE, TopFrame
from .shell_theme import BG, FG, LINE, MUTED, PANEL

# Carril/etiqueta sobre el tema oscuro del shell (los colores de banda
# ON/OFF/REF vienen de gui_timeline: son el lenguaje visual de esta vista).
ROW_LABEL_FG = FG
GRID_LINE = LINE


class TimelinePanel(Panel):
    title = "Timeline"
    key = "timeline"
    icon = "≣"
    sends_commands = False

    # Geometría del dibujo (idéntica a la vista origen).
    ROW_H = 22              # alto de cada banda booleana
    NUM_H = 46             # alto del carril numérico (min_obstacle)
    LABEL_W = 130          # ancho de la columna de etiquetas
    PLOT_W = 900           # ancho del área de dibujo
    PAD = 4

    def build(self, parent: tk.Frame) -> None:
        # Estado (vive en la instancia, igual que las estelas/config de otros paneles).
        self.hist = SignalHistory(maxlen=300)
        self.frame_count = 0
        self.last_seq = -1
        self.dropped = 0
        self._row_y: Dict[str, float] = {}
        self._flap_labels: Dict[str, ttk.Label] = {}
        self._dyn_tag = "dyn"

        self.header = tk.Label(parent, text="esperando datos…", anchor="w",
                               font=("Consolas", 11, "bold"), bg=BG, fg=FG,
                               padx=8, pady=6)
        self.header.pack(fill="x")

        body = ttk.Frame(parent, padding=8)
        body.pack(fill="both", expand=True)

        # Altura total = N bandas booleanas + 1 carril numérico + 1 banda árbitro.
        n_rows = len(BOOL_SIGNALS) + 1  # +1 árbitro
        self._plot_h = n_rows * self.ROW_H + self.NUM_H + 2 * self.PAD
        total_w = self.LABEL_W + self.PLOT_W + 2 * self.PAD
        # width/height fijos + pack_propagate(False): anti-parpadeo (el canvas no
        # debe crecer con el texto largo del header).
        wrap = ttk.Frame(body, width=total_w, height=self._plot_h)
        wrap.pack(anchor="nw")
        wrap.pack_propagate(False)

        self.canvas = tk.Canvas(wrap, width=total_w, height=self._plot_h,
                                bg=PANEL, highlightthickness=0)
        self.canvas.pack()
        self._build_lanes()

        # Tablero de flapping (transiciones por señal).
        flap = ttk.LabelFrame(
            body, text="FLAPPING (flancos en la ventana — más alto = más parpadeo)",
            padding=6)
        flap.pack(fill="x", pady=(8, 0))
        for col, (name, label) in enumerate(BOOL_SIGNALS + [("referee_cmd", "referee")]):
            cell = ttk.Frame(flap)
            cell.grid(row=0, column=col, padx=8, sticky="w")
            ttk.Label(cell, text=label, font=("Segoe UI", 8)).pack()
            lbl = ttk.Label(cell, text="0", font=("Consolas", 14, "bold"))
            lbl.pack()
            self._flap_labels[name] = lbl

    def _build_lanes(self) -> None:
        """Chasis estático: etiquetas + divisores. Guarda el y0 de cada carril.
        Las bandas dinámicas se pintan en render()."""
        y = self.PAD
        for name, label in BOOL_SIGNALS:
            self._row_y[name] = y
            self.canvas.create_text(self.PAD, y + self.ROW_H / 2, anchor="w",
                                    text=label, fill=ROW_LABEL_FG,
                                    font=("Consolas", 9))
            self.canvas.create_line(self.LABEL_W, y, self.LABEL_W + self.PLOT_W, y,
                                    fill=GRID_LINE)
            y += self.ROW_H
        # Carril numérico (min_obstacle).
        self._num_y = y
        self.canvas.create_text(self.PAD, y + self.NUM_H / 2, anchor="w",
                                text=NUM_SIGNALS[0][1], fill=ROW_LABEL_FG,
                                font=("Consolas", 9))
        self.canvas.create_line(self.LABEL_W, y, self.LABEL_W + self.PLOT_W, y,
                                fill=GRID_LINE)
        y += self.NUM_H
        # Carril del árbitro.
        self._ref_y = y
        self.canvas.create_text(self.PAD, y + self.ROW_H / 2, anchor="w",
                                text="referee_cmd", fill=ROW_LABEL_FG,
                                font=("Consolas", 9))
        self.canvas.create_line(self.LABEL_W, y, self.LABEL_W + self.PLOT_W, y,
                                fill=GRID_LINE)

    # ── Render ────────────────────────────────────────────────────────────────
    def render(self, f: TopFrame) -> None:
        # El ring buffer vive acá: cada frame se apila (era _consume en el origen).
        self.frame_count += 1
        if self.last_seq >= 0:
            gap = f.seq - self.last_seq - 1
            if gap > 0:
                self.dropped += gap
        self.last_seq = f.seq
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
        # Ancho de cada muestra: llenamos el área con las muestras del ring.
        denom = max(1, self.hist.maxlen)
        col_w = plot_w / denom
        # Las muestras se alinean a la DERECHA (lo más nuevo a la derecha).
        x_start = x0 + plot_w - n * col_w

        # Bandas booleanas.
        for name, _label in BOOL_SIGNALS:
            y = self._row_y[name]
            self._draw_bool_band(self.hist.series(name), x_start, y, col_w)

        # Sparkline numérico (min_obstacle_mm).
        self._draw_num_sparkline(self.hist.series("min_obstacle_mm"),
                                 x_start, self._num_y, col_w)

        # Banda del árbitro (categórica).
        self._draw_ref_band(self.hist.series("referee_cmd"), x_start,
                            self._ref_y, col_w)

        # Tablero de flapping.
        for name in self._flap_labels:
            self._flap_labels[name].configure(text=str(self.hist.transitions(name)))

        last = self.hist.latest
        span = self.hist.span_ms() / 1000.0
        rate = (n - 1) / span if span > 0 else 0.0
        ref_name = REFEREE.get(last.referee_cmd, f"?{last.referee_cmd}") if last else "?"
        self.header.configure(
            text=(f"seq={self.last_seq}  {rate:.0f} Hz  frames={self.frame_count}  "
                  f"perdidos={self.dropped}  muestras={n}/{self.hist.maxlen}  "
                  f"ventana={span:.1f}s  árbitro={ref_name}"))

    def _draw_bool_band(self, series, x_start: float, y: float, col_w: float) -> None:
        h = self.ROW_H - 4
        for i, v in enumerate(series):
            x = x_start + i * col_w
            self.canvas.create_rectangle(
                x, y + 2, x + col_w + 0.5, y + 2 + h,
                fill=_bool_color(bool(v)), width=0, tags=self._dyn_tag)

    def _draw_ref_band(self, series, x_start: float, y: float, col_w: float) -> None:
        h = self.ROW_H - 4
        for i, v in enumerate(series):
            x = x_start + i * col_w
            self.canvas.create_rectangle(
                x, y + 2, x + col_w + 0.5, y + 2 + h,
                fill=REF_COLORS.get(int(v), GAP_COLOR), width=0, tags=self._dyn_tag)

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
                # corte: marca un punto gris en el piso y rompe la polilínea.
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
        # Etiqueta de rango a la derecha.
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
