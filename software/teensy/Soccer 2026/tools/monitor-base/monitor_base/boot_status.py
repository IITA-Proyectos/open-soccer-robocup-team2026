"""boot_status.py — Clasificación del estado de CALIBRACIÓN al boot de la placa.

Al arrancar, el firmware imprime por USB qué calibración cargó (de EEPROM ✓ o un
fail-safe sin persistir). Esas líneas NO son telemetría: viajan como TEXTO de la
placa (ver sources.SerialSource → _push_text → last_text()). Este módulo las
clasifica en un estado legible para mostrar de forma persistente en la barra de
estado y MATAR el miedo "no sé qué calibración está operativa".

Reglas basadas en los prints reales del firmware DOWN (src/down/main_down.cpp,
src/down/comm_central.cpp, src/diag/diag_down_calibracion.cpp), por ejemplo:
    [DOWN] calib cargada de EEPROM (persistida)
    [DOWN] line_ring calibrado con blanco de EEPROM
    [DOWN] EEPROM sin calib valida — usando carpet recien medido
    [DOWN] calibrando carpet... no mover el robot
    [DOWN] calib persistida en EEPROM
    [CAL]  Calib cargada de EEPROM. Umbrales:

Módulo PURO (sin tkinter, sin hilos): testeable sin display.
"""
from __future__ import annotations

from typing import Optional

# Estados legibles (constantes para reutilizar en GUIs/tests sin repetir el texto).
CALIB_EEPROM = "Calibración: cargada de EEPROM ✓"
CALIB_FAILSAFE = "Calibración: FAIL-SAFE (sin persistir) ⚠"
CALIB_CALIBRANDO = "Calibrando carpet… (no mover el robot)"
LINE_EEPROM = "Línea: blanco de EEPROM ✓"


def classify_boot_line(line: str) -> Optional[str]:
    """Clasifica UNA línea de texto de la placa en un estado de calibración
    legible, o devuelve None si la línea no habla de calibración.

    Coincidencia por subcadena, case-insensitive. El orden importa: lo más
    específico/explícito primero (EEPROM ✓ antes que el genérico fail-safe).
    """
    if line is None:
        return None
    low = line.lower()

    # Línea calibrada con el blanco persistido (lo más específico primero).
    if "line_ring calibrado con blanco de eeprom" in low:
        return LINE_EEPROM

    # Calibración cargada/persistida desde EEPROM → operativa ✓.
    if "calib cargada de eeprom" in low or "persistida" in low:
        return CALIB_EEPROM

    # En curso: midiendo el carpet ahora mismo.
    if "calibrando carpet" in low:
        return CALIB_CALIBRANDO

    # Sin calibración persistida → fail-safe (defaults / carpet recién medido).
    if ("eeprom sin calib" in low
            or "usando carpet recien medido" in low
            or "fail-safe" in low
            or "defaults" in low):
        return CALIB_FAILSAFE

    return None


class BootStatusTracker:
    """Acumula líneas de texto de la placa y recuerda el ÚLTIMO estado de
    calibración conocido, para mostrarlo de forma persistente aunque después
    lleguen líneas que no hablan de calibración.

    Uso típico en la GUI::

        tracker = BootStatusTracker()
        ...
        bt = source.last_text()
        if bt:
            tracker.update(bt)
        st = tracker.last_calib_status()  # None hasta el primer match
    """

    def __init__(self) -> None:
        self._last: Optional[str] = None

    def update(self, line: str) -> Optional[str]:
        """Procesa una línea. Si habla de calibración, actualiza y devuelve el
        estado nuevo; si no, deja el último y devuelve el que ya había."""
        status = classify_boot_line(line)
        if status is not None:
            self._last = status
        return self._last

    def last_calib_status(self) -> Optional[str]:
        """Último estado de calibración conocido, o None si todavía no llegó
        ninguna línea de calibración."""
        return self._last

    def reset(self) -> None:
        """Olvida el estado conocido (p.ej. tras un re-flasheo/reboot)."""
        self._last = None
