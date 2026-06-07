"""simulator.py — Generador de telemetría DOWN sintética (sin robot).

Produce líneas JSON con el MISMO formato que el firmware (telemetry_down.cpp), de
modo que la app las parsea por el camino real (protocol.parse_line). Simula una
línea blanca que barre el anillo + un robot que se desplaza (OTOS). Determinista
si se fija `seed` y `noise=0` → sirve para tests reproducibles.
"""
from __future__ import annotations

import json
import math
import random
from typing import Dict, Iterator, List, Optional

from . import geometry

CARPET_LEVEL = 150   # nivel ADC típico de carpet verde
WHITE_LEVEL = 820    # nivel ADC típico de línea blanca
LINE_HALF_WIDTH_MM = 14.0  # media-anchura de la banda blanca


class Simulator:
    """Estado de una corrida simulada. Llamá next_line() repetidamente."""

    def __init__(self, n: int = 32, rate_hz: float = 20.0, noise: int = 0,
                 seed: int = 1234, dead_sensors: Optional[List[int]] = None):
        self.n = n
        self.dt_ms = int(round(1000.0 / rate_hz))
        self.noise = noise
        self.rng = random.Random(seed)
        self.dead = set(dead_sensors or [])
        self.seq = 0
        self.t_ms = 0
        self.carpet = [CARPET_LEVEL] * n
        self.white_cal = [WHITE_LEVEL] * n

    def _line_offset_mm(self) -> float:
        """La línea barre de -100 a +100 mm (seno lento) → cruza el robot."""
        phase = self.seq * 0.06
        return 100.0 * math.sin(phase)

    def _line_orientation_deg(self) -> float:
        """La orientación de la línea rota despacio."""
        return (self.seq * 0.9) % 360.0

    def _white_mask(self, off_mm: float, ori_deg: float) -> List[bool]:
        # Normal de la banda = orientación + 90°. Un sensor ve blanco si su
        # distancia perpendicular a la banda es menor que la media-anchura.
        nrm = math.radians(ori_deg + 90.0)
        nx, ny = math.cos(nrm), math.sin(nrm)
        white = []
        for i in range(self.n):
            x, y = geometry.SENSOR_POS[i]
            dist = x * nx + y * ny - off_mm
            white.append(abs(dist) < LINE_HALF_WIDTH_MM)
        return white

    def _raw_from_white(self, white: List[bool]) -> List[int]:
        raw = []
        for i in range(self.n):
            if i in self.dead:
                base = 512          # sensor muerto: pegado en un valor medio fijo
            elif white[i]:
                base = WHITE_LEVEL
            else:
                base = CARPET_LEVEL
            if self.noise and i not in self.dead:
                base += self.rng.randint(-self.noise, self.noise)
            raw.append(max(0, min(1023, base)))
        return raw

    def _line_block(self, white: List[bool], off_mm: float) -> Dict:
        idx = [i for i, w in enumerate(white) if w]
        non = len(idx)
        present = non > 0
        if present:
            sx = sum(geometry.SENSOR_POS[i][0] for i in idx)
            sy = sum(geometry.SENSOR_POS[i][1] for i in idx)
            angle = math.degrees(math.atan2(sx, sy)) if (sx or sy) else 0.0
            angle_cd = int(round(angle * 100))
            escape = angle + 180.0
            while escape > 180.0:
                escape -= 360.0
            escape_cd = int(round(escape * 100))
            pen = min(255, non * 6)
            xtrack = int(round(off_mm))
            quality = min(100, 30 + non * 8)
            flags = 0x01 if non >= 6 else 0  # EV_IMMINENT_EXIT
        else:
            angle_cd = escape_cd = -32768
            xtrack = -32768
            pen = 65535
            quality = 0
            flags = 0
        return {
            "schema": 2, "valid": 1, "present": 1 if present else 0,
            "angle_cd": angle_cd, "escape_cd": escape_cd,
            "pen_mm": pen, "xtrack_mm": xtrack, "non": non,
            "flags": flags, "q": quality, "age_ms": 2,
        }

    def _otos_block(self) -> Dict:
        # Robot avanzando con una deriva lateral suave.
        t = self.t_ms / 1000.0
        x = 50.0 * math.sin(t * 0.3)
        y = 30.0 * t % 600.0
        hdg = (10.0 * math.sin(t * 0.2))
        return {
            "n": 2, "lok": 1, "rok": 1,
            "x": round(x, 2), "y": round(y, 2), "hdg": round(hdg, 2),
            "vx": round(15.0 * math.cos(t * 0.3), 2), "vy": 30.0,
            "w": round(0.035 * math.cos(t * 0.2), 3), "slip": 0.0,
        }

    def next_obj(self) -> Dict:
        off = self._line_offset_mm()
        ori = self._line_orientation_deg()
        white = self._white_mask(off, ori)
        raw = self._raw_from_white(white)
        bits = 0
        for i, w in enumerate(white):
            if w:
                bits |= (1 << i)
        obj = {
            "v": 1, "seq": self.seq, "t_ms": self.t_ms,
            "ring": {
                "n": self.n, "raw": raw, "white": bits,
                "carpet": list(self.carpet), "white_cal": list(self.white_cal),
            },
            "line": self._line_block(white, off),
            "otos": self._otos_block(),
            "diag": {"lifted": 0, "ltick": 1000 + self.seq, "ltick_us": 120},
        }
        self.seq += 1
        self.t_ms += self.dt_ms
        return obj

    def next_line(self) -> str:
        """Devuelve la próxima línea JSON (sin '\\n')."""
        return json.dumps(self.next_obj(), separators=(",", ":"))

    def lines(self, count: int) -> Iterator[str]:
        for _ in range(count):
            yield self.next_line()
