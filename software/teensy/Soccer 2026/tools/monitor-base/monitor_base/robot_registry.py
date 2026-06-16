"""robot_registry.py — N° de serie USB del Teensy → identidad del robot. PURO.

Resuelve el problema "¿estoy mirando R1 o R2?": cada Teensy tiene un serial USB
único e inmutable. Con un mapa serial→(robot, placa) la app sabe a QUÉ robot está
conectada SIN depender de la telemetría (que NO trae ID de robot) ni de que el
operador acierte. Y la config se persiste por serial → imposible cruzar R1/R2.

El mapa CONOCIDO se siembra acá y se EXTIENDE con un .json del usuario
(`robot_registry.json` junto al paquete): a un serial desconocido la app le muestra
el serial crudo → lo copiás al .json con un nombre. Sin nombrar igual NO se cruza
(cada serial tiene su propio archivo de config); el nombre es solo para el rótulo.

Sin tkinter ni serial → host-testeable.
"""
from __future__ import annotations

import json
import os
from dataclasses import dataclass
from typing import Dict, Optional

# serial USB (string, TAL CUAL lo da pyserial) → {robot:int, board:str, name:str}.
# board ∈ {'top','down','central'} (la CENTRAL = el cerebro, tercera placa). El
# valor NO se valida acá (identify lo pasa tal cual) → registrar una CENTRAL es
# `registrar_placa.py --robot N --board central` cuando se lea su serial USB.
# Las 4 PLACAS DE COMPETENCIA top/down, verificadas con `registrar_placa.py` el
# 2026-06-15 (serial leído del descriptor USB de cada Teensy). Las 2 CENTRAL aún no
# se sembraron acá (faltan sus seriales reales — NO inventarlos): hasta entonces la
# app las muestra por su serial crudo → registralas. Placas nuevas/repuestos igual:
# aunque no estén nombradas, NO se cruzan (cada serial cae a su propio config).
_SEED: Dict[str, dict] = {
    "19810740": {"robot": 1, "board": "top",  "name": "Robot 1 · TOP"},
    "19778780": {"robot": 1, "board": "down", "name": "Robot 1 · BASE"},
    "16454680": {"robot": 2, "board": "top",  "name": "Robot 2 · TOP"},
    "19756930": {"robot": 2, "board": "down", "name": "Robot 2 · BASE"},
}


@dataclass(frozen=True)
class RobotId:
    serial: Optional[str]
    robot: Optional[int]
    board: Optional[str]
    name: str
    known: bool


def _short(serial: str) -> str:
    return serial if len(serial) <= 10 else serial[:6] + "…"


def identify(serial: Optional[str], extra: Optional[Dict[str, dict]] = None) -> RobotId:
    """Devuelve la identidad del robot a partir del serial USB. `extra` = overrides
    del usuario (de load_registry). Serial desconocido → known=False pero con el
    serial visible (para nombrarlo); serial None → 'sin identificar'."""
    if not serial:
        return RobotId(None, None, None, "sin identificar", False)
    table: Dict[str, dict] = dict(_SEED)
    if extra:
        table.update(extra)
    e = table.get(str(serial))
    if e:
        name = e.get("name") or f"Robot {e.get('robot', '?')}"
        return RobotId(str(serial), e.get("robot"), e.get("board"), name, True)
    return RobotId(str(serial), None, None, f"Robot ? · serial {_short(str(serial))}", False)


def registry_path() -> str:
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(here, "robot_registry.json")


def load_registry(path: Optional[str] = None) -> Dict[str, dict]:
    """Lee el .json de overrides del usuario (serial→{robot,board,name}); {} si no
    existe o está corrupto (degradación silenciosa: el seed sigue valiendo)."""
    p = path or registry_path()
    try:
        with open(p, "r", encoding="utf-8") as f:
            data = json.load(f)
        return data if isinstance(data, dict) else {}
    except (FileNotFoundError, ValueError, OSError):
        return {}
