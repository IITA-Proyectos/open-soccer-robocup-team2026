"""central_timeline.py — LÓGICA PURA del TIMELINE / caja-negra de la CENTRAL.

Espejo de gui_timeline.py (la versión TOP), pero con los campos del CentralFrame
(protocol_central.py). Acumula los últimos ~15 s del "cerebro" en un ring buffer
(EN LA APP, el histórico NO viaja del firmware) para cazar:

  • caídas de los enlaces TOP/DOWN (top_fresh / down_fresh flapping),
  • pérdidas de heading_valid / has_pose / ball_vis / line_valid,
  • cambios de estado de la FSM (fsm_state),
  • carga del loop (loop_ema_us) y la pose XY como sparklines numéricos.

Señales BOOLEANAS (bandas apiladas):
  match, has_pose, heading_valid, ball_vis, line_valid, top_fresh, down_fresh
Señales NUMÉRICAS (sparkline): loop_ema_us, my_x, my_y
Señal CATEGÓRICA de texto: fsm_state.

Sin Tk, sin serial → host-testeable (TDD). El panel (panel_central_timeline.py)
sólo dibuja lo que este módulo calcula.
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from typing import Deque, List, Optional, Sequence

from .protocol_central import CentralFrame

# ── Definición de señales (nombre interno, etiqueta) ─────────────────────────
# El ORDEN es el orden de apilado en pantalla (de arriba hacia abajo).
BOOL_SIGNALS = [
    ("match", "fsm.match"),
    ("has_pose", "has_pose"),
    ("heading_valid", "heading_valid"),
    ("ball_vis", "ball_vis"),
    ("line_valid", "line_valid"),
    ("top_fresh", "top.fresh"),
    ("down_fresh", "down.fresh"),
]
# Señales numéricas (se dibujan como sparkline, no como banda 0/1).
NUM_SIGNALS = [
    ("loop_ema_us", "loop_ema (µs)"),
    ("my_x", "my_x (mm)"),
    ("my_y", "my_y (mm)"),
]
# fsm_state se trata aparte (banda de texto categórica).

ALL_SERIES_NAMES = (
    [n for n, _ in BOOL_SIGNALS]
    + [n for n, _ in NUM_SIGNALS]
    + ["fsm_state"]
)


@dataclass
class CentralTimelineSample:
    """Un punto del histórico: lo que sacamos de UN CentralFrame para el timeline.

    Sólo campos que EXISTEN en el frame de la CENTRAL (honestidad de datos)."""
    seq: int
    t_ms: int
    match: bool
    heading_valid: bool
    has_pose: bool
    ball_vis: bool
    line_valid: bool
    top_fresh: bool
    down_fresh: bool
    fsm_state: str
    my_x: int
    my_y: int
    my_heading: float
    loop_ema_us: int
    cmd_vx: int
    cmd_vy: int
    cmd_w: int

    @classmethod
    def from_frame(cls, f: CentralFrame) -> "CentralTimelineSample":
        snap = f.snap
        return cls(
            seq=int(f.seq),
            t_ms=int(f.t_ms),
            match=bool(f.fsm.match),
            heading_valid=bool(snap.heading_valid),
            has_pose=bool(snap.has_pose),
            ball_vis=bool(snap.ball_vis),
            line_valid=bool(f.down.valid),
            top_fresh=bool(f.top.fresh),
            down_fresh=bool(f.down.line_fresh),
            fsm_state=str(f.fsm.state),
            my_x=int(snap.my_x),
            my_y=int(snap.my_y),
            my_heading=float(snap.my_heading_deg),
            loop_ema_us=int(f.loop.ema_us),
            cmd_vx=int(f.cmd.vx),
            cmd_vy=int(f.cmd.vy),
            cmd_w=int(f.cmd.w),
        )


def transitions(series: Sequence) -> int:
    """Cuenta los FLANCOS (cambios de valor entre muestras consecutivas) de una
    serie. Sirve para medir flapping: una serie 0/1/0/1 tiene 3 transiciones.
    Trata None como un valor más (None→algo cuenta como flanco). Una serie de
    0 o 1 elementos tiene 0 transiciones. Funciona igual para strings (fsm_state)."""
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


class CentralSignalHistory:
    """Ring buffer (deque acotado) de CentralTimelineSample. push() agrega un
    frame; series() extrae la serie por nombre para dibujar/medir flapping.

    maxlen acota el largo: a 20 Hz, 300 muestras ≈ 15 s. El histórico vive ACÁ,
    no en el firmware."""

    def __init__(self, maxlen: int = 300):
        self.maxlen = maxlen
        self._buf: Deque[CentralTimelineSample] = deque(maxlen=maxlen)

    def push(self, f: CentralFrame) -> CentralTimelineSample:
        sample = CentralTimelineSample.from_frame(f)
        self._buf.append(sample)
        return sample

    def __len__(self) -> int:
        return len(self._buf)

    @property
    def samples(self) -> List[CentralTimelineSample]:
        return list(self._buf)

    @property
    def latest(self) -> Optional[CentralTimelineSample]:
        return self._buf[-1] if self._buf else None

    def series(self, name: str) -> List:
        """Serie temporal (lista) de la señal `name`, en orden cronológico.
        Booleanas → bool; numéricas → int/float; fsm_state → str."""
        return [getattr(s, name) for s in self._buf]

    def transitions(self, name: str) -> int:
        """Cantidad de flancos de la señal `name` (cuánto parpadea / cambia)."""
        return transitions(self.series(name))

    def span_ms(self) -> int:
        """Ventana temporal cubierta por el buffer (último t_ms − primero)."""
        if len(self._buf) < 2:
            return 0
        return self._buf[-1].t_ms - self._buf[0].t_ms
