"""gui_top_health.py — TABLERO DE SALUD de la placa TOP (vista nueva v2).

A diferencia de gui_top.py (radar de posicionamiento), esta vista es un TABLERO
OPERATIVO: un semáforo por sensor (verde OK / amarillo REVISAR / rojo FALLA /
gris SIN DATO) para ver de un vistazo QUÉ sensor anda y cuál miente, la grilla de
ZONAS de cada ToF, y BOTONES para apagar el sensor que molesta y persistir la
config en EEPROM.

Reusa el core ya testeado: SerialSource (handshake STREAM ON/PING), parse_line_top
(parser v2), SimulatorTop. La lógica de veredicto vive en health.py (pura,
host-testeada); las zonas en zones.py. Esta capa SOLO dibuja y manda comandos.

Sin display: usar `--top-salud --selftest` (no importa este módulo).
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk
from typing import Dict, Optional

from .boot_status import BootStatusTracker
from .health import STATUS_COLOR, STATUS_LABEL, Status, counts, evaluate
from .protocol_top import TopFrame
from .sources import FrameSource
from .zones import NO_READING_COLOR, TOF_LABELS, ZoneGrid, zone_color

# Comandos de config (grammar verificada en telemetry_top.cpp:378-438).
# (etiqueta, comando ON, comando OFF) — toggles de habilitación.
TOGGLES = [
    ("Cámara frontal", "CAM F ON", "CAM F OFF"),
    ("Cámara trasera", "CAM B ON", "CAM B OFF"),
    ("BNO izq", "BNO L ON", "BNO L OFF"),
    ("BNO der", "BNO R ON", "BNO R OFF"),
    ("Ultrasonido", "US ON", "US OFF"),
]
TOF_POS = ("FRONT", "RIGHT", "BACK", "LEFT")
NUM_TOF_BTN = 4         # ToF con botón de on/off + posición
ZONE_CELL_PX = 32          # más grande: entra el valor mm por zona
ZONE_GRID_W = 4


class MonitorTopHealthApp:
    def __init__(self, root: tk.Tk, source: FrameSource, poll_ms: int = 100,
                 recorder=None):
        self.root = root
        self.source = source
        self.poll_ms = poll_ms
        self.recorder = recorder
        self.last: Optional[TopFrame] = None
        self.frame_count = 0
        self.last_seq = -1
        self.dropped = 0
        self._rate_hz = 0.0
        self._last_t_ms: Optional[int] = None
        self.boot = BootStatusTracker()
        self._is_sim = getattr(source, "is_sim", False)
        # Estado OPTIMISTA de los toggles (el firmware no manda el estado de
        # config en el JSON todavía → reflejamos lo que el usuario tocó).
        self._enabled: Dict[str, bool] = {}
        self._tiles: Dict[str, dict] = {}        # key -> widgets del tile
        self._zone_cv: Dict[int, dict] = {}      # idx ToF -> {canvas, cells[]}
        self._last_board: Optional[str] = None

        root.title("IITA Soccer — Monitor de SALUD (TOP) v2 — " + source.describe())
        self._build_layout()
        self.source.start()
        self.root.after(self.poll_ms, self._tick)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ── Layout ────────────────────────────────────────────────────────────
    def _build_layout(self) -> None:
        if self._is_sim:
            tk.Label(self.root, text="⚠ SIMULADOR — datos FALSOS, no es el robot",
                     bg="#b32d00", fg="#ffe27a",
                     font=("Segoe UI", 12, "bold"), pady=5).pack(fill="x")

        # width fijo (en chars): el texto crece con seq/frames; sin esto el reqwidth
        # del header sube de a poco y la ventana sigue moviéndose.
        self.header = tk.Label(self.root, text="iniciando…", anchor="w", width=92,
                               font=("Consolas", 11, "bold"),
                               bg="#11151a", fg="#cfe", padx=8, pady=6)
        self.header.pack(fill="x")

        body = ttk.Frame(self.root, padding=8)
        body.pack(fill="both", expand=True)

        # Tablero de tiles (semáforo por sensor).
        ttk.Label(body, text="SALUD POR SENSOR",
                  font=("Segoe UI", 10, "bold")).grid(row=0, column=0, sticky="w")
        self._tiles_frame = ttk.Frame(body)
        self._tiles_frame.grid(row=1, column=0, sticky="nw", padx=(0, 14))

        # Grillas de zonas de los ToF (orientación canónica; rojo=cerca, verde=lejos,
        # gris=sin lectura; el nº es la distancia en mm de esa zona).
        ttk.Label(body, text="ZONAS ToF (4×4 · 🔴 cerca  🟢 lejos  ⬛ sin lectura · mm)",
                  font=("Segoe UI", 10, "bold")).grid(row=0, column=1, sticky="w")
        self._zones_frame = ttk.Frame(body)
        self._zones_frame.grid(row=1, column=1, sticky="nw")
        self._zones_note = ttk.Label(self._zones_frame, foreground="#888",
                                     text="esperando datos…")
        self._zones_note.grid(row=0, column=0, sticky="w")

        # Config (escribe a la placa).
        cfg = ttk.LabelFrame(self.root, text="CONFIG (escribe a la placa)", padding=6)
        cfg.pack(fill="x", padx=8, pady=(0, 6))
        self._build_config(cfg)

        self.status = ttk.Label(self.root, text="", anchor="w",
                                relief="sunken", padding=4)
        self.status.pack(fill="x", side="bottom")

    def _build_config(self, parent: ttk.Frame) -> None:
        row = ttk.Frame(parent)
        row.pack(fill="x")
        # Toggles de habilitación.
        for label, on_cmd, off_cmd in TOGGLES:
            self._enabled[label] = True
            b = ttk.Button(row, width=16)
            b.configure(text=f"{label}: ON",
                        command=lambda l=label, o=on_cmd, f=off_cmd, btn=b:
                        self._toggle(l, o, f, btn))
            b.pack(side="left", padx=2)
            self._tiles.setdefault("_btns", {})  # placeholder, no usado
        # ToF: on/off + posición.
        tofrow = ttk.Frame(parent)
        tofrow.pack(fill="x", pady=(4, 0))
        for n in range(NUM_TOF_BTN):
            cell = ttk.Frame(tofrow)
            cell.pack(side="left", padx=4)
            key = f"ToF {n}"
            self._enabled[key] = True
            b = ttk.Button(cell, width=11)
            b.configure(text=f"ToF{n}: ON",
                        command=lambda k=key, nn=n, btn=b:
                        self._toggle(k, f"TOF {nn} ON", f"TOF {nn} OFF", btn))
            b.pack()
            var = tk.StringVar(value="POS")
            om = ttk.OptionMenu(cell, var, "POS", *TOF_POS,
                                command=lambda pos, nn=n: self._send(f"TOF {nn} POS {pos}"))
            om.pack()
        # Persistencia.
        cfgrow = ttk.Frame(parent)
        cfgrow.pack(fill="x", pady=(4, 0))
        for label, cmd in (("Guardar EEPROM", "CFG SAVE"),
                           ("Recargar", "CFG LOAD"),
                           ("Reset defaults", "CFG RESET")):
            ttk.Button(cfgrow, text=label,
                       command=lambda c=cmd: self._send(c)).pack(side="left", padx=2)

    # ── Comandos ──────────────────────────────────────────────────────────
    def _toggle(self, key: str, on_cmd: str, off_cmd: str, btn: ttk.Button) -> None:
        now_on = not self._enabled.get(key, True)
        self._enabled[key] = now_on
        self._send(on_cmd if now_on else off_cmd)
        short = key.replace("Cámara ", "Cám ").replace("Ultrasonido", "US")
        btn.configure(text=f"{short}: {'ON' if now_on else 'OFF'}")

    def _send(self, cmd: str) -> None:
        if self._is_sim:
            self._set_status(f"SIMULADOR: comando NO enviado ({cmd})")
            return
        self.source.send(cmd)
        self._set_status(f"→ comando enviado: {cmd}")

    # ── Loop ──────────────────────────────────────────────────────────────
    def _tick(self) -> None:
        frames = self.source.poll()
        for f in frames:
            self._consume(f)
        if hasattr(self.source, "last_text"):
            bt = self.source.last_text()
            if bt:
                self._last_board = bt
                self.boot.update(bt)
        had_error = self._drain_errors()
        if frames:
            self._render(frames[-1])
        elif not had_error and self.last is None:
            msg = "esperando datos…"
            if self._last_board:
                msg += f" la placa dice: «{self._last_board}»"
            else:
                msg += (" ¿flasheaste top_robot2_pri y conectaste la batería? "
                        "(la TOP tarda ~40 s en bootear por los ToF)")
            self._set_status(msg)
        self.root.after(self.poll_ms, self._tick)

    def _consume(self, f: TopFrame) -> None:
        self.frame_count += 1
        if self.last_seq >= 0:
            gap = f.seq - self.last_seq - 1
            if gap > 0:
                self.dropped += gap
        self.last_seq = f.seq
        if self._last_t_ms is not None and f.t_ms > self._last_t_ms:
            inst = 1000.0 / (f.t_ms - self._last_t_ms)
            self._rate_hz = inst if self._rate_hz == 0 else 0.8 * self._rate_hz + 0.2 * inst
        self._last_t_ms = f.t_ms
        if self.recorder is not None:
            self.recorder.write(f)
        self.last = f

    def _drain_errors(self) -> bool:
        msgs = []
        while True:
            try:
                msgs.append(self.source.errors.get_nowait())
            except Exception:
                break
        if msgs:
            self._set_status("⚠ " + " | ".join(msgs[-2:]))
            return True
        return False

    def _render(self, f: TopFrame) -> None:
        items = evaluate(f)
        self._render_tiles(items)
        self._render_zones(f.tof)
        c = counts(items)
        self.header.configure(
            text=(f"seq={f.seq}  {self._rate_hz:.0f} Hz  frames={self.frame_count}  "
                  f"perdidos={self.dropped}  v{f.v}     OK={c[Status.OK]}  "
                  f"REVISAR={c[Status.WARN]}  FALLA={c[Status.DEAD]}  "
                  f"SIN DATO={c[Status.NODATA]}"))
        self._set_status(f"src OK · {self.source.describe()}")

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
        frame = tk.Frame(self._tiles_frame, bd=1, relief="solid",
                         padx=6, pady=4, width=200, height=58)
        frame.grid(row=r, column=c, padx=3, pady=3, sticky="nsew")
        # ⚠️ pack_propagate(False), NO grid_propagate: los hijos van con pack, así
        # que grid_propagate NO los clampea y el tile se agrandaba con el texto
        # largo → la ventana se redimensionaba cada frame = PARPADEO (bug 2026-06-14).
        frame.pack_propagate(False)
        title = tk.Label(frame, anchor="w", font=("Segoe UI", 9, "bold"),
                         fg="#fff", justify="left")
        title.pack(anchor="w")
        badge = tk.Label(frame, anchor="w", font=("Consolas", 8, "bold"), fg="#fff")
        badge.pack(anchor="w")
        detail = tk.Label(frame, anchor="w", font=("Consolas", 8), fg="#eee",
                          wraplength=190, justify="left")
        detail.pack(anchor="w")
        return {"frame": frame, "title": title, "badge": badge, "detail": detail}

    def _render_zones(self, tof) -> None:
        if not tof.zones:
            self._zones_note.configure(
                text="pendiente firmware: las zonas no viajan en el stream todavía\n"
                     "(hoy llega 1 distancia por sensor; ver TASK de telemetría de zonas)")
            return
        self._zones_note.grid_remove()
        for i, sensor in enumerate(tof.zones):
            # from_sensor → orientación CANÓNICA (corrige el ToF izquierdo 180°).
            grid = ZoneGrid.from_sensor(sensor, i, width=ZONE_GRID_W)
            cv = self._zone_cv.get(i)
            if cv is None:
                cv = self._make_zone_canvas(i, grid)
                self._zone_cv[i] = cv
            cells, texts = cv["cells"], cv["texts"]
            for k, val in enumerate(grid.cells):
                if k >= len(cells):
                    break
                color = NO_READING_COLOR if val is None else zone_color(val)
                cv["canvas"].itemconfig(cells[k], fill=color)
                cv["canvas"].itemconfig(texts[k], text="" if val is None else str(val))
            vc = grid.valid_count
            cv["caption"].configure(
                text=f"ToF {i} · {TOF_LABELS.get(i, '?')}  ({vc}/{len(grid.cells)})")

    def _make_zone_canvas(self, idx: int, grid: ZoneGrid) -> dict:
        wrap = ttk.Frame(self._zones_frame)
        wrap.grid(row=idx // 2, column=idx % 2, padx=6, pady=6, sticky="nw")
        w = grid.width * ZONE_CELL_PX
        h = grid.height * ZONE_CELL_PX
        canvas = tk.Canvas(wrap, width=w, height=h, bg="#0b0e11",
                           highlightthickness=0)
        canvas.pack()
        cells, texts = [], []
        for r in range(grid.height):
            for c in range(grid.width):
                x0, y0 = c * ZONE_CELL_PX, r * ZONE_CELL_PX
                cells.append(canvas.create_rectangle(
                    x0, y0, x0 + ZONE_CELL_PX - 1, y0 + ZONE_CELL_PX - 1,
                    fill=NO_READING_COLOR, outline="#1a1d20"))
                texts.append(canvas.create_text(
                    x0 + ZONE_CELL_PX / 2, y0 + ZONE_CELL_PX / 2,
                    text="", fill="#0b0e11", font=("Consolas", 7)))
        caption = ttk.Label(wrap, font=("Consolas", 8),
                            text=f"ToF {idx} · {TOF_LABELS.get(idx, '?')}")
        caption.pack()
        return {"canvas": canvas, "cells": cells, "texts": texts, "caption": caption}

    # ── Helpers ───────────────────────────────────────────────────────────
    def _set_status(self, text: str) -> None:
        calib = self.boot.last_calib_status()
        if calib:
            text = f"[{calib}]  {text}"
        self.status.config(text=text)

    def _on_close(self) -> None:
        try:
            self.source.stop()
            if self.recorder is not None:
                self.recorder.close()
        finally:
            self.root.destroy()


def run_top_health(source: FrameSource, recorder=None) -> None:
    root = tk.Tk()
    MonitorTopHealthApp(root, source, recorder=recorder)
    root.mainloop()
