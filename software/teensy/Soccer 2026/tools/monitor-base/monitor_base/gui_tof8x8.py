"""gui_tof8x8.py — VISOR de los 4 ToF en resolución 8×8 (solo lectura).

El gemelo 8×8 del visor de zonas que ya tiene la app, pero para el modo de
DIAGNÓSTICO en alta resolución: el piloto del firmware emite una grilla 8×8
(64 zonas) por sensor en un mensaje de TEXTO (NO JSON), y esta vista la dibuja
como 4 heatmaps lado a lado (FRONT/BACK/RIGHT/LEFT) con el mm por celda y, lo
más importante para el banco, el dt_us del getRangingData y los fps por sensor
— para VER el impacto del 8×8 en el loop.

CONTRATO del mensaje (espejo del firmware, lo emite el piloto ToF 8×8):

    ZN8,<idx>,<res>,<dt_us>,<v0..v_{res-1}>

  • texto plano, NO empieza con '{' (no pasa por el parser JSON / auto_parse).
  • idx   : 0..3 — qué ToF (0=FRONT 1=BACK 2=RIGHT 3=LEFT, espejo de TOF_LABELS).
  • res   : 64 (grilla 8×8).
  • dt_us : µs que tardó el getRangingData de ese frame (costo en el loop).
  • v0..  : `res` distancias en mm, orden fila*8+col. 65535 = sin lectura → None.

SOLO LECTURA: abre el serial, manda STREAM ON + PING (igual que probe_top_serial,
para despertar el stream en modo competencia) y NADA MÁS. No manda comandos de
config (ZONEMASK, ROT, SAVE, etc.) — esta vista nunca toca la placa.

Reusa lo AGNÓSTICO de zones.py (ZoneGrid, zone_color, NO_READING_COLOR,
TOF_LABELS). La lógica de parseo + medición de fps vive acá abajo en funciones
PURAS (sin Tk ni serial) y se testea en tests/test_tof8x8.py. La capa Tk solo
dibuja.

Sin display / para CI: `python -m monitor_base --tof8x8 --selftest` procesa unas
líneas ZN8 sintéticas por el parser real y devuelve 0 sin abrir ventana.
"""
from __future__ import annotations

import sys
import time
from dataclasses import dataclass, field
from typing import Dict, List, Optional

from .zones import NO_READING_COLOR, TOF_LABELS, ZoneGrid, zone_color

# ── Constantes del contrato ──────────────────────────────────────────────────
ZN8_PREFIX = "ZN8"
GRID_W = 8
GRID_N = GRID_W * GRID_W          # 64 celdas
TOF_NO_READING = 65535            # sentinela de "sin lectura" (espejo del firmware)
N_SENSORS = 4

# Etiquetas cortas en orden de idx (0..3). El contrato dice 0=FRONT 1=BACK
# 2=RIGHT 3=LEFT; TOF_LABELS trae los nombres largos en castellano.
TOF_SHORT = {0: "FRONT", 1: "BACK", 2: "RIGHT", 3: "LEFT"}


# ── Lógica PURA (testeable, sin Tk ni serial) ────────────────────────────────
@dataclass
class Zn8Frame:
    """Un frame 8×8 de UN ToF, parseado de una línea ZN8.

    cells en orden row-major (fila*8+col); None = sin lectura (65535).
    """
    idx: int
    res: int
    dt_us: int
    cells: List[Optional[int]]

    @property
    def grid(self) -> ZoneGrid:
        """La grilla 8×8 (reusa el modelo agnóstico de zones.py)."""
        return ZoneGrid.from_flat(self.cells, width=GRID_W)

    @property
    def valid_count(self) -> int:
        return sum(1 for c in self.cells if c is not None)


def is_zn8_line(line: str) -> bool:
    """¿Esta línea cruda es un mensaje ZN8? (cheap, antes de parsear)."""
    return line.strip().startswith(ZN8_PREFIX + ",")


