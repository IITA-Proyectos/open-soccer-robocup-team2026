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

# Posición física de cada ToF (espejo de tof_zone_orient.h: 0=frente, 1=atrás,
# 2=derecha, 3=izquierda).
TOF_LABELS = {0: "FRONTAL", 1: "TRASERO", 2: "DERECHO", 3: "IZQUIERDO"}
_TOF_IDX_LEFT = 3


def tof_zone_needs_180(sensor_idx: int) -> bool:
    """Espejo de tof_zone_orient.h: solo el ToF IZQUIERDO (3) va rotado 180°
    (se montó mirando hacia abajo → su grilla está invertida vs los otros 3)."""
    return sensor_idx == _TOF_IDX_LEFT


def raw_zone_for_canonical(sensor_idx: int, canon_zone: int, grid_w: int = 4) -> int:
    """Dada una zona canónica (marco común de ToF 0/1/2), de qué zona CRUDA del
    sensor hay que leerla. Identidad salvo el izquierdo (180° = invertir fila y
    columna; es su propia inversa). Espejo byte-a-byte de tof_zone_orient.h."""
    if grid_w <= 0:
        return canon_zone
    n = grid_w * grid_w
    if canon_zone < 0 or canon_zone >= n:
        return canon_zone
    if not tof_zone_needs_180(sensor_idx):
        return canon_zone
    row, col = divmod(canon_zone, grid_w)
    return (grid_w - 1 - row) * grid_w + (grid_w - 1 - col)


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

    @classmethod
    def from_sensor(cls, raw_cells: List[Optional[int]], sensor_idx: int,
                    width: int = 4) -> "ZoneGrid":
        """Como from_flat pero reordena las zonas CRUDAS a orientación CANÓNICA
        (corrige el ToF izquierdo rotado 180°), para que los 4 ToF se vean
        mutuamente consistentes — igual que el diag de banco."""
        n = len(raw_cells)
        canon = [raw_cells[raw_zone_for_canonical(sensor_idx, c, width)]
                 for c in range(n)]
        return cls.from_flat(canon, width)

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
