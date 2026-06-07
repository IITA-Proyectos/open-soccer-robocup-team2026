"""Tests de los helpers puros de fuentes (sin hilos ni hardware)."""
from monitor_base.sources import parse_lines, read_replay_file


def test_parse_lines_skips_noise_and_reports_errors(golden_line):
    errors = []
    lines = [
        "[DOWN] line_ring init OK",          # print de boot → se saltea
        golden_line,                          # telemetría válida
        "basura suelta sin json",            # no telemetría → se saltea
        '{"v":99,"seq":1}',                  # parece telemetría pero schema malo
        '{"v":1,"seq":2,"t_ms":0,"ring":{"n":1}}',  # telemetría incompleta
    ]
    frames = list(parse_lines(lines, on_error=lambda ln, e: errors.append(ln)))
    assert len(frames) == 1
    assert frames[0].seq == 7
    # las 2 líneas telemetría-like rotas dispararon on_error
    assert len(errors) == 2


def test_parse_lines_no_error_callback_is_silent(golden_line):
    frames = list(parse_lines([golden_line, '{"v":99}']))
    assert len(frames) == 1


def test_read_replay_file(golden_path):
    frames = read_replay_file(golden_path)
    assert len(frames) == 1
    assert frames[0].seq == 7
    assert frames[0].line.sensors_on_line == 5
