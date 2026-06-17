"""rate_meter.py — Medidores de frecuencia (Hz) PUROS para el monitor.

Dos medidores que el shell del monitor usa para mostrar "qué tan vivo está
cada enlace":

  • FrameRateMeter — Hz de LLEGADA de un mensaje. Se le avisa con `.tick(now_ms)`
    cada vez que arriba un frame. Calcula Hz como ventana deslizante (ema +
    fallback). NO usa time.time(): el caller le pasa el reloj (testeable).

  • ChangeRateMeter — Hz de CAMBIO de un valor escalar/tuple. Se le avisa con
    `.observe(value, now_ms)` y reporta cuántas veces por segundo cambió. Útil
    para "flapping" (heading_valid baila), o para "% de actividad" (ball_vis
    pasa de 0 a 1 N veces por segundo).

Ambos: sin GUI, sin threads, sin RNG. Testeables al 100%. Mismo patrón que
`gui_timeline.SignalHistory` pero enfocados en TASA (Hz), no histórico.
"""
from __future__ import annotations

from collections import deque
from typing import Any, Deque, Optional, Tuple


class FrameRateMeter:
    """Mide Hz de llegada de un evento (típicamente un frame).

    Implementación: ventana deslizante de timestamps. Hz = (N-1) / (t_last - t_first)
    si hay ≥2 muestras dentro de `window_ms`. Si hay solo 1 muestra reciente,
    Hz=0 (no podemos estimar). Si la ÚLTIMA muestra fue hace más de
    `stale_after_ms`, también Hz=0 (no hay flujo activo).

    No usa time.time() — el caller pasa now_ms; así es testeable sin tiempo real
    y se puede alimentar desde el `t_ms` del propio frame si es preferible.

    Parámetros:
        window_ms: ventana sobre la que promedia (1000 = "Hz al último segundo").
        stale_after_ms: si la última muestra es más vieja que esto, Hz=0.
        capacity: tope de muestras en la cola (evita crecimiento sin freno).
    """

    def __init__(self, window_ms: int = 1000, stale_after_ms: int = 1500,
                 capacity: int = 256) -> None:
        if window_ms <= 0 or stale_after_ms <= 0 or capacity < 2:
            raise ValueError("window_ms y stale_after_ms > 0 y capacity ≥ 2")
        self.window_ms = window_ms
        self.stale_after_ms = stale_after_ms
        self._t: Deque[int] = deque(maxlen=capacity)
        self._last_now_ms: Optional[int] = None

    def tick(self, now_ms: int) -> None:
        """Registra un evento ocurrido en `now_ms`. Idempotente sobre orden:
        si llega un timestamp viejo (clock skew), lo ignoramos para no romper
        el cómputo."""
        if self._t and now_ms < self._t[-1]:
            return  # tiempo va para atrás: ignorar (skew defensivo)
        self._t.append(int(now_ms))
        self._last_now_ms = int(now_ms)
        self._prune(now_ms)

    def _prune(self, now_ms: int) -> None:
        """Borra muestras más viejas que window_ms desde now."""
        cutoff = now_ms - self.window_ms
        while self._t and self._t[0] < cutoff:
            self._t.popleft()

    def hz(self, now_ms: int) -> float:
        """Hz observado al instante `now_ms`. 0 si no hay suficientes muestras
        o el flujo está stale (>stale_after_ms sin actualizar)."""
        if self._last_now_ms is None:
            return 0.0
        if now_ms - self._last_now_ms > self.stale_after_ms:
            return 0.0
        self._prune(now_ms)
        n = len(self._t)
        if n < 2:
            return 0.0
        span_ms = self._t[-1] - self._t[0]
        if span_ms <= 0:
            return 0.0
        return (n - 1) * 1000.0 / span_ms

    @property
    def is_stale(self) -> bool:
        """¿No llegó nada hace mucho? Usa el último now_ms registrado como
        referencia (no hay reloj global). Si nunca tickeó, es stale."""
        return self._last_now_ms is None

    @property
    def count(self) -> int:
        """Cuántas muestras hay actualmente en la ventana."""
        return len(self._t)

    def reset(self) -> None:
        self._t.clear()
        self._last_now_ms = None


