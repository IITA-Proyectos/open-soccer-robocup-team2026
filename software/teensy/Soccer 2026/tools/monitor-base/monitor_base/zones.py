"""zones.py — Modelo PURO de la grilla de zonas de un ToF (VL53L7CX).

El VL53L7CX entrega una grilla de zonas (4x4=16 en producción) por sensor; el
firmware HOY las promedia a una distancia. Cuando el firmware mande el campo
"z" (aditivo, ver protocol_top.Tof.zones), esta clase modela la grilla para
dibujarla y para ver QUÉ zona miente (sin lectura) o está cerca/lejos.

Sin tkinter ni serial → testeable host-native.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Iterator, List, Optional

# Gris para una zona sin lectura (celda None = sentinela 65535 del firmware).
NO_READING_COLOR = "#3a3f44"


@dataclass
class ZoneGrid:
    """Grilla de zonas de UN ToF. cells en orden row-major; None = sin lectura."""
    width: int
    height: int
    cells: List[Optional[int]]

    @classmethod
    def from_flat(cls, cells: List[Optional[int]], width: int = 4) -> "ZoneGrid":
        n = len(cells)
        height = (n + width - 1) // width   # ceil: la última fila puede ser corta
        return cls(width=width, height=height, cells=list(cells))

    @property
    def valid_count(self) -> int:
        return sum(1 for c in self.cells if c is not None)

    @property
    def min_mm(self) -> Optional[int]:
        vals = [c for c in self.cells if c is not None]
        return min(vals) if vals else None

    @property
    def max_mm(self) -> Optional[int]:
        vals = [c for c in self.cells if c is not None]
        return max(vals) if vals else None

    def rows(self) -> Iterator[List[Optional[int]]]:
        for r in range(self.height):
            yield self.cells[r * self.width:(r + 1) * self.width]


def zone_color(dist_mm: float, near: int = 100, far: int = 2000) -> str:
    """Color de una zona CON lectura: cerca = cálido (rojo), lejos = frío (verde).
    Clampa a [near, far]. Devuelve '#rrggbb'. (Las zonas sin lectura las pinta el
    caller con NO_READING_COLOR.)"""
    if far <= near:
        far = near + 1
    d = max(near, min(far, dist_mm))
    t = (d - near) / (far - near)          # 0 = cerca, 1 = lejos
    r = int(round(220 + (60 - 220) * t))   # rojo → verde
    g = int(round(60 + (200 - 60) * t))
    b = int(round(40 + (90 - 40) * t))
    return f"#{r:02x}{g:02x}{b:02x}"
