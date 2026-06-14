"""protocol_top.py — Parser del contrato de telemetría TOP v2.

Espejo en Python de src/shared/telemetry_top.h. Contrato:
docs/firmware/TELEMETRIA-TOP.md. Núcleo PURO (sin GUI/serial) → testeable contra
el golden del firmware (tests/golden_top_v2.jsonl).

v2 (2026-06-14, monitor de posicionamiento A1): agrega
  • camf / camb : detecciones POR CÁMARA (front/back) además del fusionado (cam).
  • base        : OTOS (pose + velocidad) que llega de la base (DOWN), con frescura.
  • line        : línea + VECTOR DE ESCAPE de la base (DOWN), con frescura.
v2 es WIRE-BREAKING: un firmware v2 con app v1 (o viceversa) se rechaza limpio.
"""
from __future__ import annotations

import json
from dataclasses import dataclass
from typing import List, Optional

from .protocol import ProtocolError  # reusa la misma excepción

SCHEMA_VERSION_TOP = 2
TOF_NO_READING = 65535
NA_I16 = -32768       # sentinela N/A de los campos int16 de la línea (LSV2_NA_I16)
NA_U16 = 0xFFFF       # sentinela N/A de penetration (LSV2_NA_U16)

# Flags del WorldSnapshot (snap.flags).
SNAP_FLAGS = [
    (0x01, "IN_OWN_PENALTY"),
    (0x02, "PARTNER_ALIVE"),
    (0x04, "PARTNER_SEES_BALL"),
    (0x08, "MATCH_RUNNING"),
    (0x10, "HEADING_VALID"),
]

# Eventos de la línea (line.events), EV_* OR-eados (ver DownModel/types.h).
LINE_EVENTS = [
    (0x01, "IMMINENT_EXIT"),
    (0x02, "CORNER"),
    (0x04, "LINE_END"),
    (0x08, "LIFTED"),
    (0x10, "CALIB_SUSPECT"),
    (0x20, "MUX_DEAD"),
    (0x40, "DEGRADED_GEOMETRY"),
    (0x80, "SENSOR_NOISY"),
]

REFEREE = {0: "STOP", 1: "START", 2: "HALFTIME", 3: "RESET"}


def decode_snap_flags(flags: int) -> List[str]:
    return [name for bit, name in SNAP_FLAGS if flags & bit]


def decode_line_events(flags: int) -> List[str]:
    return [name for bit, name in LINE_EVENTS if flags & bit]


@dataclass
class Cam:
    front_ok: bool
    back_ok: bool
    ball_visible: bool
    ball_x_mm: int
    ball_y_mm: int
    ball_confidence: int
    ball_vx_mm_s: int
    ball_vy_mm_s: int
    yellow_visible: bool
    yellow_angle_deg: float
    yellow_distance_mm: int
    blue_visible: bool
    blue_angle_deg: float
    blue_distance_mm: int
    crc_errors: int
    resyncs: int

    @property
    def any_alive(self) -> bool:
        return self.front_ok or self.back_ok


@dataclass
class Imu:
    heading_deg: float
    left_deg: float
    right_deg: float
    disagreement_deg: float
    left_ok: bool
    right_ok: bool
    heading_valid: bool


@dataclass
class Tof:
    n: int
    distances_mm: List[Optional[int]]   # None = sin lectura (65535)
    hcsr04_mm: Optional[int]
    min_mm: Optional[int]
    # v2 ADITIVO (campo "z", opcional): zonas crudas por sensor (grilla 4x4=16),
    # antes del promedio. None = el firmware no las manda todavía. Cada celda en
    # sentinela 65535 → None (zona sin lectura). NO rompe el contrato: un frame
    # sin "z" (golden viejo) deja zones=None.
    zones: Optional[List[List[Optional[int]]]] = None


@dataclass
class Snap:
    valid: bool
    my_x_mm: int
    my_y_mm: int
    my_heading_deg: float
    my_confidence: int
    ball_x_mm: int
    ball_y_mm: int
    ball_visible: bool
    ball_confidence: int
    ball_vx_mm_s: int
    ball_vy_mm_s: int
    goal_opp_angle_deg: float
    goal_opp_distance_mm: int
    goal_opp_visible: bool
    goal_own_visible: bool
    goal_own_angle_deg: float
    goal_own_distance_mm: int
    min_obstacle_mm: Optional[int]
    referee_cmd: int
    flags: int

    @property
    def referee_name(self) -> str:
        return REFEREE.get(self.referee_cmd, f"?{self.referee_cmd}")

    @property
    def flag_names(self) -> List[str]:
        return decode_snap_flags(self.flags)


