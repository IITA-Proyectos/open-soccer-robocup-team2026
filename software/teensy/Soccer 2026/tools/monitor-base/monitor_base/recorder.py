"""recorder.py — Graba la telemetría entrante a un archivo .jsonl.

Escribe el JSON crudo de cada frame (frame.raw_json) tal cual llegó, una línea
por frame, para poder reproducirlo después con --replay o analizarlo offline
(precursor de la "telemetría estilo F1": registrar para mejorar el software).
"""
from __future__ import annotations

from typing import Optional

from .protocol import Frame


class Recorder:
    """Acumulador a archivo de líneas de telemetría. Robusto a cierres dobles."""

    def __init__(self, path: str):
        self.path = path
        self.count = 0
        self._f: Optional[object] = open(path, "w", encoding="utf-8", newline="\n")

    def write(self, frame: Frame) -> None:
        if self._f is None or not frame.raw_json:
            return
        self._f.write(frame.raw_json + "\n")
        self.count += 1

    def close(self) -> None:
        if self._f is None:
            return
        try:
            self._f.flush()
            self._f.close()
        finally:
            self._f = None

    def __enter__(self) -> "Recorder":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
