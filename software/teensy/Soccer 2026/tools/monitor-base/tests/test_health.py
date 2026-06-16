"""Tests de health.py — veredicto de salud por sensor de la placa TOP.

El valor operativo de la app: que la luz correcta se ponga roja/amarilla cuando
un sensor falla. Se testea con frames sintéticos (sin robot ni hardware).
"""
import copy

import pytest

from monitor_base.health import Status, evaluate
from monitor_base.protocol_top import parse_obj_top

# Frame v2 TODO SANO (base para mutar un sensor por test).
_BASE = {
    "v": 2, "seq": 0, "t_ms": 0,
    "cam": {"fok": 1, "bok": 1, "bvis": 1, "bx": 0, "by": 200, "bconf": 80,
            "bvx": 0, "bvy": 0, "gy_vis": 1, "gy_ang": 0, "gy_dist": 1200,
            "gb_vis": 1, "gb_ang": 18000, "gb_dist": 2400, "crc": 0, "resync": 0},
    "camf": {"bvis": 1, "bx": 0, "by": 200, "gy_vis": 1, "gy_ang": 0,
             "gy_dist": 1200, "gb_vis": 0, "gb_ang": 0, "gb_dist": 0},
    "camb": {"bvis": 0, "bx": 0, "by": 0, "gy_vis": 0, "gy_ang": 0,
             "gy_dist": 0, "gb_vis": 1, "gb_ang": 18000, "gb_dist": 2400},
    "imu": {"hdg": 0, "left": 0, "right": 0, "disagree": 0.5,
            "lok": 1, "rok": 1, "valid": 1},
    "tof": {"n": 4, "d": [200, 300, 400, 500], "hc": 350, "min": 200},
    "snap": {"valid": 1, "x": 0, "y": 0, "hdg_cd": 0, "conf": 80, "bx": 0,
             "by": 200, "bvis": 1, "bconf": 80, "bvx": 0, "bvy": 0, "opp_ang": 0,
             "opp_dist": 1200, "opp_vis": 1, "own_vis": 0, "own_ang": 0,
             "own_dist": 0, "obst": 200, "ref": 1, "flags": 24},
    "base": {"pfresh": 1, "px": 0, "py": 0, "phdg_cd": 0, "pconf": 70,
             "vfresh": 1, "vx": 0, "vy": 0, "omega": 0, "slip": 0},
    "line": {"fresh": 1, "schema": 2, "valid": 1, "angle_cd": 0, "escape_cd": 0,
             "pen_mm": 0, "cross_mm": 0, "present": 1, "sensors": 7,
             "events": 0, "quality": 90, "age_ms": 2},
    "diag": {"frames_sent": 100},
}


def make_frame(**sections):
    obj = copy.deepcopy(_BASE)
    for k, v in sections.items():
        obj[k].update(v)
    return parse_obj_top(obj)


def status_of(items, key):
    return next(i.status for i in items if i.key == key)


def test_all_healthy_no_dead_no_warn():
    items = evaluate(make_frame())
    assert all(i.status not in (Status.DEAD, Status.WARN) for i in items), \
        [(i.key, i.status) for i in items if i.status in (Status.DEAD, Status.WARN)]


def test_camera_front_dead_when_link_down():
    items = evaluate(make_frame(cam={"fok": 0}))
    assert status_of(items, "cam_front") == Status.DEAD


def test_phantom_ball_when_both_cameras_disagree():
    # Ambas ven pelota en posiciones MUY distintas (Δ=600mm) → fantasma del promedio.
    items = evaluate(make_frame(
        camf={"bvis": 1, "bx": 300, "by": 0},
        camb={"bvis": 1, "bx": -300, "by": 0},
    ))
    assert status_of(items, "ball_fusion") == Status.WARN


def test_no_phantom_when_cameras_agree():
    items = evaluate(make_frame(
        camf={"bvis": 1, "bx": 100, "by": 0},
        camb={"bvis": 1, "bx": 120, "by": 0},   # Δ=20mm
    ))
    assert status_of(items, "ball_fusion") == Status.OK


def test_heading_dead_when_invalid():
    items = evaluate(make_frame(imu={"valid": 0}))
    assert status_of(items, "heading") == Status.DEAD


def test_heading_warn_on_disagreement():
    items = evaluate(make_frame(imu={"disagree": 8.0}))
    assert status_of(items, "heading") == Status.WARN


def test_bno_dead_when_not_ready():
    # rok=0 y SIN centinela (sok ausente → default 0) → falla real.
    items = evaluate(make_frame(imu={"rok": 0}))
    assert status_of(items, "bno_right") == Status.DEAD
    assert status_of(items, "bno_left") == Status.OK


def test_bno_secundario_centinela_no_es_falla():
    # Modo primario-solo (TOP_BNO_PRIMARY_ONLY): el 2º BNO NO entra a la fusión (rok=0)
    # pero vive como CENTINELA @1Hz (sok=1) → debe verse SANO (OK), no rojo, con su heading.
    items = evaluate(make_frame(imu={"rok": 0, "sok": 1, "sdeg": -33.2}))
    assert status_of(items, "bno_right") == Status.OK
    detail = next(i.detail for i in items if i.key == "bno_right")
    assert "centinela" in detail.lower()
    assert "-33.2" in detail


def test_tof_nodata_on_sentinel():
    items = evaluate(make_frame(tof={"d": [200, 65535, 400, 500]}))
    assert status_of(items, "tof_1") == Status.NODATA
    assert status_of(items, "tof_0") == Status.OK


def test_ultrasonic_nodata_on_sentinel():
    items = evaluate(make_frame(tof={"d": [200, 300, 400, 500], "hc": 65535}))
    assert status_of(items, "ultrasonic") == Status.NODATA


def test_otos_pose_nodata_when_stale():
    items = evaluate(make_frame(base={"pfresh": 0}))
    assert status_of(items, "otos_pose") == Status.NODATA


def test_otos_vel_warn_on_slip():
    items = evaluate(make_frame(base={"vfresh": 1, "slip": 60}))
    assert status_of(items, "otos_vel") == Status.WARN


def test_line_warn_when_invalid():
    items = evaluate(make_frame(line={"fresh": 1, "valid": 0}))
    assert status_of(items, "line") == Status.WARN


def test_line_dead_on_mux_dead_event():
    items = evaluate(make_frame(line={"fresh": 1, "events": 0x20}))  # MUX_DEAD
    assert status_of(items, "line") == Status.DEAD


def test_line_nodata_when_not_fresh():
    items = evaluate(make_frame(line={"fresh": 0}))
    assert status_of(items, "line") == Status.NODATA


def test_snapshot_nodata_when_not_valid():
    items = evaluate(make_frame(snap={"valid": 0}))
    assert status_of(items, "snapshot") == Status.NODATA


def test_every_item_has_label_and_detail():
    for i in evaluate(make_frame()):
        assert i.key and i.label and isinstance(i.detail, str)
