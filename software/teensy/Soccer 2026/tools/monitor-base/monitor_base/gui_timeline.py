"""gui_timeline.py — VISTA V5: TIMELINE / CAJA-NEGRA EN VIVO del snapshot.

Una tira temporal (los últimos ~10-15 s, ring buffer EN LA APP) con las señales
del WorldSnapshot que el TOP manda a CENTRAL, dibujadas como bandas apiladas:

  • valid            — snap.valid (¿el snapshot sirve?)
  • ball_visible     — snap.ball_visible (parpadeo = pelota fantasma / oclusión)
  • heading_valid    — bit HEADING_VALID del snapshot (imu.heading_valid)
  • goal_opp_visible — snap.goal_opp_visible
  • goal_own_visible — snap.goal_own_visible
  • min_obstacle_mm  — snap.min_obstacle_mm (sparkline numérico, no banda)
  • referee_cmd      — snap.referee_cmd (STOP/START/… como banda de texto)

Sirve para CAZAR el flapping de visibilidad y las CAÍDAS de heading_valid en el
tiempo: un contador de "flancos" (transitions) por señal mide cuánto parpadea.

El histórico NO viaja del firmware — se acumula acá (un SignalHistory = deque con
maxlen). El firmware solo manda el frame instantáneo (TopFrame.snap, .imu, .seq,
.t_ms); esta vista los apila.

LÓGICA PURA (host-testeada): SignalHistory + transitions(). La GUI (Tk) solo
dibuja. Sin display: `--timeline --selftest` (no importa este módulo).
"""
from __future__ import annotations

import tkinter as tk
from collections import deque
from dataclasses import dataclass
from tkinter import ttk
from typing import Deque, List, Optional, Sequence

from .protocol_top import REFEREE, TopFrame
from .sources import FrameSource

# ── Definición de señales (nombre interno, etiqueta, ¿es booleana?) ──────────
# El ORDEN es el orden de apilado en pantalla (de arriba hacia abajo).
BOOL_SIGNALS = [
    ("valid", "snap.valid"),
    ("ball_visible", "ball_visible"),
    ("heading_valid", "heading_valid"),
    ("goal_opp_visible", "goal_opp_vis"),
    ("goal_own_visible", "goal_own_vis"),
]
# Señales numéricas (se dibujan como sparkline, no como banda 0/1).
NUM_SIGNALS = [
    ("min_obstacle_mm", "min_obst (mm)"),
]
# referee_cmd se trata aparte (banda de texto categórica).

ALL_SERIES_NAMES = (
    [n for n, _ in BOOL_SIGNALS]
    + [n for n, _ in NUM_SIGNALS]
    + ["referee_cmd"]
)


@dataclass
class TimelineSample:
    """Un punto del histórico: lo que sacamos de UN TopFrame para el timeline.
    Sólo campos que EXISTEN en el snapshot/IMU (honestidad de datos).
    min_obstacle_mm puede ser None (sin lectura)."""
    seq: int
    t_ms: int
    valid: bool
    ball_visible: bool
    heading_valid: bool
    goal_opp_visible: bool
    goal_own_visible: bool
    min_obstacle_mm: Optional[int]
    referee_cmd: int

    @classmethod
    def from_frame(cls, f: TopFrame) -> "TimelineSample":
        s = f.snap
        # heading_valid: el bit HEADING_VALID del snapshot. Lo expone imu.heading_valid
        # (mismo bit, ya decodificado por el parser); "HEADING_VALID" in s.flag_names
        # es la fuente cruda equivalente. Usamos imu.heading_valid por claridad.
        return cls(
            seq=int(f.seq),
            t_ms=int(f.t_ms),
            valid=bool(s.valid),
            ball_visible=bool(s.ball_visible),
            heading_valid=bool(f.imu.heading_valid),
            goal_opp_visible=bool(s.goal_opp_visible),
            goal_own_visible=bool(s.goal_own_visible),
            min_obstacle_mm=s.min_obstacle_mm,
            referee_cmd=int(s.referee_cmd),
        )


