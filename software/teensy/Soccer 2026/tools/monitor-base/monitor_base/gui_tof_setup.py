"""gui_tof_setup.py — Vista de CONFIGURACIÓN de los ToF de la placa TOP.

El banco de trabajo para "acomodar los ToF en su lugar" y "prender/apagar zonas
según la altura de la pared y el tamaño de la cancha" (pedido María 2026-06-15),
todo DESDE la app, sin tocar firmware.

Por cada ToF: su grilla 4×4 EN VIVO (orientada según lo que elijas), un selector
de POSICIÓN (FRONT/RIGHT/BACK/LEFT), botones de ROTAR y ESPEJO para corregir el
montaje, y on/off del sensor. CLICK en una zona = vetar/activar esa zona (las
filas superiores suelen ver por encima de la pared). Un panel de PARED+CANCHA
sugiere qué filas vetar según la geometría. Se guarda/carga a un .json y se puede
BAJAR a la placa (los comandos que el firmware ya entiende se mandan; los que
están en desarrollo —ROT/FLIP/ZONE— quedan listos y se avisan como pendientes).

La lógica vive en tof_layout.py (pura, host-testeada). Esta capa solo dibuja,
clickea y manda comandos. Sin display: `--tof-setup --selftest`.
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk
from typing import Dict, Optional

from .protocol_top import TopFrame
from .sources import FrameSource
from .tof_layout import (FLIPS, POSITIONS, ROTATIONS, TofLayout,
                         default_config_path, load_or_default)
from .zones import NO_READING_COLOR, zone_color

CELL = 40              # px por zona (grande para clickear)
GRID_W = 4
VETO_FILL = "#23262b"  # zona vetada (apagada)
VETO_X = "#e0533a"     # X roja sobre la zona vetada


class TofSetupApp:
    def __init__(self, root: tk.Tk, source: FrameSource, poll_ms: int = 120,
                 recorder=None, config_path: Optional[str] = None):
        self.root = root
        self.source = source
        self.poll_ms = poll_ms
        self.recorder = recorder
        self.cfg_path = config_path or default_config_path()
        self.cfg: TofLayout = load_or_default(self.cfg_path)
        self.last: Optional[TopFrame] = None
        self.frame_count = 0
        self._is_sim = getattr(source, "is_sim", False)
        self._cards: Dict[int, dict] = {}     # idx -> widgets de la card
        self._wall_vars: Dict[str, tk.StringVar] = {}
        self._view_raw = False                # False=vista ORIENTADA (editar) / True=CRUDA (verificar dato)

        root.title("IITA Soccer — Config de ToF (TOP) — " + source.describe())
        self._build_layout()
        self.source.start()
        self.root.after(self.poll_ms, self._tick)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ── Layout ──────────────────────────────────────────────────────────────
    def _build_layout(self) -> None:
        if self._is_sim:
            tk.Label(self.root, text="⚠ SIMULADOR — zonas FALSAS (probando la app, no el robot)",
                     bg="#b32d00", fg="#ffe27a", font=("Segoe UI", 11, "bold"), pady=4).pack(fill="x")

        self.header = tk.Label(self.root, text="iniciando…", anchor="w", width=104,
                               font=("Consolas", 11, "bold"),
                               bg="#11151a", fg="#cfe", padx=8, pady=6)
        self.header.pack(fill="x")

        # Fila de 4 cards de ToF.
        cards = ttk.Frame(self.root, padding=6)
        cards.pack(fill="x")
        # Orden visual = tira 360: IZQUIERDO | FRONTAL | DERECHO | TRASERO.
        # (idx por posición; si el operador reasigna posición, se reordena solo.)
        for col, idx in enumerate(range(4)):
            self._cards[idx] = self._make_card(cards, col, idx)

        # Panel de pared + cancha.
        wall = ttk.LabelFrame(self.root, text="PARED + CANCHA — sugerir qué filas vetar (ven por encima de la pared)",
                              padding=6)
        wall.pack(fill="x", padx=6, pady=(0, 4))
        self._build_wall_panel(wall)

        # Panel de config (guardar/cargar/bajar).
        cfgrow = ttk.LabelFrame(self.root, text="CONFIG", padding=6)
        cfgrow.pack(fill="x", padx=6, pady=(0, 4))
        ttk.Button(cfgrow, text="💾 Guardar .json", command=self._save).pack(side="left", padx=3)
        ttk.Button(cfgrow, text="📂 Cargar .json", command=self._load).pack(side="left", padx=3)
        ttk.Button(cfgrow, text="⬇ Bajar a la placa (firmware)", command=self._push_to_fw).pack(side="left", padx=12)
        ttk.Button(cfgrow, text="📤 Exportar header estático firmware", command=self._export_static_header).pack(side="left", padx=3)
        self._view_btn = ttk.Button(cfgrow, text="👁 Vista: ORIENTADA", command=self._toggle_view)
        self._view_btn.pack(side="left", padx=12)
        ttk.Label(cfgrow, text=f"archivo: {self.cfg_path}", foreground="#888").pack(side="left", padx=8)

        self.status = ttk.Label(self.root, text="", anchor="w", relief="sunken", padding=4)
        self.status.pack(fill="x", side="bottom")

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
        canvas = tk.Canvas(wrap, width=w, height=w, bg="#0b0e11", highlightthickness=0)
        canvas.pack(pady=3)
        cells, texts, xs = [], [], []
        for r in range(GRID_W):
            for c in range(GRID_W):
                x0, y0 = c * CELL, r * CELL
                cells.append(canvas.create_rectangle(x0, y0, x0 + CELL - 1, y0 + CELL - 1,
                                                     fill=NO_READING_COLOR, outline="#1a1d20"))
                texts.append(canvas.create_text(x0 + CELL / 2, y0 + CELL / 2, text="",
                                                fill="#0b0e11", font=("Consolas", 8)))
                # X de "vetada" (oculta por defecto: dos líneas).
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
        fields = [
            ("wall_height_mm", "Alto pared (mm)"),
            ("mount_height_mm", "Alto ToF (mm)"),
            ("tilt_down_deg", "Inclinación ↓ (°)"),
            ("vfov_deg", "FOV vert (°)"),
            ("field_width_mm", "Cancha ancho (mm)"),
            ("field_height_mm", "Cancha largo (mm)"),
        ]
        row = ttk.Frame(parent); row.pack(fill="x")
        for key, label in fields:
            cell = ttk.Frame(row); cell.pack(side="left", padx=4)
            ttk.Label(cell, text=label, font=("Segoe UI", 8)).pack()
            var = tk.StringVar(value=str(getattr(self.cfg.wall, key)))
            ttk.Entry(cell, textvariable=var, width=8).pack()
            self._wall_vars[key] = var
        btns = ttk.Frame(parent); btns.pack(fill="x", pady=(4, 0))
        ttk.Button(btns, text="🔎 Sugerir y vetar filas que ven por encima de la pared",
                   command=self._suggest_and_apply).pack(side="left", padx=3)
        ttk.Button(btns, text="⬛ Default: SÓLO 2ª fila desde abajo",
                   command=self._apply_default_veto).pack(side="left", padx=3)
        ttk.Button(btns, text="↺ Reset zonas (todas ON)",
                   command=self._reset_zones).pack(side="left", padx=3)
        self.wall_note = ttk.Label(parent, foreground="#9bd", font=("Consolas", 8), text="")
        self.wall_note.pack(anchor="w", pady=(2, 0))

    # ── Acciones de config ──────────────────────────────────────────────────
    def _set_pos(self, idx: int, pos: str) -> None:
        self.cfg.position[idx] = pos
        self._set_status(f"ToF {idx} → posición {pos} (se baja con 'Bajar a la placa')")

    def _cycle_rot(self, idx: int) -> None:
        cur = self.cfg.rotation_deg.get(idx, 0)
        nxt = ROTATIONS[(ROTATIONS.index(cur) + 1) % len(ROTATIONS)] if cur in ROTATIONS else 0
        self.cfg.rotation_deg[idx] = nxt
        self._refresh_card_buttons(idx, self._cards[idx])
        self._render_card(idx)
        self._set_status(f"ToF {idx} → rotación {nxt}°")

    def _cycle_flip(self, idx: int) -> None:
        cur = self.cfg.flip.get(idx, "none")
        nxt = FLIPS[(FLIPS.index(cur) + 1) % len(FLIPS)] if cur in FLIPS else "none"
        self.cfg.flip[idx] = nxt
        self._refresh_card_buttons(idx, self._cards[idx])
        self._render_card(idx)
        self._set_status(f"ToF {idx} → espejo {nxt}")

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
            self._set_status(f"ToF {idx} zona {z} (fila {r}, col {c}) → {'ACTIVA' if on else 'VETADA'}")

    def _refresh_card_buttons(self, idx: int, card: dict) -> None:
        rot = self.cfg.rotation_deg.get(idx, 0)
        fl = self.cfg.flip.get(idx, "none")
        on = self.cfg.sensor_enabled.get(idx, True)
        card["rotbtn"].configure(text=f"⟳ {rot}°")
        card["flipbtn"].configure(text=f"⇋ {fl}")
        card["onbtn"].configure(text="ON" if on else "OFF")
        card["title"].configure(
            text=f"ToF {idx} · {self.cfg.position.get(idx, '?')}" + ("" if on else "  (OFF)"))

    def _suggest_and_apply(self) -> None:
        # Leer los parámetros del panel a la config.
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
            return
        rows = self.cfg.suggest_vetoed_rows()
        self.cfg.apply_row_veto(rows)
        heights = [f"f{r}={self.cfg.row_beam_height_mm(r):.0f}mm" for r in range(GRID_W)]
        self.wall_note.configure(
            text=f"filas vetadas (ven por encima de {self.cfg.wall.wall_height_mm:.0f}mm): "
                 f"{rows or 'ninguna'}   ·   altura del rayo en la pared: {' '.join(heights)}")
        for idx in range(4):
            self._render_card(idx)
        self._set_status(f"sugerencia aplicada: filas {rows} vetadas en los 4 ToF")

    def _reset_zones(self) -> None:
        self.cfg.reset_zone_masks()
        for idx in range(4):
            self._render_card(idx)
        self.wall_note.configure(text="")
        self._set_status("zonas reseteadas (todas ON)")

    def _toggle_view(self) -> None:
        """Alterna ORIENTADA ↔ CRUDA. ORIENTADA = rotación/flip + veto aplicados (para orientar
        y editar). CRUDA = los 16 números TAL CUAL salen del sensor, sin orientar ni vetar — para
        CONFIRMAR que estás leyendo el dato puro de los 4 ToF (acercá un objeto a distancia conocida
        y mirá qué zona cruda cambia)."""
        self._view_raw = not self._view_raw
        self._view_btn.configure(text="👁 Vista: CRUDA (sin orientar)" if self._view_raw
                                 else "👁 Vista: ORIENTADA")
        for idx in range(4):
            self._render_card(idx)
        self._set_status("Vista CRUDA: números TAL CUAL del sensor (sin rotar ni vetar)."
                         if self._view_raw else
                         "Vista ORIENTADA: rotación/flip + veto aplicados (para editar).")

    def _apply_default_veto(self) -> None:
        """Config de ARRANQUE: válida SÓLO la 2ª fila desde abajo (anula las 2 filas de
        arriba —ven por encima de la pared— y la de abajo —choca el piso—)."""
        self.cfg.apply_default_veto()
        for idx in range(4):
            self._render_card(idx)
        self.wall_note.configure(text="")
        self._set_status("veto DEFAULT: válida SÓLO la 2ª fila desde abajo. "
                         "Guardá (💾) y bajá a la placa (⬇) para que la TOP lo use.")

    def _export_static_header(self) -> None:
        """Junta los layouts guardados POR SERIAL y genera el header ESTÁTICO del firmware
        (bearing/rotación/flip por serial). El veto NO va acá (es dinámico)."""
        import os
        from .tof_layout import collect_saved_layouts, export_static_layout_header
        try:
            entries = collect_saved_layouts()
            if not entries:
                self._set_status("⚠ No hay layouts guardados por serial. Guardá primero (💾) "
                                 "con la placa conectada (el archivo queda keyed por su serial).")
                return
            hdr = export_static_layout_header(entries)
            here = os.path.dirname(os.path.abspath(__file__))
            out = os.path.join(here, "tof_static_layout.h")
            with open(out, "w", encoding="utf-8") as f:
                f.write(hdr)
            self._set_status(f"✔ header estático exportado ({len(entries)} placa/s) → {out} · "
                             f"commitealo al firmware como src/shared/tof_static_layout.h")
        except Exception as e:  # noqa: BLE001
            self._set_status(f"⚠ no pude exportar: {e}")

    def _save(self) -> None:
        try:
            self.cfg.save(self.cfg_path)
            self._set_status(f"✔ guardado en {self.cfg_path}")
        except Exception as e:  # noqa: BLE001
            self._set_status(f"⚠ no pude guardar: {e}")

    def _load(self) -> None:
        try:
            self.cfg = load_or_default(self.cfg_path)
            for idx in range(4):
                self._cards[idx]["posvar"].set(self.cfg.position.get(idx, "FRONT"))
                self._refresh_card_buttons(idx, self._cards[idx])
                self._render_card(idx)
            for key, var in self._wall_vars.items():
                var.set(str(getattr(self.cfg.wall, key)))
            self._set_status(f"✔ cargado de {self.cfg_path}")
        except Exception as e:  # noqa: BLE001
            self._set_status(f"⚠ no pude cargar: {e}")

    def _push_to_fw(self) -> None:
        cmds = self.cfg.to_firmware_commands()
        live = [c for c in cmds if c.supported_now]
        pending = [c for c in cmds if not c.supported_now]
        if self._is_sim:
            self._set_status(f"SIMULADOR: NO se envió nada. {len(live)} comandos listos, "
                             f"{len(pending)} pendientes de firmware.")
            return
        sent = 0
        for c in live:
            self.source.send(c.text)
            sent += 1
        msg = f"→ {sent} comandos enviados (POS/ON-OFF/ZONEMASK/SAVE). La ZONEMASK YA se aplica en el robot."
        if pending:
            msg += (f"  ({len(pending)} ROT/FLIP NO enviados A PROPÓSITO: en modo default la rotación "
                    f"va PLEGADA dentro de la ZONEMASK; no quedan 'pendientes'.)")
        self._set_status(msg)

    # ── Loop / render ───────────────────────────────────────────────────────
    def _tick(self) -> None:
        frames = self.source.poll()
        for f in frames:
            self.frame_count += 1
            if self.recorder is not None:
                self.recorder.write(f)
            self.last = f
        if frames:
            self._render(frames[-1])
        elif self.last is None:
            self._set_status("esperando datos… (¿flasheaste un env con monitor y conectaste la TOP?)")
        self.root.after(self.poll_ms, self._tick)

    def _render(self, f: TopFrame) -> None:
        self.header.configure(
            text=(f"seq={f.seq}  frames={self.frame_count}  v{f.v}   "
                  f"[VISTA {'CRUDA' if self._view_raw else 'ORIENTADA'}]   "
                  f"zonas ToF del sensor: "
                  f"{'SÍ llegan' if f.tof.zones else 'NO llegan (¿flasheaste un env con telemetría?)'}"))
        for idx in range(4):
            self._render_card(idx)

    def _render_card(self, idx: int) -> None:
        card = self._cards.get(idx)
        if card is None:
            return
        zones = self.last.tof.zones if (self.last and self.last.tof.zones) else None
        raw = zones[idx] if (zones and idx < len(zones)) else [None] * 16
        # CRUDA = números tal cual del sensor (sin orientar, sin veto), para verificar el dato puro.
        # ORIENTADA = rotación/flip + veto aplicados (para orientar y editar el veto).
        disp = list(raw) if self._view_raw else self.cfg.oriented_cells(raw, idx)
        sensor_on = self.cfg.sensor_enabled.get(idx, True)
        for k in range(GRID_W * GRID_W):
            val = disp[k] if k < len(disp) else None
            enabled = True if self._view_raw else self.cfg.zone_is_enabled(idx, k)
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

    # ── Helpers ─────────────────────────────────────────────────────────────
    def _set_status(self, text: str) -> None:
        self.status.config(text=text)

    def _on_close(self) -> None:
        try:
            self.source.stop()
            if self.recorder is not None:
                self.recorder.close()
        finally:
            self.root.destroy()


def run_tof_setup(source: FrameSource, recorder=None, config_path: Optional[str] = None) -> None:
    root = tk.Tk()
    TofSetupApp(root, source, recorder=recorder, config_path=config_path)
    root.mainloop()
