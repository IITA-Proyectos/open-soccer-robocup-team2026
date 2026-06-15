"""panel_tof_setup.py — Panel de CONFIGURACIÓN de los ToF de la placa TOP
(embebible en el shell unificado).

Refactor de gui_tof_setup.TofSetupApp a Panel: mismas 4 cards (grilla 4×4
clickeable + selector de posición + rotar/espejo + on/off), el panel de
PARED+CANCHA que sugiere qué filas vetar, y los botones de Guardar/Cargar .json +
"Bajar a la placa" — pero sin ventana/loop/fuente propios (el shell se los
provee). La config y los Vars viven en la instancia; los comandos a la placa van
por self.ctx.send (los que el firmware soporta HOY) y los pendientes se avisan
con self.ctx.log. El banner de simulador y la status bar los pone el shell.

La lógica pura (transformación rotación/espejo/veto, sugerencia por geometría,
persistencia, generación de comandos) se importa de tof_layout (ya host-testeada);
acá sólo se dibuja, se clickea y se delega.
"""
from __future__ import annotations

import tkinter as tk
from tkinter import filedialog, ttk
from typing import Dict, Optional

from .panel import Panel
from .protocol_top import TopFrame
from .shell_theme import BG, CARD, FG, MUTED, PANEL, UI_B
from .tof_layout import (FLIPS, POSITIONS, ROTATIONS, TofLayout,
                         default_config_path, load_or_default)
from .zones import NO_READING_COLOR, zone_color

CELL = 40              # px por zona (grande para clickear)
GRID_W = 4
VETO_FILL = "#23262b"  # zona vetada (apagada)
VETO_X = "#e0533a"     # X roja sobre la zona vetada

# Campos del panel pared+cancha (clave de WallField → etiqueta).
WALL_FIELDS = [
    ("wall_height_mm", "Alto pared (mm)"),
    ("mount_height_mm", "Alto ToF (mm)"),
    ("tilt_down_deg", "Inclinación ↓ (°)"),
    ("vfov_deg", "FOV vert (°)"),
    ("field_width_mm", "Cancha ancho (mm)"),
    ("field_height_mm", "Cancha largo (mm)"),
]


