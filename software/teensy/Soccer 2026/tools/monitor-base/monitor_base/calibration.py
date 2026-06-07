"""calibration.py — Asistente de calibración por sensor (lado PC).

Refleja la lógica del firmware (line_ring / line_calib): al pasar el robot sobre
las líneas, se captura por sensor el MÍNIMO (carpet verde) y el MÁXIMO (blanco);
el umbral es el punto medio. Un margen chico (|blanco-carpet|) ⇒ el sensor casi
no separa piso de blanco (batería floja, óptica/altura, o sensor pegado).

Esto NO escribe nada en el robot: es el espejo en vivo de lo que el firmware va a
calibrar cuando se manda `CAL AUTO ON/OFF` + `CAL SAVE`. Sirve para guiar al
usuario ("pasá el robot sobre las líneas") y prever si la calib va a quedar sana.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional

# Margen mínimo por sensor para confiar (espejo de DownModelCfg.calib_min_margin,
# 40 en el firmware tras el banco 2026-06-06).
DEFAULT_MIN_MARGIN = 40


@dataclass
class SensorCalib:
    index: int
    carpet: int        # mínimo capturado
    white: int         # máximo capturado
    samples: int

    @property
    def threshold(self) -> int:
        return (self.carpet + self.white) // 2

    @property
    def margin(self) -> int:
        return self.white - self.carpet

    def is_suspect(self, min_margin: int = DEFAULT_MIN_MARGIN) -> bool:
        return self.samples == 0 or self.margin < min_margin


class CalibrationAssistant:
    """Captura min/max por sensor mientras está activo (auto-calib asistida)."""

    def __init__(self, n: int = 32):
        self.n = n
        self.active = False
        self._min: List[Optional[int]] = [None] * n
        self._max: List[Optional[int]] = [None] * n
        self._samples = 0

    def start(self) -> None:
        self.active = True
        self._min = [None] * self.n
        self._max = [None] * self.n
        self._samples = 0

    def stop(self) -> None:
        self.active = False

    def update(self, raw: List[int]) -> None:
        """Captura min/max de un frame. No-op si no está activo."""
        if not self.active:
            return
        self._samples += 1
        for i in range(min(self.n, len(raw))):
            v = int(raw[i])
            self._min[i] = v if self._min[i] is None else min(self._min[i], v)
            self._max[i] = v if self._max[i] is None else max(self._max[i], v)

    def result(self) -> List[SensorCalib]:
        out: List[SensorCalib] = []
        for i in range(self.n):
            lo = self._min[i] if self._min[i] is not None else 0
            hi = self._max[i] if self._max[i] is not None else 0
            out.append(SensorCalib(i, lo, hi, self._samples))
        return out

    def suspects(self, min_margin: int = DEFAULT_MIN_MARGIN) -> List[SensorCalib]:
        return [c for c in self.result() if c.is_suspect(min_margin)]


def calib_from_ring(carpet: List[int], white_cal: List[int]) -> List[SensorCalib]:
    """Construye SensorCalib[] desde la calibración VIGENTE que reporta el frame
    (ring.carpet / ring.white_cal). Útil para mostrar la calib actual del robot
    sin re-capturar."""
    n = min(len(carpet), len(white_cal))
    return [SensorCalib(i, int(carpet[i]), int(white_cal[i]), 1) for i in range(n)]
