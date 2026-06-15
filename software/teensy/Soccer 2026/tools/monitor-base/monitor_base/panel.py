"""panel.py — Contrato de los PANELES embebibles del monitor unificado + el
contexto que el shell les pasa + el buffer de logs.

Cada vista (cancha, polar, salud, etc.) es un Panel: construye sus widgets dentro
del frame que le da el shell y se actualiza con render(frame). NO crea ventana,
NO abre la fuente, NO corre su propio loop, NO manda comandos directo: todo eso lo
provee el shell vía el PanelContext. Así N vistas conviven en una sola app, con
una sola conexión y un loop común.
"""
from __future__ import annotations

import time
import tkinter as tk
from collections import deque
from dataclasses import dataclass
from typing import Callable, Deque, List, Optional, Tuple

from .protocol_top import TopFrame
from .shell_theme import BG


@dataclass
class PanelContext:
    """Servicios que el shell le presta a cada panel."""
    send: Callable[[str], None]          # mandar comando a la placa (con guard de sim)
    log: Callable[[str, str], None]      # log(level, msg): "info"/"warn"/"bad"/"cmd"/"ok"
    is_sim: bool
    config_path: Optional[str] = None


class Panel:
    """Base de un panel embebible. Subclasear y definir build() + render()."""
    title: str = "Panel"        # nombre en la nav
    key: str = "panel"          # identificador único
    icon: str = "●"             # glifo en la nav
    sends_commands: bool = False  # True si manda comandos a la placa

    def __init__(self, parent: tk.Widget, ctx: PanelContext):
        self.ctx = ctx
        self.container = tk.Frame(parent, bg=BG)
        self.build(self.container)

    def build(self, parent: tk.Frame) -> None:
        raise NotImplementedError

    def render(self, f: TopFrame) -> None:
        """Actualiza el panel con el último frame. Override en la subclase."""

    def on_show(self) -> None:
        """El panel pasó a ser visible (override opcional)."""

    def on_hide(self) -> None:
        """El panel dejó de ser visible (override opcional)."""


# ── Logs ────────────────────────────────────────────────────────────────────
LogEntry = Tuple[float, str, str]      # (epoch_s, level, msg)

LEVELS = ("info", "ok", "warn", "bad", "cmd")


class LogBuffer:
    """Ring buffer de eventos del monitor (conexión, comandos, anomalías, latidos
    de datos). Lo llena el shell vía ctx.log; lo dibuja el panel de Logs."""
    def __init__(self, maxlen: int = 2000):
        self._d: Deque[LogEntry] = deque(maxlen=maxlen)
        self._seq = 0

    def add(self, level: str, msg: str, *, clock: Optional[Callable[[], float]] = None) -> None:
        t = (clock or time.time)()
        lvl = level if level in LEVELS else "info"
        self._d.append((t, lvl, msg))
        self._seq += 1

    @property
    def seq(self) -> int:
        """Cambia cada vez que se agrega algo (para que el panel sepa si redibujar)."""
        return self._seq

    def entries(self, level_filter: Optional[str] = None, limit: Optional[int] = None
                ) -> List[LogEntry]:
        items = [e for e in self._d if (level_filter is None or e[1] == level_filter)]
        if limit is not None:
            items = items[-limit:]
        return items

    def clear(self) -> None:
        self._d.clear()
        self._seq += 1

    def __len__(self) -> int:
        return len(self._d)
