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
                     help="puerto serie del Teensy (COM5, /dev/ttyACM0, o 'auto' para detectarlo)")
    src.add_argument("--replay", metavar="ARCHIVO.jsonl",
                     help="reproducir una grabación de telemetría")
    p.add_argument("--list-ports", action="store_true",
                   help="lista los puertos serie disponibles (cuál parece el Teensy) y sale")
    p.add_argument("--baud", type=int, default=115200, help="baud (default 115200)")
    p.add_argument("--rate", type=float, default=20.0,
                   help="tasa Hz para sim/replay (default 20)")
    p.add_argument("--no-loop", action="store_true",
                   help="con --replay: no repetir en bucle")
    p.add_argument("--record", metavar="ARCHIVO.jsonl", default=None,
                   help="grabar la telemetría entrante a un .jsonl (para --replay/análisis)")
    p.add_argument("--sim-dead", default="",
                   help="con --sim: índices de sensores 'muertos' a inyectar, ej 5,17")
    p.add_argument("--top", action="store_true",
                   help="modo TOP (placa superior: cámaras/IMU/ToF/snapshot) en vez de la base")
    p.add_argument("--arquero", action="store_true",
                   help="vista de ARQUERO (seguidor de línea + OTOS izq/der): para probar en banco")
    p.add_argument("--selftest", action="store_true",
                   help="smoke headless: procesa N frames del sim y sale (sin GUI)")
    p.add_argument("--selftest-frames", type=int, default=200,
                   help="cuántos frames procesa --selftest (default 200)")
    return p.parse_args(argv)


def _dead_list(s: str) -> List[int]:
    return [int(x) for x in s.split(",") if x.strip().isdigit()] if s else []


def _build_source(args: argparse.Namespace):
    from .sources import ReplaySource, SerialSource, SimSource, SimTopSource
    if args.top:
        from .protocol_top import parse_line_top
        if args.port:
            return SerialSource(args.port, baud=args.baud, parser=parse_line_top)
        if args.replay:
            return ReplaySource(args.replay, rate_hz=args.rate,
                                loop=not args.no_loop, parser=parse_line_top)
        return SimTopSource(rate_hz=args.rate)
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
    print(f"[selftest] OTOS izq/der (v2) : L=({last.otos.left_x_mm:.1f},"
          f"{last.otos.left_y_mm:.1f}) R=({last.otos.right_x_mm:.1f},"
          f"{last.otos.right_y_mm:.1f})")
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


def run_selftest_top(frames: int = 200) -> int:
    """Smoke headless de la vista TOP: procesa N frames del simulador TOP por el
    parser real y reporta un resumen. Devuelve 0 si todo anduvo."""
    from .protocol_top import parse_line_top
    from .simulator_top import SimulatorTop

    sim = SimulatorTop(rate_hz=20.0)
    last = None
    seen_ball = False
    for _ in range(frames):
        last = parse_line_top(sim.next_line())
        seen_ball = seen_ball or last.cam.ball_visible
    assert last is not None
    print(f"[selftest-top] frames procesados : {frames}")
    print(f"[selftest-top] último seq         : {last.seq}")
    print(f"[selftest-top] IMU heading        : {last.imu.heading_deg:.1f} "
          f"(L={last.imu.left_ok} R={last.imu.right_ok} valid={last.imu.heading_valid})")
    print(f"[selftest-top] cámaras            : F={last.cam.front_ok} B={last.cam.back_ok} "
          f"vio pelota={seen_ball}")
    print(f"[selftest-top] ToF                : {last.tof.distances_mm} min={last.tof.min_mm}")
    print(f"[selftest-top] snapshot           : valid={last.snap.valid} "
          f"ref={last.snap.referee_name} flags={last.snap.flag_names}")
    print("[selftest-top] OK")
    return 0


def list_ports() -> int:
    """Lista los puertos serie y marca cuál parece el Teensy."""
    from .sources import list_serial_ports, autodetect_port
    ports = list_serial_ports()
    if not ports:
        print("No hay puertos serie (o falta pyserial: pip install pyserial).")
        return 1
    auto = autodetect_port()
    print("Puertos serie disponibles:")
    for p in ports:
        mark = "  <-- probable Teensy" if p["is_teensy"] else ""
        star = " *" if p["device"] == auto else ""
        print(f"  {p['device']:<8}{star}  {p['description']}{mark}")
    if auto:
        print(f"\nAutodetección elegiría: {auto}")
        print(f"Corré:  python -m monitor_base --port {auto}   (o  --port auto)")
    else:
        print("\nNo pude elegir uno solo automáticamente; pasá el COM con --port COMx.")
    return 0


def main(argv: Optional[List[str]] = None) -> int:
    args = _parse_args(argv)
    if args.list_ports:
        return list_ports()
    if args.selftest:
        if args.top:
            return run_selftest_top(args.selftest_frames)
        return run_selftest(args.selftest_frames, _dead_list(args.sim_dead))

    source = _build_source(args)

    recorder = None
    if args.record:
        from .recorder import Recorder
        recorder = Recorder(args.record)
        print(f"Grabando telemetría en {args.record}")
    try:
        if args.top:
            from . import gui_top
            gui_top.run_top(source, recorder=recorder)
        elif args.arquero:
            from . import gui_gk
            gui_gk.run_gk(source, recorder=recorder)
        else:
            from . import gui
            gui.run(source, recorder=recorder)
    except Exception as e:  # noqa: BLE001 — tkinter sin display, etc.
        print(f"No se pudo iniciar la GUI ({e}). "
              f"Probá --selftest para un chequeo sin ventana.", file=sys.stderr)
        return 2
    finally:
        if recorder is not None:
            recorder.close()
            print(f"Grabados {recorder.count} frames en {args.record}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
