"""Entrypoint de la app de base. Uso:

  python -m monitor_base --sim                 # simulador (sin robot)
  python -m monitor_base --port COM5           # Teensy real por USB
  python -m monitor_base --replay grab.jsonl   # reproducir una grabación
  python -m monitor_base --selftest            # smoke headless (sin ventana)

Para grabar mientras mirás: redirigí el Serial a archivo, o usá el botón de la
GUI (futuro). Por ahora --replay consume archivos .jsonl ya grabados.
"""
from __future__ import annotations

import argparse
import sys
from typing import List, Optional


def _parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        prog="monitor_base",
        description="Monitor/calibración de la placa base (DOWN) — IITA Soccer.")
    src = p.add_mutually_exclusive_group()
    src.add_argument("--sim", action="store_true",
                     help="usar el simulador (sin robot) [default]")
    src.add_argument("--port", metavar="COMx",
                     help="puerto serie del Teensy DOWN (p.ej. COM5 o /dev/ttyACM0)")
    src.add_argument("--replay", metavar="ARCHIVO.jsonl",
                     help="reproducir una grabación de telemetría")
    p.add_argument("--baud", type=int, default=115200, help="baud (default 115200)")
    p.add_argument("--rate", type=float, default=20.0,
                   help="tasa Hz para sim/replay (default 20)")
    p.add_argument("--no-loop", action="store_true",
                   help="con --replay: no repetir en bucle")
    p.add_argument("--sim-dead", default="",
                   help="con --sim: índices de sensores 'muertos' a inyectar, ej 5,17")
    p.add_argument("--selftest", action="store_true",
                   help="smoke headless: procesa N frames del sim y sale (sin GUI)")
    p.add_argument("--selftest-frames", type=int, default=200,
                   help="cuántos frames procesa --selftest (default 200)")
    return p.parse_args(argv)


def _dead_list(s: str) -> List[int]:
    return [int(x) for x in s.split(",") if x.strip().isdigit()] if s else []


def _build_source(args: argparse.Namespace):
    from .sources import ReplaySource, SerialSource, SimSource
    if args.port:
        return SerialSource(args.port, baud=args.baud)
    if args.replay:
        return ReplaySource(args.replay, rate_hz=args.rate, loop=not args.no_loop)
    return SimSource(rate_hz=args.rate, dead_sensors=_dead_list(args.sim_dead))


def run_selftest(frames: int = 200, dead: Optional[List[int]] = None) -> int:
    """Procesa N frames del simulador por el camino real (sin GUI ni hilos) y
    reporta un resumen. Devuelve 0 si todo anduvo. Sirve de 'compila/corre' sin
    display y de chequeo de integración del pipeline parse→salud→calib."""
    from .calibration import CalibrationAssistant
    from .protocol import parse_line
    from .sensor_health import SensorHealthTracker
    from .simulator import Simulator

    sim = Simulator(rate_hz=20.0, noise=0, dead_sensors=dead or [])
    health = SensorHealthTracker(n=32)
    calib = CalibrationAssistant(n=32)
    calib.start()
    last = None
    for _ in range(frames):
        f = parse_line(sim.next_line())
        health.update(f.ring.raw)
        calib.update(f.ring.raw)
        last = f
    calib.stop()
    assert last is not None

    problems = health.problems()
    print(f"[selftest] frames procesados : {frames}")
    print(f"[selftest] último seq         : {last.seq}")
    print(f"[selftest] schema            : v{last.v}")
    print(f"[selftest] línea válida      : {last.line.valid} "
          f"present={last.line.present} non={last.line.sensors_on_line} "
          f"ang={last.line.angle_deg}")
    print(f"[selftest] OTOS              : x={last.otos.x_mm:.1f} "
          f"y={last.otos.y_mm:.1f} hdg={last.otos.heading_deg:.1f} "
          f"L={last.otos.left_ok} R={last.otos.right_ok}")
    print(f"[selftest] sensores problema : {[s.index for s in problems]}")
    print(f"[selftest] auto-calib muestras: {calib._samples}")
    expected_dead = sorted(dead or [])
    got_dead = sorted(s.index for s in problems)
    if expected_dead and got_dead != expected_dead:
        print(f"[selftest] FALLO: esperaba muertos {expected_dead}, "
              f"detectó {got_dead}")
        return 1
    print("[selftest] OK")
    return 0


def main(argv: Optional[List[str]] = None) -> int:
    args = _parse_args(argv)
    if args.selftest:
        return run_selftest(args.selftest_frames, _dead_list(args.sim_dead))

    source = _build_source(args)
    try:
        from . import gui
    except Exception as e:  # noqa: BLE001 — tkinter sin display, etc.
        print(f"No se pudo iniciar la GUI ({e}). "
              f"Probá --selftest para un chequeo sin ventana.", file=sys.stderr)
        return 2
    gui.run(source)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
