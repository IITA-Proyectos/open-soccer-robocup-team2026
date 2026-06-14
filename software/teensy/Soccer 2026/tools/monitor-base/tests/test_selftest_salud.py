"""Smoke headless del pipeline de la vista de SALUD (sin GUI ni hardware)."""


def test_selftest_top_salud_returns_zero(capsys):
    from monitor_base.__main__ import run_selftest_top_salud
    rc = run_selftest_top_salud(120)
    assert rc == 0
    out = capsys.readouterr().out.lower()
    assert "salud" in out
    # El simulador deja caer un ToF de a ratos → tiene que haber visto SIN DATO/FALLA.
    assert "sin dato" in out or "falla" in out


def test_cli_top_salud_selftest_via_main():
    from monitor_base.__main__ import main
    rc = main(["--top-salud", "--selftest", "--selftest-frames", "60"])
    assert rc == 0
