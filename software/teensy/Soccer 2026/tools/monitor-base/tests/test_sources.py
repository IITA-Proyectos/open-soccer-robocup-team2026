"""Tests de los helpers puros de fuentes (sin hilos ni hardware)."""
from monitor_base import sources
from monitor_base.sources import parse_lines, read_replay_file


def _ports(*items):
    return list(items)


def _p(device, desc="", vid=None, teensy=False):
    return {"device": device, "description": desc, "vid": vid, "pid": 1,
            "is_teensy": teensy}


def test_autodetect_prefers_teensy(monkeypatch):
    monkeypatch.setattr(sources, "list_serial_ports", lambda: _ports(
        _p("COM1", "Communications Port"),
        _p("COM5", "USB Serial Device", vid=0x16C0, teensy=True)))
    assert sources.autodetect_port() == "COM5"


def test_autodetect_single_port(monkeypatch):
    monkeypatch.setattr(sources, "list_serial_ports",
                        lambda: _ports(_p("COM3", "USB Serial Device")))
    assert sources.autodetect_port() == "COM3"


def test_autodetect_ambiguous_returns_none(monkeypatch):
    monkeypatch.setattr(sources, "list_serial_ports", lambda: _ports(
        _p("COM1", "Bluetooth"), _p("COM2", "Bluetooth")))
    assert sources.autodetect_port() is None


def test_autodetect_no_ports(monkeypatch):
    monkeypatch.setattr(sources, "list_serial_ports", lambda: [])
    assert sources.autodetect_port() is None


class _FakeSerial:
    """Serial de mentira: devuelve unas líneas y después 'idle' (b'')."""
    def __init__(self, lines):
        import time
        self._time = time
        self._lines = list(lines)

    def readline(self):
        if self._lines:
            return (self._lines.pop(0) + "\n").encode("utf-8")
        self._time.sleep(0.02)   # emula el timeout de pyserial (no busy-spin)
        return b""

    def close(self):
        pass


def test_serialsource_surfaces_boot_text_and_parses_frames(golden_line, monkeypatch):
    import time
    src = sources.SerialSource("COMx")
    fake = _FakeSerial(["[DOWN] line_ring init OK", golden_line])
    monkeypatch.setattr(src, "_open", lambda: fake)
    src.start()
    frames = []
    deadline = time.time() + 2.0
    while time.time() < deadline and not frames:
        frames = src.poll()
        time.sleep(0.02)
    src.stop()
    texts = []
    while True:
        try:
            texts.append(src.text.get_nowait())
        except Exception:
            break
    # el print de boot se publica como TEXTO (no se traga), y la telemetría se parsea
    assert any("line_ring" in t for t in texts)
    assert any(getattr(f, "seq", None) == 7 for f in frames)


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
