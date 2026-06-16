"""Tests del parser CENTRAL contra el golden y casos de borde (FASE 3).

Mismo estilo que test_protocol_top.py: un dict mínimo válido como base + el golden
multi-línea. Verifica los accesos EXACTOS de los que dependen los paneles.
"""
import pytest

from monitor_base.protocol import ProtocolError
from monitor_base.protocol_central import (
    SCHEMA_VERSION_CENTRAL, CentralFrame, decode_snap_flags,
    is_central_line, parse_line_central, parse_obj_central,
)


def _v1_obj(**over):
    """dict CENTRAL v1 mínimo válido; over[bloque] se mergea al sub-objeto."""
    obj = {
        "central": 1, "v": 1, "seq": 0, "t_ms": 0,
        "fsm": {"role": "ATK", "state": "IDLE", "match": 0},
        "cmd": {"vx": 0, "vy": 0, "w": 0, "spin": 0},
        "pwm": [0, 0, 0],
        "top": {"rxB": 0, "fr": 0, "crc": 0, "rsy": 0, "badsz": 0,
                "fresh": 0, "age": 0},
        "down": {"rx": 0, "crc": 0, "lost": 0, "rsy": 0, "badsch": 0,
                 "line_fresh": 0, "valid": 0, "ev": 0, "age": 0},
        "otos": {"fresh": 0, "hdg": 0.0},
        "snap": {"ball_vis": 0, "ball_x": 0, "ball_y": 0, "goal_own_ang": 0.0,
                 "goal_own_dist": 0, "referee": 0, "flags": 0},
        "loop": {"max_us": 0, "ema_us": 0},
    }
    for k, v in over.items():
        if isinstance(v, dict) and isinstance(obj.get(k), dict):
            obj[k].update(v)
        else:
            obj[k] = v
    return obj


# ── Estructura y tipos básicos ────────────────────────────────────────────────

def test_min_obj_parses_with_exact_accessors():
    f = parse_obj_central(_v1_obj())
    assert isinstance(f, CentralFrame)
    assert f.v == SCHEMA_VERSION_CENTRAL == 1
    assert f.seq == 0 and f.t_ms == 0
    # fsm
    assert f.fsm.role == "ATK" and f.fsm.state == "IDLE" and f.fsm.match is False
    # cmd (ints)
    assert f.cmd.vx == 0 and f.cmd.vy == 0 and f.cmd.w == 0 and f.cmd.spin == 0
    # pwm
    assert f.pwm == [0, 0, 0] and len(f.pwm) == 3
    # top / down link
    assert f.top.rxB == 0 and f.top.fresh is False and f.top.age == 0
    assert f.down.rx == 0 and f.down.line_fresh is False and f.down.valid is False
    # otos
    assert f.otos.fresh is False and f.otos.hdg == pytest.approx(0.0)
    # snap
    assert f.snap.ball_vis is False and f.snap.referee == 0
    # loop
    assert f.loop.max_us == 0 and f.loop.ema_us == 0


def test_bool_fields_coerced():
    f = parse_obj_central(_v1_obj(
        fsm={"match": 1}, top={"fresh": 1}, down={"line_fresh": 1, "valid": 1},
        otos={"fresh": 1}, snap={"ball_vis": 1}))
    assert f.fsm.match is True
    assert f.top.fresh is True
    assert f.down.line_fresh is True and f.down.valid is True
    assert f.otos.fresh is True
    assert f.snap.ball_vis is True


# ── Golden del contrato ───────────────────────────────────────────────────────

