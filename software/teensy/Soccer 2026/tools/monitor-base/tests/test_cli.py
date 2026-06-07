"""Test del entrypoint headless (--selftest), sin GUI ni display."""
from monitor_base.__main__ import (
    _parse_args, main, run_selftest, run_selftest_top,
)


def test_selftest_runs_clean():
    assert run_selftest(frames=120) == 0


def test_selftest_top_runs_clean():
    assert run_selftest_top(frames=120) == 0


def test_main_top_selftest_via_args():
    assert main(["--top", "--selftest", "--selftest-frames", "80"]) == 0


def test_selftest_detects_injected_dead():
    # Con suficientes frames, el selftest valida que el sensor muerto inyectado
    # es exactamente el detectado.
    assert run_selftest(frames=160, dead=[5]) == 0


def test_main_selftest_via_args():
    assert main(["--selftest", "--selftest-frames", "100"]) == 0


def test_arg_parsing_defaults():
    args = _parse_args([])
    assert args.rate == 20.0
    assert args.baud == 115200
    assert not args.port and not args.replay


def test_arg_parsing_modes():
    assert _parse_args(["--arquero", "--sim"]).arquero is True
    assert _parse_args(["--top", "--sim"]).top is True
    base = _parse_args(["--sim"])
    assert not base.top and not base.arquero


def test_arg_parsing_mutually_exclusive():
    import pytest
    with pytest.raises(SystemExit):
        _parse_args(["--port", "COM5", "--replay", "x.jsonl"])
