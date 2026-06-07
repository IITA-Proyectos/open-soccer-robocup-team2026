"""sensor_health.py — Detección de sensores muertos/pegados/saturados.

Núcleo PURO (sin GUI). Acumula lecturas crudas frame a frame y, sobre una
ventana móvil, decide la salud de cada sensor. La idea operativa: al pasar el
robot sobre las líneas, un sensor SANO oscila mucho (carpet≈150 ↔ blanco≈800);
uno MUERTO/PEGADO se queda plano. Eso lo detectamos por el RANGO (max-min) en la
ventana, sin necesitar calibración.
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from enum import Enum
from typing import Deque, List


class Health(str, Enum):
    UNKNOWN = "unknown"   # todavía no hay suficientes muestras
    OK = "ok"             # oscila → vivo
    DEAD = "dead"         # rango ~0 tras muchas muestras → muerto/pegado
    SAT_HIGH = "sat_high"  # clavado en el tope del ADC
    SAT_LOW = "sat_low"   # clavado en el piso del ADC


@dataclass
class SensorStatus:
    index: int
    health: Health
    last: int
    vmin: int
    vmax: int
    span: int          # vmax - vmin en la ventana
    samples: int

    @property
    def is_problem(self) -> bool:
        return self.health in (Health.DEAD, Health.SAT_HIGH, Health.SAT_LOW)


class SensorHealthTracker:
    """Mantiene una ventana móvil de lecturas por sensor y reporta su salud.

    Parámetros:
      n            cantidad de sensores.
      window       cuántas muestras recientes considerar.
      min_samples  muestras mínimas antes de declarar DEAD (evita falsos al inicio).
      dead_span    si span <= dead_span tras min_samples ⇒ DEAD.
      adc_max      fondo de escala del ADC (10-bit ⇒ 1023).
      sat_margin   margen contra los topes para SAT_HIGH/SAT_LOW.
    """

    def __init__(self, n: int = 32, window: int = 256, min_samples: int = 64,
                 dead_span: int = 6, adc_max: int = 1023, sat_margin: int = 8):
        self.n = n
        self.window = window
        self.min_samples = min_samples
        self.dead_span = dead_span
        self.adc_max = adc_max
        self.sat_margin = sat_margin
        self._buf: List[Deque[int]] = [deque(maxlen=window) for _ in range(n)]

    def reset(self) -> None:
        for d in self._buf:
            d.clear()

    def update(self, raw: List[int]) -> None:
        """Empuja un frame de lecturas crudas (largo n)."""
        for i in range(min(self.n, len(raw))):
            self._buf[i].append(int(raw[i]))

    def status_of(self, i: int) -> SensorStatus:
        d = self._buf[i]
        samples = len(d)
        if samples == 0:
            return SensorStatus(i, Health.UNKNOWN, 0, 0, 0, 0, 0)
        vmin = min(d)
        vmax = max(d)
        last = d[-1]
        span = vmax - vmin

        health = Health.UNKNOWN
        if samples >= self.min_samples:
            if span <= self.dead_span:
                # No se mueve. ¿Pegado arriba, abajo, o en el medio?
                if last >= self.adc_max - self.sat_margin:
                    health = Health.SAT_HIGH
                elif last <= self.sat_margin:
                    health = Health.SAT_LOW
                else:
                    health = Health.DEAD
            else:
                health = Health.OK
        return SensorStatus(i, health, last, vmin, vmax, span, samples)

    def status(self) -> List[SensorStatus]:
        return [self.status_of(i) for i in range(self.n)]

    def problems(self) -> List[SensorStatus]:
        return [s for s in self.status() if s.is_problem]
