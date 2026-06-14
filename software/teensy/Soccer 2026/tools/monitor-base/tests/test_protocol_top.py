"""Tests del parser TOP contra el golden del firmware C++ (FASE 2)."""
import pytest

from monitor_base.protocol import ProtocolError
from monitor_base.protocol_top import (
    SCHEMA_VERSION_TOP, decode_snap_flags, parse_line_top, parse_obj_top,
)


def _v2_obj_with(tof_extra=None):
    """dict v2 mínimo válido; tof_extra se mergea al bloque 'tof'."""
    tof = {"n": 4, "d": [150, 800, 65535, 1200], "hc": 300, "min": 150}
    if tof_extra:
        tof.update(tof_extra)
    return {
        "v": 2, "seq": 0, "t_ms": 0,
        "cam": {"fok": 1, "bok": 1, "bvis": 0, "bx": 0, "by": 0, "bconf": 0,
                "bvx": 0, "bvy": 0, "gy_vis": 0, "gy_ang": 0, "gy_dist": 0,
                "gb_vis": 0, "gb_ang": 0, "gb_dist": 0, "crc": 0, "resync": 0},
        "camf": {"bvis": 0, "bx": 0, "by": 0, "gy_vis": 0, "gy_ang": 0,
                 "gy_dist": 0, "gb_vis": 0, "gb_ang": 0, "gb_dist": 0},
        "camb": {"bvis": 0, "bx": 0, "by": 0, "gy_vis": 0, "gy_ang": 0,
                 "gy_dist": 0, "gb_vis": 0, "gb_ang": 0, "gb_dist": 0},
        "imu": {"hdg": 0, "left": 0, "right": 0, "disagree": 0,
                "lok": 1, "rok": 1, "valid": 1},
        "tof": tof,
        "snap": {"valid": 0, "x": 0, "y": 0, "hdg_cd": 0, "conf": 0, "bx": 0,
                 "by": 0, "bvis": 0, "bconf": 0, "bvx": 0, "bvy": 0, "opp_ang": 0,
                 "opp_dist": 0, "opp_vis": 0, "own_vis": 0, "own_ang": 0,
                 "own_dist": 0, "obst": 65535, "ref": 0, "flags": 0},
        "base": {"pfresh": 0, "px": 0, "py": 0, "phdg_cd": 0, "pconf": 0,
                 "vfresh": 0, "vx": 0, "vy": 0, "omega": 0, "slip": 0},
        "line": {"fresh": 0, "schema": 2, "valid": 0, "angle_cd": -32768,
                 "escape_cd": -32768, "pen_mm": 65535, "cross_mm": -32768,
                 "present": 0, "sensors": 0, "events": 0, "quality": 0, "age_ms": 0},
        "diag": {"frames_sent": 0},
    }


def test_tof_zones_absent_is_none():
    # Frame v2 SIN "z" (como el golden viejo) → zones None: compat hacia atrás.
    f = parse_obj_top(_v2_obj_with())
    assert f.tof.zones is None


def test_tof_zones_present_parsed_with_sentinels():
    # Campo "z" aditivo: una grilla por sensor; 65535 → None (sin lectura en la zona).
    z = [[100, 200, 65535, 400], [500, 600, 700, 800]]
    f = parse_obj_top(_v2_obj_with({"z": z}))
    assert f.tof.zones == [[100, 200, None, 400], [500, 600, 700, 800]]


def test_golden_top_parses(golden_top_line):
    f = parse_line_top(golden_top_line)
    assert f.v == SCHEMA_VERSION_TOP == 2
    assert f.seq == 3 and f.t_ms == 5000

    # cam
    assert f.cam.front_ok is True and f.cam.back_ok is False
    assert f.cam.ball_visible is True
    assert f.cam.ball_x_mm == -120 and f.cam.ball_y_mm == 340
    assert f.cam.ball_confidence == 77
    assert f.cam.yellow_visible is True
    assert f.cam.yellow_angle_deg == pytest.approx(45.0)
    assert f.cam.blue_visible is False
    assert f.cam.crc_errors == 2 and f.cam.resyncs == 5

    # imu
    assert f.imu.heading_deg == pytest.approx(42.5)
    assert f.imu.left_deg == pytest.approx(42.1)
    assert f.imu.right_deg == pytest.approx(42.9)
    assert f.imu.left_ok and f.imu.right_ok and f.imu.heading_valid

    # tof — el índice 2 es 65535 → None
    assert f.tof.n == 4
    assert f.tof.distances_mm == [150, 800, None, 1200]
    assert f.tof.hcsr04_mm == 300
    assert f.tof.min_mm == 150

    # snap
    assert f.snap.valid is True
    assert f.snap.my_x_mm == 100 and f.snap.my_y_mm == -200
    assert f.snap.my_heading_deg == pytest.approx(42.5)
    assert f.snap.goal_opp_visible is True
    assert f.snap.goal_own_visible is False
    assert f.snap.referee_name == "START"
    assert f.snap.flag_names == ["MATCH_RUNNING", "HEADING_VALID"]
    assert f.frames_sent == 1234

    # v2 — per-cámara: la frontal ve la pelota en (-118,338) y el arco amarillo;
    # la trasera ve el azul (a -90°). El delta delata el promedio fusionado.
    assert f.camf.ball_visible is True
    assert f.camf.ball_x_mm == -118 and f.camf.ball_y_mm == 338
    assert f.camf.yellow_visible is True
    assert f.camf.yellow_angle_deg == pytest.approx(45.0)
    assert f.camf.blue_visible is False
    assert f.camb.ball_visible is False
    assert f.camb.blue_visible is True
    assert f.camb.blue_angle_deg == pytest.approx(-90.0)

    # v2 — base (OTOS de la DOWN), fresh
    assert f.base.pose_fresh is True
    assert f.base.pose_x_mm == 1500 and f.base.pose_y_mm == 2000
    assert f.base.pose_heading_deg == pytest.approx(90.0)
    assert f.base.vel_fresh is True and f.base.vel_slip == 12

    # v2 — línea + vector de escape; cross_track en N/A → None
    assert f.line.fresh is True and f.line.data_valid is True
    assert f.line.angle_deg == pytest.approx(45.0)
    assert f.line.escape_deg == pytest.approx(-135.0)   # dirección del vector de escape
    assert f.line.penetration_mm == 80                  # magnitud
    assert f.line.cross_track_mm is None                # sentinela N/A
    assert f.line.present is True
    assert f.line.event_names == ["IMMINENT_EXIT"]

    # v2 — campo "z" (zonas 4x4 por sensor), aditivo: el golden del firmware ahora
    # lo emite. sensor 0 trae 140,160,(sentinel),175; el resto sentinel → None.
    assert f.tof.zones is not None
    assert len(f.tof.zones) == 4 and all(len(s) == 16 for s in f.tof.zones)
    assert f.tof.zones[0][0] == 140 and f.tof.zones[0][1] == 160
    assert f.tof.zones[0][2] is None and f.tof.zones[0][3] == 175
    assert all(z is None for z in f.tof.zones[1])


