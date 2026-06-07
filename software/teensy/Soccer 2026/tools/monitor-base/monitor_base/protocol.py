"""protocol.py — Parser del contrato de telemetría DOWN v1 (JSON Lines).

Espejo en Python de src/shared/telemetry_down.h. Contrato documentado en
docs/firmware/TELEMETRIA-DOWN.md. Núcleo PURO (sin GUI, sin serial) → testeable
con pytest contra el golden generado por el firmware C++
(tests/golden_frame_v1.jsonl).
"""
from __future__ import annotations

import json
from dataclasses import dataclass, field
from typing import List, Optional

# Versión de schema que esta app entiende (campo "v"). Espejo de
# TELEMETRY_DOWN_SCHEMA en telemetry_down.h.
SCHEMA_VERSION = 1

# Sentinels N/A — espejo de TD_NA_I16 / TD_NA_U16 (y de LineStatusV2).
NA_I16 = -32768
NA_U16 = 65535

# Flags de evento EV_* (src/shared/types.h). bit -> nombre legible.
EVENT_FLAGS = [
    (0x01, "IMMINENT_EXIT"),
    (0x02, "CORNER"),
    (0x04, "LINE_END"),
    (0x08, "LIFTED"),
    (0x10, "CALIB_SUSPECT"),
    (0x20, "MUX_DEAD"),
    (0x40, "DEGRADED_GEOMETRY"),
    (0x80, "SENSOR_NOISY"),
]


class ProtocolError(ValueError):
    """Línea malformada, JSON inválido, o schema no soportado."""


def decode_flags(flags: int) -> List[str]:
    """Devuelve los nombres de los EV_* prendidos en `flags`."""
    return [name for bit, name in EVENT_FLAGS if flags & bit]