class ChangeRateMeter:
    """Mide Hz de CAMBIO de un valor (cuántas veces por segundo cambia).

    Es un FrameRateMeter al que solo le tickeás cuando el valor cambia. Útil
    para detectar flapping ("heading_valid baila 6 veces por segundo") o
    actividad ("la pose se mueve 20 veces por segundo").

    `observe(value, now_ms)` compara con el último valor. Si difiere, tickea.
    El primer observe NO cuenta como cambio (solo establece la base).

    Acepta cualquier tipo hashable o comparable con `!=`: int, float, str, bool,
    tuple, etc. Para tipos con tolerancia (floats con ruido) pasá `epsilon`.
    """

    def __init__(self, window_ms: int = 1000, stale_after_ms: int = 1500,
                 epsilon: float = 0.0, capacity: int = 256) -> None:
        self._meter = FrameRateMeter(window_ms, stale_after_ms, capacity)
        self.epsilon = float(epsilon)
        self._last_value: Any = _UNSET

    def observe(self, value: Any, now_ms: int) -> bool:
        """Registra una observación. Retorna True si fue un CAMBIO (y se contó
        en el medidor); False si fue el primer valor o no cambió."""
        changed = False
        if self._last_value is _UNSET:
            pass  # primer valor: solo bandera
        elif self.epsilon > 0.0:
            try:
                if abs(float(value) - float(self._last_value)) > self.epsilon:
                    changed = True
            except (TypeError, ValueError):
                if value != self._last_value:
                    changed = True
        else:
            if value != self._last_value:
                changed = True
        self._last_value = value
        if changed:
            self._meter.tick(now_ms)
        return changed

    def hz(self, now_ms: int) -> float:
        return self._meter.hz(now_ms)

    @property
    def last_value(self) -> Any:
        return None if self._last_value is _UNSET else self._last_value

    @property
    def change_count(self) -> int:
        return self._meter.count

    def reset(self) -> None:
        self._meter.reset()
        self._last_value = _UNSET


class _Unset:
    """Sentinela explícito para distinguir "nunca observado" de "observado None"."""
    __slots__ = ()
    def __repr__(self) -> str:  # pragma: no cover
        return "<UNSET>"

_UNSET = _Unset()


# ── Helpers para multi-placa (TOP/DOWN/CENTRAL) ─────────────────────────────

class BoardRateMeters:
    """Conjunto de medidores POR PLACA: Hz de llegada del frame USB y Hz de
    cambio de campos clave. Una instancia por placa en el shell.

    Uso típico:
        meters = BoardRateMeters()
        meters.on_frame(now_ms=frame.t_ms, ball_x=snap.ball_x, my_x=snap.my_x, ...)
        hz_frame = meters.frame_hz(now_ms)
        hz_change_ball = meters.change_hz("ball_x", now_ms)
    """

    def __init__(self, window_ms: int = 1000, stale_after_ms: int = 1500) -> None:
        self.frame_meter = FrameRateMeter(window_ms, stale_after_ms)
        self._changes: dict[str, ChangeRateMeter] = {}
        self._window_ms = window_ms
        self._stale_after_ms = stale_after_ms

    def watch_field(self, name: str, epsilon: float = 0.0) -> None:
        """Registra un campo a vigilar. Idempotente."""
        if name not in self._changes:
            self._changes[name] = ChangeRateMeter(
                self._window_ms, self._stale_after_ms, epsilon=epsilon)

    def on_frame(self, now_ms: int, **fields: Any) -> None:
        """Llamar cada vez que llega un frame. Tickea el frame meter y observa
        cada campo registrado con `watch_field`."""
        self.frame_meter.tick(now_ms)
        for name, value in fields.items():
            if name in self._changes:
                self._changes[name].observe(value, now_ms)

    def frame_hz(self, now_ms: int) -> float:
        return self.frame_meter.hz(now_ms)

    def change_hz(self, field: str, now_ms: int) -> float:
        m = self._changes.get(field)
        return m.hz(now_ms) if m else 0.0

    @property
    def is_stale(self) -> bool:
        return self.frame_meter.is_stale

    def reset(self) -> None:
        self.frame_meter.reset()
        for m in self._changes.values():
            m.reset()
