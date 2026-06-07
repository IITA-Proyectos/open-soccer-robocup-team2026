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
    WAITING = "waiting"   # no varió, pero el robot está QUIETO → no se puede juzgar
    DEAD = "dead"         # rango ~0 con el robot MOVIÉNDOSE → muerto/pegado
    SAT_HIGH = "sat_high"  # clavado en el tope del ADC (robot moviéndose)
    SAT_LOW = "sat_low"   # clavado en el piso del ADC (robot moviéndose)


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

    @property
    def is_waiting(self) -> bool:
        return self.health == Health.WAITING


class SensorHealthTracker:
    """Mantiene una ventana móvil de lecturas por sensor y reporta su salud.

    Parámetros:
      n            cantidad de sensores.
      window       cuántas muestras recientes considerar.
      min_samples  muestras mínimas antes de declarar DEAD (evita falsos al inicio).
      dead_span    si span <= dead_span (y el robot SE MUEVE) ⇒ DEAD.
      adc_max      fondo de escala del ADC (10-bit ⇒ 1023).
      sat_margin   margen contra los topes para SAT_HIGH/SAT_LOW.
      move_span    span por sensor que cuenta como "este sensor vio variación".
      move_min     cuántos sensores con variación = el robot SE ESTÁ MOVIENDO.

    Anti-falso-positivo: un sensor sólo se declara DEAD/SAT si NO varió MIENTRAS
    el robot se mueve (otros sensores SÍ varían, p.ej. la línea pasó por el anillo).
    Con el robot QUIETO no se puede distinguir muerto de "todavía no pasó la línea":
    se reporta WAITING, no DEAD (evita la pantalla llena de rojo en banco).
    """

    def __init__(self, n: int = 32, window: int = 256, min_samples: int = 64,
                 dead_span: int = 6, adc_max: int = 1023, sat_margin: int = 8,
                 move_span: int = 80, move_min: int = 2):
        self.n = n
        self.window = window
        self.min_samples = min_samples
        self.dead_span = dead_span
        self.adc_max = adc_max
        self.sat_margin = sat_margin
        self.move_span = move_span
        self.move_min = move_min
        self._buf: List[Deque[int]] = [deque(maxlen=window) for _ in range(n)]

    def reset(self) -> None:
        for d in self._buf:
            d.clear()

    def update(self, raw: List[int]) -> None:
        """Empuja un frame de lecturas crudas (largo n)."""
        for i in range(min(self.n, len(raw))):
            self._buf[i].append(int(raw[i]))

    def _span_of(self, i: int) -> int:
        d = self._buf[i]
        return (max(d) - min(d)) if d else 0

    def is_moving(self) -> bool:
        """¿El robot se está moviendo? Heurística: al menos `move_min` sensores
        mostraron variación apreciable (span > move_span) en la ventana — la
        línea pasó por el anillo. Con esto distinguimos 'muerto' de 'quieto'."""
        active = sum(1 for i in range(self.n) if self._span_of(i) > self.move_span)
        return active >= self.move_min

    def status_of(self, i: int, moving: bool = None) -> SensorStatus:
        if moving is None:
            moving = self.is_moving()
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
                if not moving:
                    # Robot quieto: no se puede juzgar (no pasó ninguna línea).
                    health = Health.WAITING
                elif last >= self.adc_max - self.sat_margin:
                    health = Health.SAT_HIGH
                elif last <= self.sat_margin:
                    health = Health.SAT_LOW
                else:
                    health = Health.DEAD
            else:
                health = Health.OK
        return SensorStatus(i, health, last, vmin, vmax, span, samples)

    def status(self) -> List[SensorStatus]:
        moving = self.is_moving()
        return [self.status_of(i, moving) for i in range(self.n)]

    def problems(self) -> List[SensorStatus]:
        return [s for s in self.status() if s.is_problem]

    def waiting_count(self) -> int:
        return sum(1 for s in self.status() if s.is_waiting)