@dataclass
class Ring:
    """Anillo de N sensores de luz: crudo + calibración."""
    n: int
    raw: List[int]
    white: List[bool]        # bit-expandido del bitmask "white"
    carpet: List[int]
    white_cal: List[int]

    @property
    def thresholds(self) -> List[int]:
        """Umbral por sensor = (carpet + white) / 2 (igual que line_ring)."""
        return [(c + w) // 2 for c, w in zip(self.carpet, self.white_cal)]

    @property
    def margins(self) -> List[int]:
        """Separación blanco-carpet por sensor (|white - carpet|).

        Margen chico ⇒ el sensor casi no distingue piso de blanco
        (batería floja, sensor sucio/pegado o mal calibrado).
        """
        return [abs(w - c) for c, w in zip(self.carpet, self.white_cal)]


@dataclass
class LineStatus:
    """LineStatusV2 — la interpretación que DOWN manda a la CENTRAL.

    Los campos N/A se exponen como None (ya decodificados desde los sentinels).
    La compuerta maestra es `valid`: si es False, NADA acá es confiable.
    """
    schema: int
    valid: bool
    present: bool
    angle_deg: Optional[float]       # None si N/A
    escape_deg: Optional[float]      # None si N/A
    penetration_mm: Optional[int]    # None si N/A
    cross_track_mm: Optional[int]    # None si N/A
    sensors_on_line: int
    flags: int
    quality: int
    sample_age_ms: int

    @property
    def flag_names(self) -> List[str]:
        return decode_flags(self.flags)


@dataclass
class Otos:
    """Pose y velocidad fusionada de los OTOS (odometría)."""
    n: int
    left_ok: bool
    right_ok: bool
    x_mm: float
    y_mm: float
    heading_deg: float
    vx_mm_s: float
    vy_mm_s: float
    omega_rad_s: float
    slip_mm_s: float

    @property
    def has_pose(self) -> bool:
        """¿Hay al menos un OTOS sano para confiar en la pose?"""
        return self.n > 0 and (self.left_ok or self.right_ok)


@dataclass
class Frame:
    """Un frame de telemetría completo (una línea JSON)."""
    v: int
    seq: int
    t_ms: int
    ring: Ring
    line: LineStatus
    otos: Otos
    lifted: bool
    line_tick: int
    line_tick_us: int
    raw_json: str = field(default="", repr=False)


def _expand_white(bits: int, n: int) -> List[bool]:
    return [bool(bits & (1 << i)) for i in range(n)]


def _na_i16(v: int) -> Optional[int]:
    return None if v == NA_I16 else v


def _na_u16(v: int) -> Optional[int]:
    return None if v == NA_U16 else v


def parse_obj(obj: dict, raw: str = "") -> Frame:
    """Construye un Frame desde un dict ya decodificado de JSON.

    Lanza ProtocolError si falta una clave esperada o el schema no se soporta.
    """
    try:
        v = int(obj["v"])
        if v != SCHEMA_VERSION:
            raise ProtocolError(
                f"schema de telemetría no soportado: v={v} "
                f"(esta app entiende v={SCHEMA_VERSION})"
            )
        r = obj["ring"]
        n = int(r["n"])
        raw_counts = [int(x) for x in r["raw"]]
        carpet = [int(x) for x in r["carpet"]]
        white_cal = [int(x) for x in r["white_cal"]]
        ring = Ring(
            n=n,
            raw=raw_counts,
            white=_expand_white(int(r["white"]), n),
            carpet=carpet,
            white_cal=white_cal,
        )

        ln = obj["line"]
        angle = _na_i16(int(ln["angle_cd"]))
        escape = _na_i16(int(ln["escape_cd"]))
        xtrack = _na_i16(int(ln["xtrack_mm"]))
        pen = _na_u16(int(ln["pen_mm"]))
        line = LineStatus(
            schema=int(ln["schema"]),
            valid=bool(ln["valid"]),
            present=bool(ln["present"]),
            angle_deg=(None if angle is None else angle / 100.0),
            escape_deg=(None if escape is None else escape / 100.0),
            penetration_mm=pen,
            cross_track_mm=xtrack,
            sensors_on_line=int(ln["non"]),
            flags=int(ln["flags"]),
            quality=int(ln["q"]),
            sample_age_ms=int(ln["age_ms"]),
        )

        o = obj["otos"]
        otos = Otos(
            n=int(o["n"]),
            left_ok=bool(o["lok"]),
            right_ok=bool(o["rok"]),
            x_mm=float(o["x"]),
            y_mm=float(o["y"]),
            heading_deg=float(o["hdg"]),
            vx_mm_s=float(o["vx"]),
            vy_mm_s=float(o["vy"]),
            omega_rad_s=float(o["w"]),
            slip_mm_s=float(o["slip"]),
        )

        d = obj["diag"]
        return Frame(
            v=v,
            seq=int(obj["seq"]),
            t_ms=int(obj["t_ms"]),
            ring=ring,
            line=line,
            otos=otos,
            lifted=bool(d["lifted"]),
            line_tick=int(d["ltick"]),
            line_tick_us=int(d["ltick_us"]),
            raw_json=raw,
        )
    except ProtocolError:
        raise
    except (KeyError, TypeError, ValueError) as e:
        raise ProtocolError(f"frame de telemetría malformado: {e}") from e


def parse_line(line: str) -> Frame:
    """Parsea UNA línea JSON a un Frame. Ignora whitespace/CR/LF al borde.

    Lanza ProtocolError si la línea no es JSON válido o no cumple el contrato.
    """
    s = line.strip()
    if not s:
        raise ProtocolError("línea vacía")
    try:
        obj = json.loads(s)
    except json.JSONDecodeError as e:
        raise ProtocolError(f"JSON inválido: {e}") from e
    if not isinstance(obj, dict):
        raise ProtocolError("se esperaba un objeto JSON por línea")
    return parse_obj(obj, raw=s)


def is_telemetry_line(line: str) -> bool:
    """Heurística barata: ¿parece un frame de telemetría? (para filtrar prints
    de boot del firmware como '[DOWN] ...' mezclados en el mismo Serial)."""
    s = line.lstrip()
    return s.startswith("{") and '"v":' in s
