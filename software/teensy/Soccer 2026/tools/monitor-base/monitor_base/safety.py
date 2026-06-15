"""safety.py — Clasifica comandos que ESCRIBEN config persistente en la placa.

El shell los gatea con una confirmación que NOMBRA al robot conectado, para no
escribirle a la EEPROM del robot equivocado (riesgo real: configurás R1, enchufás
R2 y guardás la config de R1 en R2). PURO → host-testeable.

Criterio: cualquier comando con token SAVE (CFG SAVE, CAL SAVE, IMU SAVE …) o el
CFG RESET (borra la config del robot). Los re-ceros transitorios (OTOS RESET) NO
se gatean — no tocan config persistente y se usan seguido en el armado del banco.
"""
from __future__ import annotations


def is_destructive_write(cmd: str) -> bool:
    """True si el comando ESCRIBE/borra config persistente del robot (→ confirmar
    a qué robot va). False para comandos transitorios (STREAM/PING/POS/SENSOR/
    OTOS RESET/…)."""
    toks = str(cmd).strip().upper().split()
    if "SAVE" in toks:
        return True
    if "RESET" in toks and "CFG" in toks:   # CFG RESET = borra config; OTOS RESET no
        return True
    return False