def parse_zn8_line(line: str) -> Zn8Frame:
    """Parsea una línea 'ZN8,idx,res,dt_us,v0..v_{res-1}' a un Zn8Frame.

    Lanza ValueError si la línea está malformada (prefijo, campos, conteo o
    rango de idx). 65535 → None (sin lectura). Es la ÚNICA fuente de verdad del
    parseo; la GUI y el selftest la usan.
    """
    s = line.strip()
    parts = s.split(",")
    if len(parts) < 4 or parts[0] != ZN8_PREFIX:
        raise ValueError(f"no es una línea ZN8: {s[:40]!r}")
    try:
        idx = int(parts[1])
        res = int(parts[2])
        dt_us = int(parts[3])
    except ValueError as e:
        raise ValueError(f"cabecera ZN8 inválida: {s[:40]!r} ({e})") from e
    if not (0 <= idx < N_SENSORS):
        raise ValueError(f"idx fuera de rango (0..{N_SENSORS - 1}): {idx}")
    vals = parts[4:]
    if len(vals) != res:
        raise ValueError(
            f"ZN8 idx={idx}: esperaba {res} valores, llegaron {len(vals)}")
    try:
        cells: List[Optional[int]] = [
            None if int(v) == TOF_NO_READING else int(v) for v in vals]
    except ValueError as e:
        raise ValueError(f"ZN8 idx={idx}: valor no numérico ({e})") from e
    return Zn8Frame(idx=idx, res=res, dt_us=dt_us, cells=cells)


class FpsMeter:
    """Mide fps por sensor a partir de los tiempos de llegada de sus frames.

    EWMA sobre el período entre frames (robusto a un frame perdido). Puro: se le
    pasa el tiempo de llegada (inyectable para tests), no llama a time.time().
    """

    def __init__(self, alpha: float = 0.3):
        self.alpha = alpha
        self._last_t: Dict[int, float] = {}
        self._period: Dict[int, float] = {}
        self.count: Dict[int, int] = {}

    def tick(self, idx: int, now: float) -> None:
        self.count[idx] = self.count.get(idx, 0) + 1
        last = self._last_t.get(idx)
        self._last_t[idx] = now
        if last is None or now <= last:
            return
        dt = now - last
        prev = self._period.get(idx)
        self._period[idx] = dt if prev is None else (1 - self.alpha) * prev + self.alpha * dt

    def fps(self, idx: int) -> Optional[float]:
        p = self._period.get(idx)
        if not p or p <= 0:
            return None
        return 1.0 / p


@dataclass
class Zn8State:
    """Estado del visor: último frame por ToF + medidor de fps. Puro y testeable
    sin Tk: se alimenta con líneas crudas y devuelve qué sensor se actualizó."""
    frames: Dict[int, Zn8Frame] = field(default_factory=dict)
    fps: FpsMeter = field(default_factory=FpsMeter)
    parse_errors: int = 0
    total: int = 0

    def feed(self, line: str, now: Optional[float] = None) -> Optional[int]:
        """Procesa una línea cruda. Si es un ZN8 válido, guarda el frame, anota
        el fps y devuelve el idx del sensor actualizado. Si no es ZN8 → None; si
        es ZN8 pero malformado → cuenta el error y devuelve None."""
        if not is_zn8_line(line):
            return None
        try:
            fr = parse_zn8_line(line)
        except ValueError:
            self.parse_errors += 1
            return None
        self.frames[fr.idx] = fr
        self.total += 1
        self.fps.tick(fr.idx, time.monotonic() if now is None else now)
        return fr.idx


# ── Apertura del serial (solo lectura, espejo de probe_top_serial) ───────────
def _open_serial(port: Optional[str], baud: int = 115200):
    """Abre el serial autodetectando el Teensy si port es None/'auto'. Manda
    STREAM ON + PING una vez (despierta el stream). Solo lectura: nunca config."""
    import serial  # type: ignore

    from .sources import autodetect_port
    dev = port if (port and str(port).lower() != "auto") else autodetect_port()
    if not dev:
        raise RuntimeError("no encontré el Teensy (pasá --port COMx)")
    s = serial.Serial(dev, baud, timeout=0.2)
    try:
        s.reset_input_buffer()
    except Exception:  # noqa: BLE001
        pass
    try:
        s.write(b"STREAM ON\n")
        s.flush()
    except Exception:  # noqa: BLE001 — best-effort
        pass
    return s, dev