def transitions(series: Sequence) -> int:
    """Cuenta los FLANCOS (cambios de valor entre muestras consecutivas) de una
    serie. Sirve para medir flapping: una serie 0/1/0/1 tiene 3 transiciones.
    Trata None como un valor más (None→algo cuenta como flanco). Una serie de
    0 o 1 elementos tiene 0 transiciones."""
    n = 0
    prev = None
    first = True
    for v in series:
        if first:
            first = False
        elif v != prev:
            n += 1
        prev = v
    return n


class SignalHistory:
    """Ring buffer (deque acotado) de TimelineSample. push() agrega un frame;
    series() extrae la serie por nombre para dibujar/medir flapping.

    maxlen acota el largo: a 20 Hz, 300 muestras ≈ 15 s. El histórico vive ACÁ,
    no en el firmware."""

    def __init__(self, maxlen: int = 300):
        self.maxlen = maxlen
        self._buf: Deque[TimelineSample] = deque(maxlen=maxlen)

    def push(self, f: TopFrame) -> TimelineSample:
        sample = TimelineSample.from_frame(f)
        self._buf.append(sample)
        return sample

    def __len__(self) -> int:
        return len(self._buf)

    @property
    def samples(self) -> List[TimelineSample]:
        return list(self._buf)

    @property
    def latest(self) -> Optional[TimelineSample]:
        return self._buf[-1] if self._buf else None

    def series(self, name: str) -> List:
        """Serie temporal (lista) de la señal `name`, en orden cronológico.
        Para señales booleanas devuelve bool; min_obstacle_mm devuelve int|None;
        referee_cmd devuelve int."""
        return [getattr(s, name) for s in self._buf]

    def transitions(self, name: str) -> int:
        """Cantidad de flancos de la señal `name` (cuánto parpadea)."""
        return transitions(self.series(name))

    def span_ms(self) -> int:
        """Ventana temporal cubierta por el buffer (último t_ms − primero)."""
        if len(self._buf) < 2:
            return 0
        return self._buf[-1].t_ms - self._buf[0].t_ms


# ── Colores de banda (mismo lenguaje visual que el resto de la app) ───────────
ON_COLOR = "#2e8b57"        # verde = señal activa / válida
OFF_COLOR = "#5a2230"       # rojo apagado = señal inactiva
GAP_COLOR = "#3a3f44"       # gris = sin muestra (no debería pasar, defensivo)
GRID_BG = "#0b0e11"
ROW_LABEL_FG = "#cfe"
NUM_LINE = "#e0a23a"        # ámbar para el sparkline numérico
REF_COLORS = {              # color por comando de árbitro
    0: "#5a2230",           # STOP rojo
    1: "#2e8b57",           # START verde
    2: "#3a6ea5",           # HALFTIME azul
    3: "#8a6d3b",           # RESET ámbar
}


def _bool_color(v: bool) -> str:
    return ON_COLOR if v else OFF_COLOR


