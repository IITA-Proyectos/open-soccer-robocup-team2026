"""panel_health.py — Panel de SALUD por sensor (embebible en el shell).

Refactor de gui_top_health a Panel: mismo tablero de semáforos + grilla de zonas
ToF + botones de config, pero sin ventana/loop/fuente propios (el shell se los
provee). Es el MOLDE de cómo se hace un Panel: build() arma los widgets en el
frame que da el shell; render(f) actualiza; los comandos van por self.ctx.send.
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk
from typing import Dict

from .health import STATUS_COLOR, STATUS_LABEL, Status, counts, evaluate
from .panel import Panel
from .protocol_top import TopFrame
from .shell_theme import BG, CARD, FG, MONO, MUTED, UI_B
from .tooltip import attach_tooltip
from .tooltips_text import tip
from .zones import NO_READING_COLOR, TOF_LABELS, ZoneGrid, zone_color

TOGGLES = [
    ("Cámara frontal", "CAM F ON", "CAM F OFF"),
    ("Cámara trasera", "CAM B ON", "CAM B OFF"),
    # BNO por ROL en la fusión (idx0/idx1), no "izq/der" (los comandos BNO L/R del
    # firmware son idx0/idx1 → no se tocan). Bus/dirección = constante de build, no
    # viaja en telemetría → no se rotula (mentiría según el binario flasheado).
    ("BNO primario", "BNO L ON", "BNO L OFF"),
    ("BNO secundario", "BNO R ON", "BNO R OFF"),
    ("Ultrasonido", "US ON", "US OFF"),
]
TOF_POS = ("FRONT", "RIGHT", "BACK", "LEFT")
NUM_TOF_BTN = 4
ZONE_CELL_PX = 30
ZONE_GRID_W = 4


class HealthPanel(Panel):
    title = "Salud"
    key = "salud"
    icon = "✚"
    sends_commands = True

    def build(self, parent: tk.Frame) -> None:
        self._enabled: Dict[str, bool] = {}
        self._tiles: Dict[str, dict] = {}
        self._zone_cv: Dict[int, dict] = {}

        self.header = tk.Label(parent, text="esperando datos…", anchor="w",
                               font=("Consolas", 11, "bold"), bg=BG, fg=FG, padx=8, pady=6)
        self.header.pack(fill="x")

        body = ttk.Frame(parent, padding=8)
        body.pack(fill="both", expand=True)
        ttk.Label(body, text="SALUD POR SENSOR", style="Muted.TLabel").grid(row=0, column=0, sticky="w")
        self._tiles_frame = ttk.Frame(body)
        self._tiles_frame.grid(row=1, column=0, sticky="nw", padx=(0, 14))

        ttk.Label(body, text="ZONAS ToF (4×4 · 🔴 cerca 🟢 lejos ⬛ sin lectura · mm)",
                  style="Muted.TLabel").grid(row=0, column=1, sticky="w")
        self._zones_frame = ttk.Frame(body)
        self._zones_frame.grid(row=1, column=1, sticky="nw")
        self._zones_note = ttk.Label(self._zones_frame, style="Muted.TLabel",
                                     text="esperando datos…")
        self._zones_note.grid(row=0, column=0, sticky="w")

        cfg = ttk.LabelFrame(parent, text="CONFIG (escribe a la placa)", padding=6)
        cfg.pack(fill="x", padx=8, pady=(0, 6))
        self._build_config(cfg)

    def _build_config(self, parent: ttk.Frame) -> None:
        row = ttk.Frame(parent); row.pack(fill="x")
        for label, on_cmd, off_cmd in TOGGLES:
            self._enabled[label] = True
            b = ttk.Button(row, width=16)
            b.configure(text=f"{label}: ON",
                        command=lambda l=label, o=on_cmd, f=off_cmd, btn=b: self._toggle(l, o, f, btn))
            b.pack(side="left", padx=2)
            attach_tooltip(b, tip(on_cmd))
        tofrow = ttk.Frame(parent); tofrow.pack(fill="x", pady=(4, 0))
        for n in range(NUM_TOF_BTN):
            cell = ttk.Frame(tofrow); cell.pack(side="left", padx=4)
            key = f"ToF {n}"; self._enabled[key] = True
            b = ttk.Button(cell, width=11)
            b.configure(text=f"ToF{n}: ON",
                        command=lambda k=key, nn=n, btn=b: self._toggle(k, f"TOF {nn} ON", f"TOF {nn} OFF", btn))
            b.pack()
            attach_tooltip(b, tip("TOF ON"))
            var = tk.StringVar(value="POS")
            om = ttk.OptionMenu(cell, var, "POS", *TOF_POS,
                                command=lambda pos, nn=n: self.ctx.send(f"TOF {nn} POS {pos}"))
            om.pack()
            attach_tooltip(om, tip("TOF POS"))
        cfgrow = ttk.Frame(parent); cfgrow.pack(fill="x", pady=(4, 0))
        for label, cmd in (("Guardar EEPROM", "CFG SAVE"), ("Recargar", "CFG LOAD"),
                           ("Reset defaults", "CFG RESET")):
            cb = ttk.Button(cfgrow, text=label, command=lambda c=cmd: self.ctx.send(c))
            cb.pack(side="left", padx=2)
            attach_tooltip(cb, tip(cmd))

    def _toggle(self, key: str, on_cmd: str, off_cmd: str, btn: ttk.Button) -> None:
        now_on = not self._enabled.get(key, True)
        self._enabled[key] = now_on
        self.ctx.send(on_cmd if now_on else off_cmd)
        short = key.replace("Cámara ", "Cám ").replace("Ultrasonido", "US")
        btn.configure(text=f"{short}: {'ON' if now_on else 'OFF'}")

    # ── Render ──────────────────────────────────────────────────────────────
    def render(self, f: TopFrame) -> None:
        items = evaluate(f)
        self._render_tiles(items)
        self._render_zones(f.tof)
        c = counts(items)
        self.header.configure(
            text=(f"seq={f.seq}  v{f.v}     OK={c[Status.OK]}  REVISAR={c[Status.WARN]}  "
                  f"FALLA={c[Status.DEAD]}  SIN DATO={c[Status.NODATA]}"))

    def _render_tiles(self, items) -> None:
        ncol = 3
        for idx, it in enumerate(items):
            t = self._tiles.get(it.key)
            if t is None:
                t = self._make_tile(idx // ncol, idx % ncol)
                self._tiles[it.key] = t
            t["frame"].configure(bg=STATUS_COLOR[it.status])
            t["title"].configure(text=it.label, bg=STATUS_COLOR[it.status])
            t["badge"].configure(text=STATUS_LABEL[it.status], bg=STATUS_COLOR[it.status])
            t["detail"].configure(text=it.detail, bg=STATUS_COLOR[it.status])

    def _make_tile(self, r: int, c: int) -> dict:
        frame = tk.Frame(self._tiles_frame, bd=1, relief="solid", padx=6, pady=4,
                         width=200, height=58)
        frame.grid(row=r, column=c, padx=3, pady=3, sticky="nsew")
        frame.pack_propagate(False)
        title = tk.Label(frame, anchor="w", font=("Segoe UI", 9, "bold"), fg="#fff", justify="left")
        title.pack(anchor="w")
        badge = tk.Label(frame, anchor="w", font=("Consolas", 8, "bold"), fg="#fff")
        badge.pack(anchor="w")
        detail = tk.Label(frame, anchor="w", font=("Consolas", 8), fg="#eee", wraplength=190, justify="left")
        detail.pack(anchor="w")
        return {"frame": frame, "title": title, "badge": badge, "detail": detail}

    def _render_zones(self, tof) -> None:
        if not tof.zones:
            self._zones_note.configure(text="pendiente firmware: las zonas no viajan todavía")
            return
        self._zones_note.grid_remove()
        for i, sensor in enumerate(tof.zones):
            grid = ZoneGrid.from_sensor(sensor, i, width=ZONE_GRID_W)
            cv = self._zone_cv.get(i)
            if cv is None:
                cv = self._make_zone_canvas(i, grid)
                self._zone_cv[i] = cv
            for k, val in enumerate(grid.cells):
                if k >= len(cv["cells"]):
                    break
                color = NO_READING_COLOR if val is None else zone_color(val)
                cv["canvas"].itemconfig(cv["cells"][k], fill=color)
                cv["canvas"].itemconfig(cv["texts"][k], text="" if val is None else str(val))
            cv["caption"].configure(
                text=f"ToF {i} · {TOF_LABELS.get(i, '?')}  ({grid.valid_count}/{len(grid.cells)})")

    def _make_zone_canvas(self, idx: int, grid: ZoneGrid) -> dict:
        wrap = ttk.Frame(self._zones_frame)
        wrap.grid(row=idx // 2, column=idx % 2, padx=6, pady=6, sticky="nw")
        w = grid.width * ZONE_CELL_PX
        h = grid.height * ZONE_CELL_PX
        canvas = tk.Canvas(wrap, width=w, height=h, bg="#0b0e11", highlightthickness=0)
        canvas.pack()
        cells, texts = [], []
        for r in range(grid.height):
            for c in range(grid.width):
                x0, y0 = c * ZONE_CELL_PX, r * ZONE_CELL_PX
                cells.append(canvas.create_rectangle(x0, y0, x0 + ZONE_CELL_PX - 1,
                                                     y0 + ZONE_CELL_PX - 1,
                                                     fill=NO_READING_COLOR, outline="#1a1d20"))
                texts.append(canvas.create_text(x0 + ZONE_CELL_PX / 2, y0 + ZONE_CELL_PX / 2,
                                                text="", fill="#0b0e11", font=("Consolas", 7)))
        caption = ttk.Label(wrap, font=("Consolas", 8),
                            text=f"ToF {idx} · {TOF_LABELS.get(idx, '?')}")
        caption.pack()
        return {"canvas": canvas, "cells": cells, "texts": texts, "caption": caption}