# ── Capa Tk (no testeada) ────────────────────────────────────────────────────
class Tof8x8App:
    """4 grillas 8×8 (FRONT/BACK/RIGHT/LEFT) coloreadas por distancia, con mm por
    celda + dt_us y fps por sensor. SOLO LECTURA: abre el serial, manda
    STREAM ON + PING y dibuja; no envía comandos de config."""

    CELL = 48                 # px por celda (8×8 = 384 px de grilla) — grande para leer el mm
    PING_INTERVAL_S = 1.0

    def __init__(self, root, port: Optional[str], baud: int = 115200,
                 poll_ms: int = 60):
        import tkinter as tk  # import local: el módulo se importa sin display
        from tkinter import ttk
        self.tk = tk
        self.ttk = ttk
        self.root = root
        self.port = port
        self.baud = baud
        self.poll_ms = poll_ms
        self.state = Zn8State()
        self._serial = None
        self._dev: Optional[str] = None
        self._last_ping = 0.0
        self._cards: Dict[int, dict] = {}

        root.title("IITA Soccer — Visor ToF 8×8 (TOP, solo lectura)")
        self._build_layout()
        self._connect()
        self.root.after(self.poll_ms, self._tick)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ── Layout ──────────────────────────────────────────────────────────────
    def _build_layout(self) -> None:
        tk, ttk = self.tk, self.ttk
        tk.Label(self.root,
                 text="VISOR 8×8 — SOLO LECTURA (no manda comandos de config a la placa)",
                 bg="#11151a", fg="#9bd", font=("Segoe UI", 10, "bold"),
                 pady=4).pack(fill="x")
        self.header = tk.Label(self.root, text="conectando…", anchor="w",
                               font=("Consolas", 11, "bold"),
                               bg="#11151a", fg="#cfe", padx=8, pady=6)
        self.header.pack(fill="x")

        # Área SCROLLABLE: las 4 grillas 8×8 grandes no entran en pantalla → Canvas con barras
        # de desplazamiento (horizontal abajo, vertical a la derecha) que aparecen si no entra.
        outer = ttk.Frame(self.root)
        outer.pack(fill="both", expand=True)
        scv = tk.Canvas(outer, bg="#11151a", highlightthickness=0)
        vsb = ttk.Scrollbar(outer, orient="vertical", command=scv.yview)
        hsb = ttk.Scrollbar(outer, orient="horizontal", command=scv.xview)
        scv.configure(yscrollcommand=vsb.set, xscrollcommand=hsb.set)
        vsb.pack(side="right", fill="y")
        hsb.pack(side="bottom", fill="x")
        scv.pack(side="left", fill="both", expand=True)
        cards = ttk.Frame(scv, padding=6)
        scv.create_window((0, 0), window=cards, anchor="nw")
        # Recalcular el área scrolleable cuando cambia el tamaño del contenido (rueda del mouse opcional).
        cards.bind("<Configure>", lambda e: scv.configure(scrollregion=scv.bbox("all")))
        scv.bind_all("<MouseWheel>", lambda e: scv.yview_scroll(int(-e.delta / 120), "units"))
        scv.bind_all("<Shift-MouseWheel>", lambda e: scv.xview_scroll(int(-e.delta / 120), "units"))
        # Orden visual por idx 0..3 = FRONT | BACK | RIGHT | LEFT (orden del contrato).
        for col, idx in enumerate(range(N_SENSORS)):
            self._cards[idx] = self._make_card(cards, col, idx)

        self.status = ttk.Label(self.root, text="", anchor="w",
                                relief="sunken", padding=4)
        self.status.pack(fill="x", side="bottom")

    def _make_card(self, parent, col: int, idx: int) -> dict:
        tk, ttk = self.tk, self.ttk
        wrap = ttk.Frame(parent, relief="solid", borderwidth=1, padding=4)
        wrap.grid(row=0, column=col, padx=5, sticky="n")
        title = ttk.Label(
            wrap, font=("Segoe UI", 9, "bold"),
            text=f"{TOF_SHORT.get(idx, '?')} · ToF {idx} · {TOF_LABELS.get(idx, '')}")
        title.pack()

        w = GRID_W * self.CELL
        canvas = tk.Canvas(wrap, width=w, height=w, bg="#0b0e11",
                           highlightthickness=0)
        canvas.pack(pady=3)
        cells, texts = [], []
        for r in range(GRID_W):
            for c in range(GRID_W):
                x0, y0 = c * self.CELL, r * self.CELL
                cells.append(canvas.create_rectangle(
                    x0, y0, x0 + self.CELL - 1, y0 + self.CELL - 1,
                    fill=NO_READING_COLOR, outline="#1a1d20"))
                texts.append(canvas.create_text(
                    x0 + self.CELL / 2, y0 + self.CELL / 2, text="",
                    fill="#000000", font=("Consolas", 13, "bold")))
        caption = ttk.Label(wrap, font=("Consolas", 8), text="(esperando ZN8…)")
        caption.pack()
        return {"wrap": wrap, "canvas": canvas, "cells": cells, "texts": texts,
                "caption": caption}

    # ── Serial ──────────────────────────────────────────────────────────────
    def _connect(self) -> None:
        try:
            self._serial, self._dev = _open_serial(self.port, self.baud)
            self._last_ping = time.monotonic()
            self._set_status(f"conectado a {self._dev} @{self.baud} — STREAM ON enviado")
        except Exception as e:  # noqa: BLE001
            self._serial = None
            self._set_status(f"⚠ no pude abrir el serial: {e}")

    # ── Loop ────────────────────────────────────────────────────────────────
    def _tick(self) -> None:
        ser = self._serial
        if ser is not None:
            now = time.monotonic()
            if now - self._last_ping >= self.PING_INTERVAL_S:
                self._last_ping = now
                try:
                    ser.write(b"PING\n")
                    ser.flush()
                except Exception:  # noqa: BLE001
                    pass
            updated = set()
            # Drena lo que haya llegado (varias líneas por tick a 8×8).
            for _ in range(64):
                try:
                    raw = ser.readline()
                except Exception as e:  # noqa: BLE001
                    self._set_status(f"⚠ enlace perdido: {e}")
                    self._serial = None
                    break
                if not raw:
                    break
                line = raw.decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                idx = self.state.feed(line)
                if idx is not None:
                    updated.add(idx)
            for idx in updated:
                self._render_card(idx)
            self._render_header()
        elif not self.state.frames:
            self._set_status("sin serial (¿conectaste la TOP?). "
                             "Reabrí la vista o pasá --port COMx.")
        self.root.after(self.poll_ms, self._tick)

    def _render_header(self) -> None:
        fps = self.state.fps
        per = "  ".join(
            f"{TOF_SHORT.get(i)}={'--' if fps.fps(i) is None else f'{fps.fps(i):.0f}'}fps"
            for i in range(N_SENSORS))
        self.header.configure(
            text=f"frames={self.state.total}  errores={self.state.parse_errors}   {per}")

    def _render_card(self, idx: int) -> None:
        card = self._cards.get(idx)
        fr = self.state.frames.get(idx)
        if card is None or fr is None:
            return
        for k in range(GRID_N):
            val = fr.cells[k] if k < len(fr.cells) else None
            color = NO_READING_COLOR if val is None else zone_color(val)
            card["canvas"].itemconfig(card["cells"][k], fill=color)
            # Mostrar SIEMPRE el mm (incluidas las lejanas/verdes ≥1000) — pedido Virginia.
            # Con CELL=48 + fuente 13 bold entran los 4 dígitos. None = sin lectura → vacío.
            txt = "" if val is None else str(val)
            card["canvas"].itemconfig(card["texts"][k], text=txt)
        f = self.state.fps.fps(idx)
        card["caption"].configure(
            text=f"dt={fr.dt_us}µs  {'--' if f is None else f'{f:.0f}'}fps  "
                 f"({fr.valid_count}/{GRID_N})  min={fr.grid.min_mm}")

    # ── Helpers ─────────────────────────────────────────────────────────────
    def _set_status(self, text: str) -> None:
        self.status.config(text=text)

    def _on_close(self) -> None:
        try:
            if self._serial is not None:
                # SOLO LECTURA: no mandamos nada al cerrar; el firmware se vuelve a
                # modo competencia solo (apaga su stream tras 3 s sin PING).
                self._serial.close()
        except Exception:  # noqa: BLE001
            pass
        finally:
            self.root.destroy()