def test_golden_central_parses(golden_central_line):
    f = parse_line_central(golden_central_line)
    assert f.v == SCHEMA_VERSION_CENTRAL == 1
    assert f.seq == 10 and f.t_ms == 2000

    # fsm: delantero atacando, partido corriendo
    assert f.fsm.role == "ATK" and f.fsm.state == "ATTACK"
    assert f.fsm.match is True

    # cmd
    assert f.cmd.vx == 120 and f.cmd.vy == -200
    assert f.cmd.w == 4250 and f.cmd.spin == 0

    # pwm real por rueda
    assert f.pwm == [200, -150, 40]

    # enlace TOP (fresco)
    assert f.top.rxB == 4096 and f.top.fr == 372
    assert f.top.crc == 1 and f.top.rsy == 0 and f.top.badsz == 0
    assert f.top.fresh is True and f.top.age == 4

    # enlace DOWN (fresco, línea válida); lost y crc SEPARADOS
    assert f.down.rx == 355 and f.down.crc == 2 and f.down.lost == 1
    assert f.down.rsy == 0 and f.down.badsch == 0
    assert f.down.line_fresh is True and f.down.valid is True
    assert f.down.ev == 1 and f.down.age == 3

    # otos
    assert f.otos.fresh is True
    assert f.otos.hdg == pytest.approx(42.5)

    # snap
    assert f.snap.ball_vis is True
    assert f.snap.ball_x == -118 and f.snap.ball_y == 338
    assert f.snap.goal_own_ang == pytest.approx(178.5)
    assert f.snap.goal_own_dist == 1800
    assert f.snap.referee == 1 and f.snap.referee_name == "START"
    assert f.snap.flags == 24
    assert f.snap.flag_names == ["MATCH_RUNNING", "HEADING_VALID"]

    # loop
    assert f.loop.max_us == 9500 and f.loop.ema_us == 2200

    # raw_json preservado (para el recorder/replay)
    assert f.raw_json == golden_central_line


def test_golden_all_lines_parse(golden_central_path):
    with open(golden_central_path, encoding="utf-8") as fh:
        lines = [ln for ln in fh if ln.strip()]
    frames = [parse_line_central(ln) for ln in lines]
    assert len(frames) == 4
    assert [f.seq for f in frames] == [10, 11, 12, 13]
    # línea 2 (seq 11): arquero con TOP CAÍDO (fresh=0, age alto)
    assert frames[1].fsm.role == "GK"
    assert frames[1].top.fresh is False and frames[1].top.age == 340
    # línea 3 (seq 12): partido PARADO, línea no válida
    assert frames[2].fsm.match is False
    assert frames[2].down.valid is False
    assert frames[2].snap.referee_name == "STOP"
    # línea 4 (seq 13): OTOS no fresco + DOWN no fresco + loop largo
    assert frames[3].otos.fresh is False
    assert frames[3].down.line_fresh is False
    assert frames[3].loop.max_us == 41000


# ── Errores y validación de schema ────────────────────────────────────────────

def test_missing_central_key_raises():
    obj = _v1_obj()
    del obj["central"]
    with pytest.raises(ProtocolError):
        parse_obj_central(obj)


def test_unsupported_schema_raises():
    with pytest.raises(ProtocolError):
        parse_line_central('{"central":1,"v":2,"seq":0}')


def test_missing_field_raises():
    obj = _v1_obj()
    del obj["cmd"]["spin"]
    with pytest.raises(ProtocolError):
        parse_obj_central(obj)


def test_bad_json_raises():
    with pytest.raises(ProtocolError):
        parse_line_central("{nope")


def test_empty_line_raises():
    with pytest.raises(ProtocolError):
        parse_line_central("   ")


def test_non_object_raises():
    with pytest.raises(ProtocolError):
        parse_line_central("[1,2,3]")


# ── Helpers ───────────────────────────────────────────────────────────────────

def test_decode_snap_flags():
    assert decode_snap_flags(0) == []
    assert decode_snap_flags(0x18) == ["MATCH_RUNNING", "HEADING_VALID"]
    assert "PARTNER_ALIVE" in decode_snap_flags(0x02)


def test_is_central_line():
    assert is_central_line('{"central":1,"v":1,"seq":0}') is True
    # un frame TOP (trae "cam", no "central") NO matchea
    assert is_central_line('{"v":2,"cam":{},"seq":0}') is False
    # un frame DOWN (trae "ring") tampoco
    assert is_central_line('{"v":3,"ring":{},"seq":0}') is False
    assert is_central_line("texto suelto") is False