class MonitorTimelineApp:
    """Tira temporal apilada. Reusa el pipeline FrameSource.poll() → _tick() →
    _render(), igual que MonitorTopHealthApp."""

    ROW_H = 22              # alto de cada banda booleana
    NUM_H = 46             # alto del carril numérico (min_obstacle)
    LABEL_W = 130          # ancho de la columna de etiquetas
    PLOT_W = 900           # ancho del área de dibujo
    PAD = 4

    def __init__(self, root: tk.Tk, source: FrameSource, poll_ms: int = 100,
                 recorder=None, maxlen: int = 300):
        self.root = root
        self.source = source
        self.poll_ms = poll_ms
        self.recorder = recorder
        self.hist = SignalHistory(maxlen=maxlen)
        self.frame_count = 0
        self.last_seq = -1
        self.dropped = 0
        self._is_sim = getattr(source, "is_sim", False)
        self._last_board: Optional[str] = None
        self._row_y: dict = {}        # nombre de señal -> y0 de su carril

        root.title("IITA Soccer — Timeline / caja-negra EN VIVO (TOP→CENTRAL) — "
                   + source.describe())
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

        self.header = tk.Label(self.root, text="iniciando…", anchor="w", width=104,
                               font=("Consolas", 11, "bold"),
                               bg="#11151a", fg="#cfe", padx=8, pady=6)
        self.header.pack(fill="x")

        body = ttk.Frame(self.root, padding=8)
        # width fijo anti-parpadeo: el canvas no debe crecer con el texto largo.
        body.pack(fill="both", expand=True)

        # Altura total = N bandas booleanas + 1 carril numérico + 1 banda árbitro.
        n_rows = len(BOOL_SIGNALS) + 1  # +1 árbitro
        self._plot_h = n_rows * self.ROW_H + self.NUM_H + 2 * self.PAD
        total_w = self.LABEL_W + self.PLOT_W + 2 * self.PAD
        wrap = ttk.Frame(body, width=total_w, height=self._plot_h)
        wrap.pack(anchor="nw")
        wrap.pack_propagate(False)   # anti-parpadeo (igual que gui_top_health).

        self.canvas = tk.Canvas(wrap, width=total_w, height=self._plot_h,
                                bg=GRID_BG, highlightthickness=0)
        self.canvas.pack()
        self._build_lanes()

        # Tablero de flapping (transiciones por señal).
        flap = ttk.LabelFrame(body, text="FLAPPING (flancos en la ventana — más alto = más parpadeo)",
                              padding=6)
        flap.pack(fill="x", pady=(8, 0))
        self._flap_labels: dict = {}
        for col, (name, label) in enumerate(BOOL_SIGNALS + [("referee_cmd", "referee")]):
            cell = ttk.Frame(flap)
            cell.grid(row=0, column=col, padx=8, sticky="w")
            ttk.Label(cell, text=label, font=("Segoe UI", 8)).pack()
            lbl = ttk.Label(cell, text="0", font=("Consolas", 14, "bold"))
            lbl.pack()
            self._flap_labels[name] = lbl

        self.status = ttk.Label(self.root, text="", anchor="w",
                                relief="sunken", padding=4)
        self.status.pack(fill="x", side="bottom")

    def _build_lanes(self) -> None:
        """Dibuja las etiquetas y guarda el y0 de cada carril. Las bandas se
        pintan en _render (acá solo el chasis estático)."""
        y = self.PAD
        for name, label in BOOL_SIGNALS:
            self._row_y[name] = y
            self.canvas.create_text(self.PAD, y + self.ROW_H / 2, anchor="w",
                                    text=label, fill=ROW_LABEL_FG,
                                    font=("Consolas", 9))
            self.canvas.create_line(self.LABEL_W, y, self.LABEL_W + self.PLOT_W, y,
                                    fill="#1a1d20")
            y += self.ROW_H
        # Carril numérico (min_obstacle).
        self._num_y = y
        self.canvas.create_text(self.PAD, y + self.NUM_H / 2, anchor="w",
                                text=NUM_SIGNALS[0][1], fill=ROW_LABEL_FG,
                                font=("Consolas", 9))
        self.canvas.create_line(self.LABEL_W, y, self.LABEL_W + self.PLOT_W, y,
                                fill="#1a1d20")
        y += self.NUM_H
        # Carril del árbitro.
        self._ref_y = y
        self.canvas.create_text(self.PAD, y + self.ROW_H / 2, anchor="w",
                                text="referee_cmd", fill=ROW_LABEL_FG,
                                font=("Consolas", 9))
        self.canvas.create_line(self.LABEL_W, y, self.LABEL_W + self.PLOT_W, y,
                                fill="#1a1d20")
        # Tag para borrar sólo los dibujos dinámicos en cada frame.
        self._dyn_tag = "dyn"

    # ── Loop ──────────────────────────────────────────────────────────────
    def _tick(self) -> None:
        frames = self.source.poll()
        for f in frames:
            self._consume(f)
        if hasattr(self.source, "last_text"):
            bt = self.source.last_text()
            if bt:
                self._last_board = bt
        had_error = self._drain_errors()
        if frames:
            self._render()
        elif not had_error and len(self.hist) == 0:
            msg = "esperando datos…"
            if self._last_board:
                msg += f" la placa dice: «{self._last_board}»"
            else:
                msg += (" ¿flasheaste un env con monitor TOP y conectaste la batería? "
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
        if self.recorder is not None:
            self.recorder.write(f)
        self.hist.push(f)

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

    # ── Render ─────────────────────────────────────────────────────────────
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
        self._set_status(f"src OK · {self.source.describe()}")

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
        # Línea base (eje).
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
                                text=f"{hi}", fill="#888",
                                font=("Consolas", 7), tags=self._dyn_tag)
        self.canvas.create_text(x_start - 2, y + h, anchor="e",
                                text=f"{lo}", fill="#888",
                                font=("Consolas", 7), tags=self._dyn_tag)

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