@dataclass
class CamPer:
    """Detección de UNA cámara (frontal o trasera), pre-fusión. Pelota en x/y,
    arcos en polar (ángulo/distancia). El delta front↔back delata la pelota
    fantasma del promedio fusionado."""
    ball_visible: bool
    ball_x_mm: int
    ball_y_mm: int
    yellow_visible: bool
    yellow_angle_deg: float
    yellow_distance_mm: int
    blue_visible: bool
    blue_angle_deg: float
    blue_distance_mm: int


@dataclass
class Base:
    """OTOS (pose + velocidad) que la base (DOWN) difunde a la TOP. *_fresh=False
    → dato viejo, GRISEAR (no mostrar como real)."""
    pose_fresh: bool
    pose_x_mm: int
    pose_y_mm: int
    pose_heading_deg: float
    pose_confidence: int
    vel_fresh: bool
    vel_vx_mm_s: int
    vel_vy_mm_s: int
    vel_omega_deg_s: float
    vel_slip: int


@dataclass
class Line:
    """Línea + VECTOR DE ESCAPE de la base (DOWN). data_valid=False = compuerta
    maestra (ignorar la geometría). El vector de escape = dirección escape_deg +
    magnitud penetration_mm. Campos Optional = N/A (sentinela)."""
    fresh: bool
    schema: int
    data_valid: bool
    angle_deg: Optional[float]
    escape_deg: Optional[float]
    penetration_mm: Optional[int]
    cross_track_mm: Optional[int]
    present: bool
    sensors_on: int
    event_flags: int
    quality: int
    sample_age_ms: int

    @property
    def event_names(self) -> List[str]:
        return decode_line_events(self.event_flags)


@dataclass
class TopFrame:
    v: int
    seq: int
    t_ms: int
    cam: Cam
    camf: CamPer
    camb: CamPer
    imu: Imu
    tof: Tof
    snap: Snap
    base: Base
    line: Line
    frames_sent: int
    raw_json: str = ""


def _tof(v: int) -> Optional[int]:
    return None if int(v) == TOF_NO_READING else int(v)


def _na_i16(v: int) -> Optional[int]:
    return None if int(v) == NA_I16 else int(v)


def _na_i16_deg(v: int) -> Optional[float]:
    iv = int(v)
    return None if iv == NA_I16 else iv / 100.0


def _na_u16(v: int) -> Optional[int]:
    return None if int(v) == NA_U16 else int(v)


def _zones(raw) -> Optional[List[List[Optional[int]]]]:
    """Mapea el campo "z" (opcional) a zonas por sensor con 65535 → None.
    None si el firmware no lo manda (compat hacia atrás)."""
    if raw is None:
        return None
    return [[_tof(z) for z in sensor] for sensor in raw]


def _camper(c: dict) -> CamPer:
    return CamPer(
        ball_visible=bool(c["bvis"]), ball_x_mm=int(c["bx"]), ball_y_mm=int(c["by"]),
        yellow_visible=bool(c["gy_vis"]), yellow_angle_deg=int(c["gy_ang"]) / 100.0,
        yellow_distance_mm=int(c["gy_dist"]),
        blue_visible=bool(c["gb_vis"]), blue_angle_deg=int(c["gb_ang"]) / 100.0,
        blue_distance_mm=int(c["gb_dist"]),
    )


