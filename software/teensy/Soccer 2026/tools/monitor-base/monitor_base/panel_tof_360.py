"""panel_tof_360.py — Panel TIRA 360 de los 4 ToF + cámaras (embebible en el shell).

Refactor de gui_tof_360.Tof360App a Panel: la misma tira horizontal de las 4
grillas de zonas 4×4 en ORDEN FÍSICO 360 (IZQUIERDO │ FRONTAL │ DERECHO │
TRASERO), con las cámaras frontal/trasera arriba y el aviso honesto de
orientación ("arriba en la grilla ≠ adelante del robot"). Sin ventana/loop/fuente
propios (el shell se los provee).

La LÓGICA PURA (orden de columnas, mapa sensor→columna, orientación del izquierdo
vía ZoneGrid.from_sensor, resumen de cámara) se IMPORTA de gui_tof_360 — ya está
testeada en tests/test_tof_360.py. Acá solo vive la capa Tk: build() arma los
widgets en el frame que da el shell; render(f) los actualiza.

El banner de SIMULADOR y la status bar los pone el shell global, así que acá no
van. Este panel NO manda comandos (sends_commands=False).
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk
from typing import Dict

from .gui_tof_360 import (
    CAM_COLUMN,
    ZONE_CELL_PX,
    ZONE_GRID_W,
    cam_summary,
    oriented_grid_for_sensor,
    panel_label_for_column,
    panel_sensor_for_column,
)
from .panel import Panel
from .protocol_top import TopFrame
from .shell_theme import BG, CARD, FG, LINE, MUTED, PANEL, WARN
from .zones import NO_READING_COLOR, zone_color

_CANVAS_BG = PANEL          # área de dato (canvas de zonas)
_CELL_OUTLINE = LINE
_AVISO = ("ⓘ Las 4 grillas están alineadas ENTRE SÍ; 'arriba' en la grilla ≠ "
          "adelante del robot hasta que el firmware exponga el azimut-por-zona "
          "(la rotación ~90° canónica está sin escribir).")


class Tof360Panel(Panel):
    title = "ToF 360"
    key = "tof360"
    icon = "☷"
    sends_commands = False

    def build(self, parent: tk.Frame) -> None:
        self._panels: Dict[int, dict] = {}     # col -> widgets del panel ToF
        self._cam_lbls: Dict[str, tk.Label] = {}

        self.header = tk.Label(parent, text="esperando datos…", anchor="w",
                               font=("Consolas", 11, "bold"), bg=BG, fg=FG,
                               padx=8, pady=6)
        self.header.pack(fill="x")

        body = ttk.Frame(parent, padding=8)
        body.pack(fill="both", expand=True)

        # Fila de cámaras (arriba): frontal sobre FRONTAL (col 1), trasera sobre
        # TRASERO (col 3). El resto de columnas queda vacío (placeholder).
        for col in range(4):
            slot = ttk.Frame(body)
            slot.grid(row=0, column=col, padx=6, pady=(0, 4), sticky="nsew")
            cam_key = next((k for k, c in CAM_COLUMN.items() if c == col), None)
            if cam_key is None:
                continue
            name = "Cámara FRONTAL" if cam_key == "camf" else "Cámara TRASERA"
            ttk.Label(slot, text=name, font=("Segoe UI", 9, "bold")).pack(anchor="w")
            lbl = tk.Label(slot, text="esperando…", anchor="w", justify="left",
                           font=("Consolas", 9), bg=_CANVAS_BG, fg=FG,
                           width=30, padx=6, pady=4)
            lbl.pack(anchor="w", fill="x")
            self._cam_lbls[cam_key] = lbl

        # Fila de paneles ToF (la tira 360), en orden físico de columnas.
        for col in range(4):
            self._panels[col] = self._make_panel(body, col)

        # Aviso honesto de orientación (no mentir sobre el azimut).
        tk.Label(parent, text=_AVISO, anchor="w", justify="left", wraplength=560,
                 fg=WARN, bg=CARD, font=("Segoe UI", 9), padx=8, pady=4
                 ).pack(fill="x", padx=8, pady=(0, 4))

    def _make_panel(self, parent: ttk.Frame, col: int) -> dict:
        wrap = ttk.Frame(parent, relief="solid", borderwidth=1, padding=4)
        wrap.grid(row=1, column=col, padx=6, pady=4, sticky="nw")
        label = panel_label_for_column(col)
        sensor_idx = panel_sensor_for_column(col)
        ttk.Label(wrap, font=("Segoe UI", 9, "bold"),
                  text=f"{label} · ToF {sensor_idx}").pack()
        w = ZONE_GRID_W * ZONE_CELL_PX
        h = ZONE_GRID_W * ZONE_CELL_PX
        canvas = tk.Canvas(wrap, width=w, height=h, bg=_CANVAS_BG,
                           highlightthickness=0)
        canvas.pack(pady=3)
        cells, texts = [], []
        for r in range(ZONE_GRID_W):
            for c in range(ZONE_GRID_W):
                x0, y0 = c * ZONE_CELL_PX, r * ZONE_CELL_PX
                cells.append(canvas.create_rectangle(
                    x0, y0, x0 + ZONE_CELL_PX - 1, y0 + ZONE_CELL_PX - 1,
                    fill=NO_READING_COLOR, outline=_CELL_OUTLINE))
                texts.append(canvas.create_text(
                    x0 + ZONE_CELL_PX / 2, y0 + ZONE_CELL_PX / 2,
                    text="", fill=_CANVAS_BG, font=("Consolas", 7)))
        caption = ttk.Label(wrap, font=("Consolas", 8), text="(esperando datos…)")
        caption.pack()
        return {"wrap": wrap, "canvas": canvas, "cells": cells, "texts": texts,
                "caption": caption, "sensor_idx": sensor_idx, "label": label}

    # ── Render ────────────────────────────────────────────────────────────────
    def render(self, f: TopFrame) -> None:
        self._render_cameras(f)
        self._render_strip(f.tof)
        self.header.configure(
            text=(f"seq={f.seq}  v{f.v}     "
                  f"zonas ToF: {'SÍ' if f.tof.zones else 'pendiente firmware (no viajan aún)'}"))

    def _render_cameras(self, f: TopFrame) -> None:
        for key, cam in (("camf", f.camf), ("camb", f.camb)):
            lbl = self._cam_lbls.get(key)
            if lbl is not None:
                lbl.configure(text=cam_summary(cam))

    def _render_strip(self, tof) -> None:
        zones = tof.zones if tof.zones else None
        for col in range(4):
            panel = self._panels[col]
            idx = panel["sensor_idx"]
            if zones is None or idx >= len(zones):
                panel["caption"].configure(
                    text=f"{panel['label']} · ToF {idx}  (sin zonas)")
                continue
            grid = oriented_grid_for_sensor(zones[idx], idx, width=ZONE_GRID_W)
            cells, texts = panel["cells"], panel["texts"]
            canvas = panel["canvas"]
            for k, val in enumerate(grid.cells):
                if k >= len(cells):
                    break
                color = NO_READING_COLOR if val is None else zone_color(val)
                canvas.itemconfig(cells[k], fill=color)
                canvas.itemconfig(texts[k], text="" if val is None else str(val))
            panel["caption"].configure(
                text=f"{panel['label']} · ToF {idx}  ({grid.valid_count}/{len(grid.cells)})")