class TofSetupPanel(Panel):
    title = "Config ToF"
    key = "tofcfg"
    icon = "⚙"
    sends_commands = True

    def build(self, parent: tk.Frame) -> None:
        # Estado en la instancia (config + última lectura + widgets).
        self.cfg_path: str = self.ctx.config_path or default_config_path()
        self.cfg: TofLayout = load_or_default(self.cfg_path)
        self.last: Optional[TopFrame] = None
        self._cards: Dict[int, dict] = {}
        self._wall_vars: Dict[str, tk.StringVar] = {}

        self.header = tk.Label(parent, text="esperando datos…", anchor="w",
                               font=("Consolas", 11, "bold"), bg=BG, fg=FG, padx=8, pady=6)
        self.header.pack(fill="x")

        # Fila de 4 cards de ToF (orden visual = tira 360).
        cards = ttk.Frame(parent, padding=6)
        cards.pack(fill="x")
        for col, idx in enumerate(range(4)):
            self._cards[idx] = self._make_card(cards, col, idx)

        # Panel de pared + cancha.
        wall = ttk.LabelFrame(
            parent, padding=6,
            text="PARED + CANCHA — sugerir qué filas vetar (ven por encima de la pared)")
        wall.pack(fill="x", padx=6, pady=(0, 4))
        self._build_wall_panel(wall)

        # Panel de config (guardar/cargar/bajar).
        cfgrow = ttk.LabelFrame(parent, text="CONFIG", padding=6)
        cfgrow.pack(fill="x", padx=6, pady=(0, 4))
        ttk.Button(cfgrow, text="💾 Guardar .json", command=self._save).pack(side="left", padx=3)
        ttk.Button(cfgrow, text="📂 Cargar .json", command=self._load).pack(side="left", padx=3)
        ttk.Button(cfgrow, text="⬇ Bajar a la placa (firmware)",
                   command=self._push_to_fw).pack(side="left", padx=12)
        self.cfg_note = ttk.Label(cfgrow, style="Muted.TLabel", text=f"archivo: {self.cfg_path}")
        self.cfg_note.pack(side="left", padx=8)

    # ── Cards ────────────────────────────────────────────────────────────────
    def _make_card(self, parent: ttk.Frame, col: int, idx: int) -> dict:
        wrap = ttk.Frame(parent, relief="solid", borderwidth=1, padding=4)
        wrap.grid(row=0, column=col, padx=5, sticky="n")
        title = ttk.Label(wrap, font=("Segoe UI", 9, "bold"))
        title.pack()

        # Selector de posición.
        posrow = ttk.Frame(wrap); posrow.pack()
        ttk.Label(posrow, text="pos:").pack(side="left")
        posvar = tk.StringVar(value=self.cfg.position.get(idx, "FRONT"))
        ttk.OptionMenu(posrow, posvar, posvar.get(), *POSITIONS,
                       command=lambda v, i=idx: self._set_pos(i, v)).pack(side="left")

        # Canvas de zonas (clickeable).
        w = GRID_W * CELL
        canvas = tk.Canvas(wrap, width=w, height=w, bg=PANEL, highlightthickness=0)
        canvas.pack(pady=3)
        cells, texts, xs = [], [], []
        for r in range(GRID_W):
            for c in range(GRID_W):
                x0, y0 = c * CELL, r * CELL
                cells.append(canvas.create_rectangle(x0, y0, x0 + CELL - 1, y0 + CELL - 1,
                                                     fill=NO_READING_COLOR, outline="#1a1d20"))
                texts.append(canvas.create_text(x0 + CELL / 2, y0 + CELL / 2, text="",
                                                fill=PANEL, font=("Consolas", 8)))
                xs.append((
                    canvas.create_line(x0 + 4, y0 + 4, x0 + CELL - 5, y0 + CELL - 5,
                                       fill=VETO_X, width=2, state="hidden"),
                    canvas.create_line(x0 + CELL - 5, y0 + 4, x0 + 4, y0 + CELL - 5,
                                       fill=VETO_X, width=2, state="hidden"),
                ))
        canvas.bind("<Button-1>", lambda e, i=idx: self._on_zone_click(i, e))

        # Botones rotar / espejo / on-off.
        btns = ttk.Frame(wrap); btns.pack()
        rotbtn = ttk.Button(btns, width=9, command=lambda i=idx: self._cycle_rot(i))
        rotbtn.pack(side="left", padx=1)
        flipbtn = ttk.Button(btns, width=9, command=lambda i=idx: self._cycle_flip(i))
        flipbtn.pack(side="left", padx=1)
        onbtn = ttk.Button(btns, width=7, command=lambda i=idx: self._toggle_sensor(i))
        onbtn.pack(side="left", padx=1)

        card = {"wrap": wrap, "title": title, "posvar": posvar, "canvas": canvas,
                "cells": cells, "texts": texts, "xs": xs,
                "rotbtn": rotbtn, "flipbtn": flipbtn, "onbtn": onbtn}
        self._refresh_card_buttons(idx, card)
        return card

    def _build_wall_panel(self, parent: ttk.Frame) -> None:
        row = ttk.Frame(parent); row.pack(fill="x")
        for key, label in WALL_FIELDS:
            cell = ttk.Frame(row); cell.pack(side="left", padx=4)
            ttk.Label(cell, text=label, font=("Segoe UI", 8)).pack()
            var = tk.StringVar(value=str(getattr(self.cfg.wall, key)))
            ttk.Entry(cell, textvariable=var, width=8).pack()
            self._wall_vars[key] = var
        btns = ttk.Frame(parent); btns.pack(fill="x", pady=(4, 0))
        ttk.Button(btns, text="🔎 Sugerir y vetar filas que ven por encima de la pared",
                   command=self._suggest_and_apply).pack(side="left", padx=3)
        ttk.Button(btns, text="↺ Reset zonas (todas ON)",
                   command=self._reset_zones).pack(side="left", padx=3)
        self.wall_note = ttk.Label(parent, foreground="#9bd", font=("Consolas", 8), text="")
        self.wall_note.pack(anchor="w", pady=(2, 0))

    # ── Acciones de config ───────────────────────────────────────────────────
    def _set_pos(self, idx: int, pos: str) -> None:
        self.cfg.position[idx] = pos
        self._refresh_card_buttons(idx, self._cards[idx])
        self.ctx.log("info", f"ToF {idx} → posición {pos} (se baja con 'Bajar a la placa')")

    def _cycle_rot(self, idx: int) -> None:
        cur = self.cfg.rotation_deg.get(idx, 0)
        nxt = ROTATIONS[(ROTATIONS.index(cur) + 1) % len(ROTATIONS)] if cur in ROTATIONS else 0
        self.cfg.rotation_deg[idx] = nxt
        self._refresh_card_buttons(idx, self._cards[idx])
        self._render_card(idx)
        self.ctx.log("info", f"ToF {idx} → rotación {nxt}°")

    def _cycle_flip(self, idx: int) -> None:
        cur = self.cfg.flip.get(idx, "none")
        nxt = FLIPS[(FLIPS.index(cur) + 1) % len(FLIPS)] if cur in FLIPS else "none"
        self.cfg.flip[idx] = nxt
        self._refresh_card_buttons(idx, self._cards[idx])
        self._render_card(idx)
        self.ctx.log("info", f"ToF {idx} → espejo {nxt}")

    def _toggle_sensor(self, idx: int) -> None:
        self.cfg.sensor_enabled[idx] = not self.cfg.sensor_enabled.get(idx, True)
        self._refresh_card_buttons(idx, self._cards[idx])
        self._render_card(idx)

    def _on_zone_click(self, idx: int, event) -> None:
        c = int(event.x // CELL)
        r = int(event.y // CELL)
        if 0 <= r < GRID_W and 0 <= c < GRID_W:
            z = r * GRID_W + c
            on = self.cfg.toggle_zone(idx, z)
            self._render_card(idx)
            self.ctx.log("info",
                         f"ToF {idx} zona {z} (fila {r}, col {c}) → {'ACTIVA' if on else 'VETADA'}")

    def _refresh_card_buttons(self, idx: int, card: dict) -> None:
        rot = self.cfg.rotation_deg.get(idx, 0)
        fl = self.cfg.flip.get(idx, "none")
        on = self.cfg.sensor_enabled.get(idx, True)
        card["rotbtn"].configure(text=f"⟳ {rot}°")
        card["flipbtn"].configure(text=f"⇋ {fl}")
        card["onbtn"].configure(text="ON" if on else "OFF")
        card["posvar"].set(self.cfg.position.get(idx, "FRONT"))
        card["title"].configure(
            text=f"ToF {idx} · {self.cfg.position.get(idx, '?')}" + ("" if on else "  (OFF)"))

    def _suggest_and_apply(self) -> None:
        ok = True
        for key, var in self._wall_vars.items():
            try:
                val = float(var.get())
                if key in ("field_width_mm", "field_height_mm"):
                    val = int(val)
                setattr(self.cfg.wall, key, val)
            except ValueError:
                ok = False
        if not ok:
            self.wall_note.configure(text="⚠ revisá los números (algún campo no es válido)")
            self.ctx.log("warn", "Config ToF: algún parámetro de pared/cancha no es un número")
            return
        rows = self.cfg.suggest_vetoed_rows()
        self.cfg.apply_row_veto(rows)
        heights = [f"f{r}={self.cfg.row_beam_height_mm(r):.0f}mm" for r in range(GRID_W)]
        self.wall_note.configure(
            text=f"filas vetadas (ven por encima de {self.cfg.wall.wall_height_mm:.0f}mm): "
                 f"{rows or 'ninguna'}   ·   altura del rayo en la pared: {' '.join(heights)}")
        for idx in range(4):
            self._render_card(idx)
        self.ctx.log("info", f"Config ToF: sugerencia aplicada — filas {rows} vetadas en los 4 ToF")

    def _reset_zones(self) -> None:
        self.cfg.reset_zone_masks()
        for idx in range(4):
            self._render_card(idx)
        self.wall_note.configure(text="")
        self.ctx.log("info", "Config ToF: zonas reseteadas (todas ON)")

    def _save(self) -> None:
        path = filedialog.asksaveasfilename(
            title="Guardar config de ToF", defaultextension=".json",
            initialfile=self.cfg_path, filetypes=[("JSON", "*.json"), ("Todos", "*.*")])
        if not path:
            return
        try:
            self.cfg.save(path)
            self.cfg_path = path
            self.cfg_note.configure(text=f"archivo: {self.cfg_path}")
            self.ctx.log("ok", f"Config ToF guardada en {path}")
        except Exception as e:  # noqa: BLE001
            self.ctx.log("bad", f"Config ToF: no pude guardar: {e}")

    def _load(self) -> None:
        path = filedialog.askopenfilename(
            title="Cargar config de ToF",
            filetypes=[("JSON", "*.json"), ("Todos", "*.*")])
        if not path:
            return
        try:
            self.cfg = load_or_default(path)
            self.cfg_path = path
            self.cfg_note.configure(text=f"archivo: {self.cfg_path}")
            for idx in range(4):
                self._refresh_card_buttons(idx, self._cards[idx])
                self._render_card(idx)
            for key, var in self._wall_vars.items():
                var.set(str(getattr(self.cfg.wall, key)))
            self.ctx.log("ok", f"Config ToF cargada de {path}")
        except Exception as e:  # noqa: BLE001
            self.ctx.log("bad", f"Config ToF: no pude cargar: {e}")

    def _push_to_fw(self) -> None:
        cmds = self.cfg.to_firmware_commands()
        live = [c for c in cmds if c.supported_now]
        pending = [c for c in cmds if not c.supported_now]
        for c in live:
            self.ctx.send(c.text)
        self.ctx.log("ok", f"Config ToF: {len(live)} comandos enviados (POS/ON-OFF/SAVE)")
        if pending:
            self.ctx.log("warn",
                         f"Config ToF: {len(pending)} comandos PENDIENTES de firmware "
                         f"(ROT/FLIP/ZONE) — aplicados en el display; bajarán cuando el "
                         f"firmware los soporte")

    # ── Render ───────────────────────────────────────────────────────────────
    def render(self, f: TopFrame) -> None:
        self.last = f
        self.header.configure(
            text=(f"seq={f.seq}  v{f.v}     "
                  f"zonas ToF: {'SÍ' if f.tof.zones else 'pendiente firmware (no viajan aún)'}"))
        for idx in range(4):
            self._render_card(idx)

    def _render_card(self, idx: int) -> None:
        card = self._cards.get(idx)
        if card is None:
            return
        zones = self.last.tof.zones if (self.last and self.last.tof.zones) else None
        raw = zones[idx] if (zones and idx < len(zones)) else [None] * 16
        disp = self.cfg.oriented_cells(raw, idx)
        sensor_on = self.cfg.sensor_enabled.get(idx, True)
        for k in range(GRID_W * GRID_W):
            val = disp[k] if k < len(disp) else None
            enabled = self.cfg.zone_is_enabled(idx, k)
            x1, x2 = card["xs"][k]
            if not enabled or not sensor_on:
                card["canvas"].itemconfig(card["cells"][k], fill=VETO_FILL)
                card["canvas"].itemconfig(card["texts"][k], text="")
                st = "normal" if not enabled else "hidden"   # X solo para zona vetada
                card["canvas"].itemconfig(x1, state=st)
                card["canvas"].itemconfig(x2, state=st)
            else:
                color = NO_READING_COLOR if val is None else zone_color(val)
                card["canvas"].itemconfig(card["cells"][k], fill=color)
                card["canvas"].itemconfig(card["texts"][k], text="" if val is None else str(val))
                card["canvas"].itemconfig(x1, state="hidden")
                card["canvas"].itemconfig(x2, state="hidden")
