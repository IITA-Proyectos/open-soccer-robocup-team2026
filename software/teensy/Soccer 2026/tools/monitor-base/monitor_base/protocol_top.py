"""protocol_top.py — Parser del contrato de telemetría TOP v1 (FASE 2).

Espejo en Python de src/shared/telemetry_top.h. Contrato:
docs/firmware/TELEMETRIA-TOP.md. Núcleo PURO (sin GUI/serial) → testeable contra
el golden del firmware (tests/golden_top_v1.jsonl).
"""
from __future__ import annotations

import json
from dataclasses import dataclass
from typing import List, Optional

from .protocol import ProtocolError  # reusa la misma excepción

SCHEMA_VERSION_TOP = 1
TOF_NO_READING = 65535

# Flags del WorldSnapshot (snap.flags).
SNAP_FLAGS = [
    (0x01, "IN_OWN_PENALTY"),
    (0x02, "PARTNER_ALIVE"),
    (0x04, "PARTNER_SEES_BALL"),
    (0x08, "MATCH_RUNNING"),
    (0x10, "HEADING_VALID"),
]

REFEREE = {0: "STOP", 1: "START", 2: "HALFTIME", 3: "RESET"}


def decode_snap_flags(flags: int) -> List[str]:
    return [name for bit, name in SNAP_FLAGS if flags & bit]


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
class TopFrame:
    v: int
    seq: int
    t_ms: int
    cam: Cam
    imu: Imu
    tof: Tof
    snap: Snap
    frames_sent: int
    raw_json: str = ""


def _tof(v: int) -> Optional[int]:
    return None if int(v) == TOF_NO_READING else int(v)


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
        return TopFrame(
            v=v, seq=int(obj["seq"]), t_ms=int(obj["t_ms"]),
            cam=cam, imu=imu, tof=tof, snap=snap,
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
