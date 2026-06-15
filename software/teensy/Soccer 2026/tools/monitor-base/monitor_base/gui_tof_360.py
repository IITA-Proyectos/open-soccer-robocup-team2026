"""gui_tof_360.py — VISTA V3: TIRA 360 de los 4 ToF + cámaras arriba.

Una tira horizontal con las 4 grillas de zonas 4×4 lado a lado, en ORDEN FÍSICO
360 alrededor del robot:

    IZQUIERDO │ FRONTAL │ DERECHO │ TRASERO

Cada grilla es un heatmap (rojo=cerca, verde=lejos, gris=sin lectura) con el valor
en mm por zona. Arriba del panel FRONTAL se dibuja qué ve la cámara frontal (camf:
pelota + arcos en texto); arriba del TRASERO, la cámara trasera (camb).

Orientación: las zonas crudas se corrigen con ZoneGrid.from_sensor (que rota 180°
el ToF izquierdo, montado mirando abajo). BONUS: si hay una config del operador
(--tof-setup → tof_layout.json), se respeta — se orientan las zonas con
oriented_cells() y se apagan las zonas vetadas con zone_is_enabled(), así la tira
360 coincide con lo que se configuró.

AVISO HONESTO (en la UI): las 4 grillas están alineadas ENTRE SÍ (consistentes
unas con otras), pero "arriba" en una grilla NO es necesariamente "adelante del
robot" — la rotación ~90° canónica por sensor (azimut-por-zona) todavía no la
expone el firmware. No mentimos sobre eso.

Reusa el core ya testeado: SimTopSource/SerialSource (parser v2), zones.py
(ZoneGrid/zone_color/from_sensor), tof_layout.py (config del operador). La lógica
PURA (orden de paneles + corrección del izquierdo) vive acá abajo y se testea en
tests/test_tof_360.py. La capa Tk SOLO dibuja.

Sin display: usar `--tof-360 --selftest` (no importa este módulo / no abre ventana).
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk
from typing import Dict, List, Optional

from .protocol_top import CamPer, TopFrame
from .sources import FrameSource
from .zones import NO_READING_COLOR, TOF_LABELS, ZoneGrid, zone_color

# ── Lógica PURA (testeable, sin Tk) ──────────────────────────────────────────
# Orden físico 360 de los paneles, de izquierda a derecha en la pantalla. Cada
# entrada es (etiqueta, índice de sensor). El mapa sensor→posición es el espejo
# de zones.TOF_LABELS / tof_zone_orient.h (0=FRONTAL 1=TRASERO 2=DERECHO 3=IZQ).
PANEL_ORDER_360: List[tuple] = [
    ("IZQUIERDO", 3),
    ("FRONTAL", 0),
    ("DERECHO", 2),
    ("TRASERO", 1),
]
ZONE_CELL_PX = 30
ZONE_GRID_W = 4
# Posiciones de columna donde van las cámaras (la frontal arriba del panel FRONTAL,
# la trasera arriba del panel TRASERO).
CAM_COLUMN = {"camf": 1, "camb": 3}   # col 1 = FRONTAL, col 3 = TRASERO


def panel_sensor_for_column(col: int) -> int:
    """Índice de sensor del panel en la columna `col` (0..3) de la tira 360.
    Orden: 0=IZQUIERDO(3) · 1=FRONTAL(0) · 2=DERECHO(2) · 3=TRASERO(1)."""
    return PANEL_ORDER_360[col][1]


def panel_label_for_column(col: int) -> str:
    """Etiqueta del panel en la columna `col` (0..3) de la tira 360."""
    return PANEL_ORDER_360[col][0]


def panel_order_labels() -> List[str]:
    """Las 4 etiquetas en orden físico, de izquierda a derecha."""
    return [label for label, _ in PANEL_ORDER_360]


def oriented_grid_for_sensor(raw_cells, sensor_idx: int,
                             cfg=None, width: int = ZONE_GRID_W) -> ZoneGrid:
    """Grilla orientada del sensor `sensor_idx`.

    - Sin config (cfg=None): usa ZoneGrid.from_sensor, que corrige el ToF
      izquierdo (idx 3) rotándolo 180° y deja los otros 3 en identidad. Es la
      ÚNICA fuente de verdad de orientación que tenemos garantizada.
    - Con config del operador (cfg=TofLayout): respeta su rotación/espejo elegidos
      vía cfg.oriented_cells(), para que la tira 360 coincida con --tof-setup.

    `raw_cells` es la lista de 16 zonas crudas del sensor (None = sin lectura).
    """
    if cfg is not None:
        return ZoneGrid.from_flat(cfg.oriented_cells(list(raw_cells), sensor_idx),
                                  width=width)
    return ZoneGrid.from_sensor(list(raw_cells), sensor_idx, width=width)


def cam_summary(cam: Optional[CamPer]) -> str:
    """Resumen en texto de lo que ve una cámara (pelota + arcos). HONESTO: solo
    campos que existen en CamPer. '—' cuando no ve nada."""
    if cam is None:
        return "sin datos"
    parts: List[str] = []
    if cam.ball_visible:
        parts.append(f"⚽ ({cam.ball_x_mm:+d},{cam.ball_y_mm:+d})mm")
    else:
        parts.append("⚽ —")
    if cam.yellow_visible:
        parts.append(f"🟡 {cam.yellow_angle_deg:+.0f}° {cam.yellow_distance_mm}mm")
    if cam.blue_visible:
        parts.append(f"🔵 {cam.blue_angle_deg:+.0f}° {cam.blue_distance_mm}mm")
    if len(parts) == 1:        # solo la pelota (y no la ve) → ningún arco
        parts.append("arcos —")
    return "   ".join(parts)


# ── Capa Tk (no testeada) ────────────────────────────────────────────────────
class Tof360App:
    """Tira 360 de los 4 ToF + cámaras frontal/trasera arriba."""

    def __init__(self, root: tk.Tk, source: FrameSource, poll_ms: int = 100,
                 recorder=None, cfg=None):
        self.root = root
        self.source = source
        self.poll_ms = poll_ms
        self.recorder = recorder
        self.cfg = cfg                     # TofLayout o None (BONUS, opcional)
        self.last: Optional[TopFrame] = None
        self.frame_count = 0
        self.last_seq = -1
        self.dropped = 0
        self._rate_hz = 0.0
        self._last_t_ms: Optional[int] = None
        self._is_sim = getattr(source, "is_sim", False)
        self._panels: Dict[int, dict] = {}     # col -> widgets del panel
        self._cam_lbls: Dict[str, tk.Label] = {}

        root.title("IITA Soccer — TIRA 360 ToF + cámaras (TOP) v3 — " + source.describe())
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

        self.header = tk.Label(self.root, text="iniciando…", anchor="w", width=98,
                               font=("Consolas", 11, "bold"),
                               bg="#11151a", fg="#cfe", padx=8, pady=6)
        self.header.pack(fill="x")

        body = ttk.Frame(self.root, padding=8)
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
                           font=("Consolas", 9), bg="#0b0e11", fg="#cfe",
                           width=30, padx=6, pady=4)
            lbl.pack(anchor="w", fill="x")
            self._cam_lbls[cam_key] = lbl

        # Fila de paneles ToF (la tira 360), en orden físico de columnas.
        for col in range(4):
            self._panels[col] = self._make_panel(body, col)

        # Aviso honesto de orientación (no mentir sobre el azimut).
        warn = ("ⓘ Las 4 grillas están alineadas ENTRE SÍ; 'arriba' en la grilla "
                "≠ adelante del robot hasta que el firmware exponga el "
                "azimut-por-zona (la rotación ~90° canónica está sin escribir).")
        tk.Label(self.root, text=warn, anchor="w", justify="left",
                 wraplength=560, fg="#e8c14a", bg="#1b1d12",
                 font=("Segoe UI", 9), padx=8, pady=4).pack(fill="x", padx=8, pady=(0, 4))

        self.status = ttk.Label(self.root, text="", anchor="w",
                                relief="sunken", padding=4)
        self.status.pack(fill="x", side="bottom")

    def _make_panel(self, parent: ttk.Frame, col: int) -> dict:
        wrap = ttk.Frame(parent, relief="solid", borderwidth=1, padding=4)
        wrap.grid(row=1, column=col, padx=6, pady=4, sticky="nw")
        label = panel_label_for_column(col)
        sensor_idx = panel_sensor_for_column(col)
        title = ttk.Label(wrap, font=("Segoe UI", 9, "bold"),
                          text=f"{label} · ToF {sensor_idx}")
        title.pack()
        w = ZONE_GRID_W * ZONE_CELL_PX
        h = ZONE_GRID_W * ZONE_CELL_PX
        canvas = tk.Canvas(wrap, width=w, height=h, bg="#0b0e11",
                           highlightthickness=0)
        canvas.pack(pady=3)
        cells, texts = [], []
        for r in range(ZONE_GRID_W):
            for c in range(ZONE_GRID_W):
                x0, y0 = c * ZONE_CELL_PX, r * ZONE_CELL_PX
                cells.append(canvas.create_rectangle(
                    x0, y0, x0 + ZONE_CELL_PX - 1, y0 + ZONE_CELL_PX - 1,
                    fill=NO_READING_COLOR, outline="#1a1d20"))
                texts.append(canvas.create_text(
                    x0 + ZONE_CELL_PX / 2, y0 + ZONE_CELL_PX / 2,
                    text="", fill="#0b0e11", font=("Consolas", 7)))
        caption = ttk.Label(wrap, font=("Consolas", 8), text="(esperando datos…)")
        caption.pack()
        return {"wrap": wrap, "canvas": canvas, "cells": cells, "texts": texts,
                "caption": caption, "sensor_idx": sensor_idx, "label": label}

    # ── Loop ──────────────────────────────────────────────────────────────
    def _tick(self) -> None:
        frames = self.source.poll()
        for f in frames:
            self._consume(f)
        had_error = self._drain_errors()
        if frames:
            self._render(frames[-1])
        elif not had_error and self.last is None:
            self._set_status("esperando datos… (¿flasheaste un env con monitor y "
                             "conectaste la TOP? los ToF tardan ~40 s en bootear)")
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
        self._render_cameras(f)
        self._render_strip(f.tof)
        self.header.configure(
            text=(f"seq={f.seq}  {self._rate_hz:.0f} Hz  frames={self.frame_count}  "
                  f"perdidos={self.dropped}  v{f.v}     "
                  f"zonas ToF: {'SÍ' if f.tof.zones else 'pendiente firmware (no viajan aún)'}"))
        self._set_status(f"src OK · {self.source.describe()}")

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
            grid = oriented_grid_for_sensor(zones[idx], idx, cfg=self.cfg,
                                            width=ZONE_GRID_W)
            cells, texts = panel["cells"], panel["texts"]
            for k, val in enumerate(grid.cells):
                if k >= len(cells):
                    break
                # BONUS: si hay config y la zona está vetada → apagada (gris oscuro).
                vetoed = self.cfg is not None and not self.cfg.zone_is_enabled(idx, k)
                if vetoed:
                    panel["canvas"].itemconfig(cells[k], fill="#23262b")
                    panel["canvas"].itemconfig(texts[k], text="")
                else:
                    color = NO_READING_COLOR if val is None else zone_color(val)
                    panel["canvas"].itemconfig(cells[k], fill=color)
                    panel["canvas"].itemconfig(texts[k], text="" if val is None else str(val))
            vc = grid.valid_count
            panel["caption"].configure(
                text=f"{panel['label']} · ToF {idx}  ({vc}/{len(grid.cells)})")

    # ── Helpers ───────────────────────────────────────────────────────────
    def _set_status(self, text: str) -> None:
        self.status.config(text=text)

    def _on_close(self) -> None:
        try:
            self.source.stop()
            if self.recorder is not None:
                self.recorder.close()
        finally:
            self.root.destroy()


def selftest(frames: int = 200) -> int:
    """Smoke headless de la vista TIRA 360: corre N frames del simulador TOP por
    el parser real y ejercita la lógica PURA (orden de paneles + orientación del
    izquierdo + resumen de cámaras) SIN abrir ventana. Devuelve 0 si OK.

    Sirve de 'corre sin display' y de chequeo de integración del pipeline
    parse → orden 360 → grillas orientadas.
    """
    import sys

    from .protocol_top import parse_line_top
    from .simulator_top import SimulatorTop

    # Consolas Windows (cp1252) no banca °/⚽/ⓘ → salida UTF-8 tolerante (igual que
    # run_selftest_top_salud). La GUI no usa esta ruta.
    if hasattr(sys.stdout, "reconfigure"):
        try:
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        except Exception:  # noqa: BLE001
            pass

    sim = SimulatorTop(rate_hz=20.0)
    last = None
    saw_zones = False
    # El orden de columnas DEBE ser IZQUIERDO,FRONTAL,DERECHO,TRASERO.
    order_ok = panel_order_labels() == ["IZQUIERDO", "FRONTAL", "DERECHO", "TRASERO"]
    # El izquierdo (col 0) debe mapear al sensor 3 y orientarse 180° (no identidad).
    left_180_ok = True
    for _ in range(frames):
        f = parse_line_top(sim.next_line())
        last = f
        if f.tof.zones:
            for col in range(4):
                idx = panel_sensor_for_column(col)
                grid = oriented_grid_for_sensor(f.tof.zones[idx], idx)
                if grid.valid_count > 0:
                    saw_zones = True
            # Chequeo de orientación del izquierdo con un patrón conocido.
            probe = list(range(16))
            left = oriented_grid_for_sensor(probe, panel_sensor_for_column(0))
            if not (left.cells[0] == 15 and left.cells[15] == 0):
                left_180_ok = False
    assert last is not None

    print(f"[selftest-360] frames procesados : {frames}")
    print(f"[selftest-360] orden de paneles  : {panel_order_labels()}")
    print(f"[selftest-360] orden 360 correcto: {'OK' if order_ok else 'FALLO'}")
    print(f"[selftest-360] izquierdo 180°    : {'OK' if left_180_ok else 'FALLO'}")
    print(f"[selftest-360] zonas ToF         : {'presentes' if saw_zones else 'AUSENTES'}")
    print(f"[selftest-360] cám frontal       : {cam_summary(last.camf)}")
    print(f"[selftest-360] cám trasera       : {cam_summary(last.camb)}")
    if not (order_ok and left_180_ok and saw_zones):
        print("[selftest-360] FALLO")
        return 1
    print("[selftest-360] OK")
    return 0


def run_tof_360(source: FrameSource, recorder=None) -> None:
    root = tk.Tk()
    # BONUS: respetar la config del operador (--tof-setup) si existe. Si falla por
    # cualquier motivo, seguimos sin config (from_sensor de fábrica).
    cfg = None
    try:
        from .tof_layout import load_or_default
        cfg = load_or_default()
    except Exception:  # noqa: BLE001 — la config es opcional, nunca bloquea
        cfg = None
    Tof360App(root, source, recorder=recorder, cfg=cfg)
    root.mainloop()