def run_timeline(source: FrameSource, recorder=None) -> None:
    root = tk.Tk()
    MonitorTimelineApp(root, source, recorder=recorder)
    root.mainloop()


def selftest(frames: int = 200) -> int:
    """Smoke headless de la vista TIMELINE: corre N frames del simulador TOP por
    el parser real (parse_line_top, el mismo que usa SimTopSource), los apila en
    un SignalHistory y reporta el conteo de flancos por señal. Devuelve 0 si OK.
    No abre ventana (CI / 'corre sin display').
    """
    import sys

    from .protocol_top import parse_line_top
    from .simulator_top import SimulatorTop

    # Consola Windows (cp1252) no banca °/⚠ → UTF-8 tolerante (igual que las
    # otras selftest). La GUI no usa esta ruta.
    if hasattr(sys.stdout, "reconfigure"):
        try:
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        except Exception:  # noqa: BLE001
            pass

    sim = SimulatorTop(rate_hz=20.0)
    hist = SignalHistory(maxlen=300)
    last = None
    for _ in range(frames):
        last = parse_line_top(sim.next_line())
        hist.push(last)
    assert last is not None

    # El buffer no puede pasar su maxlen.
    if len(hist) > hist.maxlen:
        print(f"[selftest-timeline] FALLO: buffer {len(hist)} > maxlen {hist.maxlen}")
        return 1

    flaps = {name: hist.transitions(name) for name, _ in BOOL_SIGNALS}
    print(f"[selftest-timeline] frames procesados : {frames}")
    print(f"[selftest-timeline] muestras en buffer : {len(hist)} (maxlen {hist.maxlen})")
    print(f"[selftest-timeline] ventana            : {hist.span_ms() / 1000.0:.1f} s")
    s = hist.latest
    print(f"[selftest-timeline] último seq         : {s.seq}")
    print(f"[selftest-timeline] valid={s.valid} ball_vis={s.ball_visible} "
          f"hdg_valid={s.heading_valid} opp={s.goal_opp_visible} own={s.goal_own_visible} "
          f"min_obst={s.min_obstacle_mm} ref={REFEREE.get(s.referee_cmd, s.referee_cmd)}")
    print(f"[selftest-timeline] flancos por señal  : {flaps}")
    # El simulador parpadea ball_visible (s%30) → tiene que haber flancos.
    if hist.transitions("ball_visible") == 0:
        print("[selftest-timeline] FALLO: ball_visible no parpadeó (¿simulador mudo?)")
        return 1
    print("[selftest-timeline] OK")
    return 0
