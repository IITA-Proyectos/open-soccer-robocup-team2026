"""Tests del parser de protocolo contra el GOLDEN generado por el firmware C++.

El mismo string lo valida test_telemetry_down (C++) → contrato cross-lenguaje.
"""
import pytest

from monitor_base.protocol import (
    NA_I16, NA_U16, ProtocolError, SCHEMA_VERSION, decode_flags,
    is_telemetry_line, parse_line,
)


def test_golden_parses_all_fields(golden_line):
    f = parse_line(golden_line)
    assert f.v == SCHEMA_VERSION == 3
    assert f.seq == 7
    assert f.t_ms == 123456

    # Ring
    assert f.ring.n == 32
    assert len(f.ring.raw) == 32
    assert f.ring.raw[0] == 100
    assert f.ring.raw[-1] == 410
    # white bitmask 196611 = 0b...0011_0000_0000_0000_0011 → bits 0,1,16,17
    assert [i for i, w in enumerate(f.ring.white) if w] == [0, 1, 16, 17]
    assert f.ring.carpet[0] == 150 and f.ring.white_cal[0] == 800
    assert f.ring.thresholds[0] == 475          # umbral efectivo (v3)
    assert f.ring.margins[0] == 650

    # Sintonía fina (schema v3)
    assert f.ring.threshold[0] == 475 and f.ring.threshold[-1] == 506
    assert all(f.ring.enabled) and len(f.ring.enabled) == 32   # enabled_bits=0xFFFFFFFF
    assert f.ring.global_sens == -5
    assert f.ring.persensor_sens[0] == 0 and len(f.ring.persensor_sens) == 32
    assert f.ring.distance_to_threshold[0] == 100 - 475        # raw - umbral

    # Línea
    assert f.line.valid is True
    assert f.line.present is True
    assert f.line.angle_deg == pytest.approx(-12.5)     # -1250 cd / 100
    assert f.line.escape_deg == pytest.approx(167.5)
    assert f.line.penetration_mm == 42
    assert f.line.cross_track_mm == -8
    assert f.line.sensors_on_line == 5
    assert f.line.quality == 88
    assert f.line.sample_age_ms == 3
    assert f.line.flag_names == ["IMMINENT_EXIT"]

    # OTOS
    assert f.otos.n == 2
    assert f.otos.left_ok is True and f.otos.right_ok is False
    assert f.otos.x_mm == pytest.approx(123.45)
    assert f.otos.y_mm == pytest.approx(-67.80)
    assert f.otos.heading_deg == pytest.approx(12.34)
    assert f.otos.omega_rad_s == pytest.approx(0.123)
    assert f.otos.slip_mm_s == pytest.approx(1.50)
    assert f.otos.has_pose is True
    # Lecturas por-OTOS (schema v2)
    assert f.otos.left_x_mm == pytest.approx(120.10)
    assert f.otos.left_y_mm == pytest.approx(-65.20)
    assert f.otos.left_heading_deg == pytest.approx(11.50)
    assert f.otos.right_x_mm == pytest.approx(126.80)
    assert f.otos.right_y_mm == pytest.approx(-70.40)
    assert f.otos.right_heading_deg == pytest.approx(13.18)

    # Diag
    assert f.lifted is False
    assert f.line_tick == 100000
    assert f.line_tick_us == 250


def test_na_sentinels_become_none():
    line = (
        '{"v":2,"seq":0,"t_ms":0,'
        '"ring":{"n":1,"raw":[100],"white":0,"carpet":[150],"white_cal":[800]},'
        '"line":{"schema":2,"valid":0,"present":0,'
        f'"angle_cd":{NA_I16},"escape_cd":{NA_I16},"pen_mm":{NA_U16},'
        f'"xtrack_mm":{NA_I16},"non":0,"flags":0,"q":0,"age_ms":255}},'
        '"otos":{"n":0,"lok":0,"rok":0,"x":0,"y":0,"hdg":0,"vx":0,"vy":0,'
        '"w":0,"slip":0,"lx":0,"ly":0,"lh":0,"rx":0,"ry":0,"rh":0},'
        '"diag":{"lifted":0,"ltick":0,"ltick_us":0}}'
    )
    f = parse_line(line)
    assert f.line.angle_deg is None
    assert f.line.escape_deg is None
    assert f.line.penetration_mm is None
    assert f.line.cross_track_mm is None
    assert f.line.valid is False
    assert f.otos.has_pose is False


def test_unsupported_schema_raises():
    with pytest.raises(ProtocolError):
        parse_line('{"v":99,"seq":0}')


def test_bad_json_raises():
    with pytest.raises(ProtocolError):
        parse_line("{not json")
    with pytest.raises(ProtocolError):
        parse_line("")


def test_missing_key_raises():
    with pytest.raises(ProtocolError):
        parse_line('{"v":1,"seq":0,"t_ms":0,"ring":{"n":1}}')


def test_non_object_raises():
    with pytest.raises(ProtocolError):
        parse_line("[1,2,3]")


def test_decode_flags():
    assert decode_flags(0x00) == []
    assert decode_flags(0x01) == ["IMMINENT_EXIT"]
    assert decode_flags(0x08) == ["LIFTED"]
    assert "MUX_DEAD" in decode_flags(0x20)
    assert decode_flags(0xFF) == [
        "IMMINENT_EXIT", "CORNER", "LINE_END", "LIFTED",
        "CALIB_SUSPECT", "MUX_DEAD", "DEGRADED_GEOMETRY", "SENSOR_NOISY",
    ]


def test_is_telemetry_line():
    assert is_telemetry_line('{"v":1,"seq":0}')
    assert is_telemetry_line('   {"v":1}')
    assert not is_telemetry_line("[DOWN] line_ring init OK")
    assert not is_telemetry_line("")
    assert not is_telemetry_line("{}")  # sin "v":