# ── Selftest headless (sin ventana, para CI) ─────────────────────────────────
def _synthetic_zn8(idx: int, dt_us: int, base_mm: int) -> str:
    """Arma una línea ZN8 sintética de 64 valores (con un 'sin lectura' adentro)."""
    vals = [base_mm + k for k in range(GRID_N)]
    vals[7] = TOF_NO_READING          # una zona sin lectura, a propósito
    body = ",".join(str(v) for v in vals)
    return f"{ZN8_PREFIX},{idx},{GRID_N},{dt_us},{body}"


def selftest(frames: int = 200) -> int:
    """Smoke headless: procesa líneas ZN8 SINTÉTICAS por el parser real (sin Tk ni
    serial) y verifica el pipeline parse → estado → fps. Devuelve 0 si OK.

    Sirve de 'corre sin display' y de chequeo de integración: que el parser banca
    64 celdas, que 65535 → None, que el estado guarda 4 sensores y mide fps.
    """
    if hasattr(sys.stdout, "reconfigure"):
        try:
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        except Exception:  # noqa: BLE001
            pass

    st = Zn8State()
    t = 0.0
    ok = True
    for i in range(frames):
        idx = i % N_SENSORS
        line = _synthetic_zn8(idx, dt_us=5000 + idx * 100, base_mm=100 + idx * 50)
        t += 0.02                       # 50 Hz sintético por línea
        upd = st.feed(line, now=t)
        if upd != idx:
            ok = False

    # Una línea que NO es ZN8 (no debe tocar el estado ni contar como error).
    before = (st.total, st.parse_errors)
    st.feed('{"v":2,"seq":1}')
    if (st.total, st.parse_errors) != before:
        ok = False
    # Una línea ZN8 MALFORMADA (conteo de valores incorrecto) → cuenta error.
    st.feed(f"{ZN8_PREFIX},0,{GRID_N},5000,1,2,3")
    if st.parse_errors != 1:
        ok = False

    # Chequeos de contenido del último frame de cada sensor.
    saw_4 = sorted(st.frames) == [0, 1, 2, 3]
    sample = st.frames.get(0)
    cells_ok = (sample is not None and len(sample.cells) == GRID_N
                and sample.cells[7] is None and sample.valid_count == GRID_N - 1)
    grid_ok = sample is not None and sample.grid.width == 8 and sample.grid.height == 8

    print(f"[selftest-8x8] frames procesados : {st.total}")
    print(f"[selftest-8x8] sensores vistos    : {sorted(st.frames)}")
    print(f"[selftest-8x8] celdas por grilla  : {len(sample.cells) if sample else 0} (esperado {GRID_N})")
    print(f"[selftest-8x8] grilla 8×8          : {'OK' if grid_ok else 'FALLO'}")
    print(f"[selftest-8x8] 65535 → sin lectura : {'OK' if cells_ok else 'FALLO'}")
    print(f"[selftest-8x8] no-ZN8 ignorada     : {'OK' if (st.total, ) else 'FALLO'}")
    print(f"[selftest-8x8] errores de parseo   : {st.parse_errors} (esperado 1)")
    for i in range(N_SENSORS):
        f = st.fps.fps(i)
        print(f"[selftest-8x8]   {TOF_SHORT[i]:5s} fps      : "
              f"{'--' if f is None else f'{f:.0f}'}  (dt={st.frames[i].dt_us}µs)")
    if not (ok and saw_4 and cells_ok and grid_ok):
        print("[selftest-8x8] FALLO")
        return 1
    print("[selftest-8x8] OK")
    return 0


def run_tof8x8(port: Optional[str] = None, baud: int = 115200) -> None:
    """Abre la ventana del visor 8×8 contra el serial (autodetecta si port=None)."""
    import tkinter as tk
    root = tk.Tk()
    Tof8x8App(root, port=port, baud=baud)
    root.mainloop()