def test_tof_and_line_na_are_none():
    # v2: todo en sentinela → ToF None, línea N/A None, base no-fresh.
    f = parse_line_top(
        '{"v":2,"seq":0,"t_ms":0,'
        '"cam":{"fok":0,"bok":0,"bvis":0,"bx":0,"by":0,"bconf":0,"bvx":0,"bvy":0,'
        '"gy_vis":0,"gy_ang":0,"gy_dist":0,"gb_vis":0,"gb_ang":0,"gb_dist":0,'
        '"crc":0,"resync":0},'
        '"camf":{"bvis":0,"bx":0,"by":0,"gy_vis":0,"gy_ang":0,"gy_dist":0,'
        '"gb_vis":0,"gb_ang":0,"gb_dist":0},'
        '"camb":{"bvis":0,"bx":0,"by":0,"gy_vis":0,"gy_ang":0,"gy_dist":0,'
        '"gb_vis":0,"gb_ang":0,"gb_dist":0},'
        '"imu":{"hdg":0,"left":0,"right":0,"disagree":0,"lok":0,"rok":0,"valid":0},'
        '"tof":{"n":4,"d":[65535,65535,65535,65535],"hc":65535,"min":65535},'
        '"snap":{"valid":0,"x":0,"y":0,"hdg_cd":0,"conf":0,"bx":0,"by":0,"bvis":0,'
        '"bconf":0,"bvx":0,"bvy":0,"opp_ang":0,"opp_dist":0,"opp_vis":0,"own_vis":0,'
        '"own_ang":0,"own_dist":0,"obst":65535,"ref":0,"flags":0},'
        '"base":{"pfresh":0,"px":0,"py":0,"phdg_cd":0,"pconf":0,'
        '"vfresh":0,"vx":0,"vy":0,"omega":0,"slip":0},'
        '"line":{"fresh":0,"schema":2,"valid":0,"angle_cd":-32768,"escape_cd":-32768,'
        '"pen_mm":65535,"cross_mm":-32768,"present":0,"sensors":0,"events":0,'
        '"quality":0,"age_ms":0},'
        '"diag":{"frames_sent":0}}'
    )
    assert f.tof.distances_mm == [None, None, None, None]
    assert f.tof.hcsr04_mm is None and f.tof.min_mm is None
    assert f.snap.min_obstacle_mm is None     # 65535 = libre
    assert f.snap.valid is False
    assert f.snap.referee_name == "STOP"
    # base no fresh + línea toda en N/A
    assert f.base.pose_fresh is False and f.base.vel_fresh is False
    assert f.line.fresh is False and f.line.data_valid is False
    assert f.line.angle_deg is None and f.line.escape_deg is None
    assert f.line.penetration_mm is None and f.line.cross_track_mm is None
    assert f.line.event_names == []


def test_unsupported_schema_raises():
    # v=3 ya no soportado (v=2 ahora sí); el chequeo de schema corta antes de los campos.
    with pytest.raises(ProtocolError):
        parse_line_top('{"v":3,"seq":0}')


def test_bad_json_raises():
    with pytest.raises(ProtocolError):
        parse_line_top("{nope")


def test_decode_snap_flags():
    assert decode_snap_flags(0) == []
    assert decode_snap_flags(0x18) == ["MATCH_RUNNING", "HEADING_VALID"]
    assert "PARTNER_ALIVE" in decode_snap_flags(0x02)