def parse_obj_top(obj: dict, raw: str = "") -> TopFrame:
    try:
        v = int(obj["v"])
        if v != SCHEMA_VERSION_TOP:
            raise ProtocolError(
                f"schema TOP no soportado: v={v} (app entiende v={SCHEMA_VERSION_TOP})")
        c = obj["cam"]
        cam = Cam(
            front_ok=bool(c["fok"]), back_ok=bool(c["bok"]),
            ball_visible=bool(c["bvis"]), ball_x_mm=int(c["bx"]), ball_y_mm=int(c["by"]),
            ball_confidence=int(c["bconf"]), ball_vx_mm_s=int(c["bvx"]),
            ball_vy_mm_s=int(c["bvy"]),
            yellow_visible=bool(c["gy_vis"]), yellow_angle_deg=int(c["gy_ang"]) / 100.0,
            yellow_distance_mm=int(c["gy_dist"]),
            blue_visible=bool(c["gb_vis"]), blue_angle_deg=int(c["gb_ang"]) / 100.0,
            blue_distance_mm=int(c["gb_dist"]),
            crc_errors=int(c["crc"]), resyncs=int(c["resync"]),
        )
        camf = _camper(obj["camf"])   # per-cámara frontal (v2)
        camb = _camper(obj["camb"])   # per-cámara trasera (v2)
        i = obj["imu"]
        imu = Imu(
            heading_deg=float(i["hdg"]), left_deg=float(i["left"]),
            right_deg=float(i["right"]), disagreement_deg=float(i["disagree"]),
            left_ok=bool(i["lok"]), right_ok=bool(i["rok"]), heading_valid=bool(i["valid"]),
        )
        t = obj["tof"]
        tof = Tof(
            n=int(t["n"]),
            distances_mm=[_tof(d) for d in t["d"]],
            hcsr04_mm=_tof(t["hc"]), min_mm=_tof(t["min"]),
            zones=_zones(t.get("z")),   # campo aditivo opcional
        )
        s = obj["snap"]
        snap = Snap(
            valid=bool(s["valid"]), my_x_mm=int(s["x"]), my_y_mm=int(s["y"]),
            my_heading_deg=int(s["hdg_cd"]) / 100.0, my_confidence=int(s["conf"]),
            ball_x_mm=int(s["bx"]), ball_y_mm=int(s["by"]), ball_visible=bool(s["bvis"]),
            ball_confidence=int(s["bconf"]), ball_vx_mm_s=int(s["bvx"]),
            ball_vy_mm_s=int(s["bvy"]),
            goal_opp_angle_deg=int(s["opp_ang"]) / 100.0,
            goal_opp_distance_mm=int(s["opp_dist"]), goal_opp_visible=bool(s["opp_vis"]),
            goal_own_visible=bool(s["own_vis"]),
            goal_own_angle_deg=int(s["own_ang"]) / 100.0,
            goal_own_distance_mm=int(s["own_dist"]),
            min_obstacle_mm=_tof(s["obst"]), referee_cmd=int(s["ref"]), flags=int(s["flags"]),
        )
        b = obj["base"]
        base = Base(
            pose_fresh=bool(b["pfresh"]), pose_x_mm=int(b["px"]), pose_y_mm=int(b["py"]),
            pose_heading_deg=int(b["phdg_cd"]) / 100.0, pose_confidence=int(b["pconf"]),
            vel_fresh=bool(b["vfresh"]), vel_vx_mm_s=int(b["vx"]), vel_vy_mm_s=int(b["vy"]),
            vel_omega_deg_s=int(b["omega"]) / 100.0, vel_slip=int(b["slip"]),
        )
        ln = obj["line"]
        line = Line(
            fresh=bool(ln["fresh"]), schema=int(ln["schema"]), data_valid=bool(ln["valid"]),
            angle_deg=_na_i16_deg(ln["angle_cd"]), escape_deg=_na_i16_deg(ln["escape_cd"]),
            penetration_mm=_na_u16(ln["pen_mm"]), cross_track_mm=_na_i16(ln["cross_mm"]),
            present=bool(ln["present"]), sensors_on=int(ln["sensors"]),
            event_flags=int(ln["events"]), quality=int(ln["quality"]),
            sample_age_ms=int(ln["age_ms"]),
        )
        return TopFrame(
            v=v, seq=int(obj["seq"]), t_ms=int(obj["t_ms"]),
            cam=cam, camf=camf, camb=camb, imu=imu, tof=tof, snap=snap,
            base=base, line=line,
            frames_sent=int(obj["diag"]["frames_sent"]), raw_json=raw,
        )
    except ProtocolError:
        raise
    except (KeyError, TypeError, ValueError) as e:
        raise ProtocolError(f"frame TOP malformado: {e}") from e


def parse_line_top(line: str) -> TopFrame:
    s = line.strip()
    if not s:
        raise ProtocolError("línea vacía")
    try:
        obj = json.loads(s)
    except json.JSONDecodeError as e:
        raise ProtocolError(f"JSON inválido: {e}") from e
    if not isinstance(obj, dict):
        raise ProtocolError("se esperaba un objeto JSON por línea")
    return parse_obj_top(obj, raw=s)
