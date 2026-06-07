"""Tests del parser TOP contra el golden del firmware C++ (FASE 2)."""
import pytest

from monitor_base.protocol import ProtocolError
from monitor_base.protocol_top import (
    SCHEMA_VERSION_TOP, decode_snap_flags, parse_line_top,
)


def test_golden_top_parses(golden_top_line):
    f = parse_line_top(golden_top_line)
    assert f.v == SCHEMA_VERSION_TOP == 1
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


def test_tof_no_reading_is_none():
    line = parse_line_top  # noqa: F841 (silence linters)
    f = parse_line_top(
        '{"v":1,"seq":0,"t_ms":0,'
        '"cam":{"fok":0,"bok":0,"bvis":0,"bx":0,"by":0,"bconf":0,"bvx":0,"bvy":0,'
        '"gy_vis":0,"gy_ang":0,"gy_dist":0,"gb_vis":0,"gb_ang":0,"gb_dist":0,'
        '"crc":0,"resync":0},'
        '"imu":{"hdg":0,"left":0,"right":0,"disagree":0,"lok":0,"rok":0,"valid":0},'
        '"tof":{"n":4,"d":[65535,65535,65535,65535],"hc":65535,"min":65535},'
        '"snap":{"valid":0,"x":0,"y":0,"hdg_cd":0,"conf":0,"bx":0,"by":0,"bvis":0,'
        '"bconf":0,"bvx":0,"bvy":0,"opp_ang":0,"opp_dist":0,"opp_vis":0,"own_vis":0,'
        '"own_ang":0,"own_dist":0,"obst":65535,"ref":0,"flags":0},'
        '"diag":{"frames_sent":0}}'
    )
    assert f.tof.distances_mm == [None, None, None, None]
    assert f.tof.hcsr04_mm is None and f.tof.min_mm is None
    assert f.snap.min_obstacle_mm is None     # 65535 = libre
    assert f.snap.valid is False
    assert f.snap.referee_name == "STOP"


def test_unsupported_schema_raises():
    with pytest.raises(ProtocolError):
        parse_line_top('{"v":2,"seq":0}')


def test_bad_json_raises():
    with pytest.raises(ProtocolError):
        parse_line_top("{nope")


def test_decode_snap_flags():
    assert decode_snap_flags(0) == []
    assert decode_snap_flags(0x18) == ["MATCH_RUNNING", "HEADING_VALID"]
    assert "PARTNER_ALIVE" in decode_snap_flags(0x02)
