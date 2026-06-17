"""Entrypoint de la app de base. Uso:

  python -m monitor_base --sim                 # simulador placa TOP (sin robot)
  python -m monitor_base --sim-central         # simulador placa CENTRAL (sin robot)
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
                     help="usar el simulador de la placa TOP (sin robot) [default]")
    src.add_argument("--sim-central", action="store_true",
                     help="usar el simulador de la placa CENTRAL (sin robot): "
                          "ejercita los paneles Cerebro / Salud CENTRAL / Cancha / "
                          "Timeline / Hz con pose XY + pelota + arcos sintéticos")
    src.add_argument("--port", metavar="COMx",
                     help="puerto serie del Teensy (COM5, /dev/ttyACM0, o 'auto' para detectarlo)")
    src.add_argument("--replay", metavar="ARCHIVO.jsonl",
                     help="reproducir una grabación de telemetría")
    p.add_argument("--list-ports", action="store_true",
                   help="lista los puertos serie disponibles (cuál parece el Teensy) y sale")
    p.add_argument("--readme", "--guia", action="store_true",
                   help="muestra la guía completa de la aplicación y sale")
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
    p.add_argument("--top-salud", action="store_true",
                   help="modo TOP — TABLERO DE SALUD por sensor (verde/rojo) + zonas ToF + botones de config")
    p.add_argument("--arquero", action="store_true",
                   help="vista de ARQUERO (seguidor de línea + OTOS izq/der): para probar en banco")
    p.add_argument("--tof-setup", action="store_true",
                   help="modo TOP — CONFIGURAR los ToF: ubicación, rotar/espejar, y vetar zonas "
                        "según altura de pared + cancha (guarda a .json; baja a firmware)")
    p.add_argument("--tof-360", action="store_true",
                   help="modo TOP — TIRA 360 de los 4 ToF (izq|frente|der|atrás) + cámaras arriba")
    p.add_argument("--timeline", action="store_true",
                   help="modo TOP — TIMELINE/caja-negra EN VIVO del snapshot TOP→CENTRAL")
    p.add_argument("--cam-fusion", action="store_true",
                   help="modo TOP — comparador de CÁMARA front/back vs fusión (anti-pelota-fantasma)")
    p.add_argument("--polar", action="store_true",
                   help="modo TOP — vista POLAR cenital: conos de ToF (profundidad por zona) + "
                        "conos de cámara (pelota/arcos sí-no + posición)")
    p.add_argument("--field", action="store_true",
                   help="modo TOP — vista de CANCHA: robot ubicado por OTOS con orientación + "
                        "estela, y pelota con estela (distancia sin calibrar)")
    p.add_argument("--monitor", action="store_true",
                   help="MONITOR UNIFICADO: consola industrial con todas las vistas en un menú + "
                        "logs (la forma recomendada de usar el monitor TOP)")
    p.add_argument("--selftest", action="store_true",
                   help="smoke headless: procesa N frames del sim y sale (sin GUI)")
    p.add_argument("--selftest-frames", type=int, default=200,
                   help="cuántos frames procesa --selftest (default 200)")
    return p.parse_args(argv)


def _dead_list(s: str) -> List[int]:
    return [int(x) for x in s.split(",") if x.strip().isdigit()] if s else []


def _wants_shell(args: argparse.Namespace) -> bool:
    """La CONSOLA unificada es el modo por defecto (y de --monitor + flags de vista
    TOP). Las únicas excepciones son las vistas viejas standalone --top / --arquero."""
    return not (args.top or args.arquero)


def _build_source(args: argparse.Namespace):
    from .sources import (ReplaySource, SerialSource, SimCentralSource, SimSource,
                          SimTopSource, auto_parse)
    if _wants_shell(args):
        # Fuente MULTI-PLACA: sniffea TOP vs BASE por línea (hot-swap del USB).
        if args.port:
            return SerialSource(args.port, baud=args.baud, parser=auto_parse)
        if args.replay:
            return ReplaySource(args.replay, rate_hz=args.rate,
                                loop=not args.no_loop, parser=auto_parse)
        if args.sim_central:
            return SimCentralSource(rate_hz=args.rate)   # sim de la placa CENTRAL
        if args.sim:
            return SimTopSource(rate_hz=args.rate)   # el sim es de la placa TOP
        # Sin flag de fuente: AUTODETECTAR el Teensy y sniffear qué placa es.
        return SerialSource("auto", baud=args.baud, parser=auto_parse)
    # Vistas viejas standalone (--arquero = base; --top = radar viejo).
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


def run_selftest_top_salud(frames: int = 200) -> int:
    """Smoke headless de la vista de SALUD: corre N frames del simulador por el
    pipeline real (parser → salud → zonas) y reporta el tablero. Devuelve 0 si OK.
    No abre ventana (sirve de CI y de 'corre sin display')."""
    from .health import (STATUS_LABEL, Status, counts, evaluate, worst_status)
    from .protocol_top import parse_line_top
    from .simulator_top import SimulatorTop
    from .zones import ZoneGrid

    # Consolas Windows (cp1252) no bancan °/Δ/↔ → salida UTF-8 tolerante (como
    # tools/blackbox/analizar_corrida.py). La GUI no usa esta ruta.
    if hasattr(sys.stdout, "reconfigure"):
        try:
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        except Exception:  # noqa: BLE001
            pass

    sim = SimulatorTop(rate_hz=20.0)
    last = None
    saw_zones = False
    for _ in range(frames):
        f = parse_line_top(sim.next_line())
        last = evaluate(f)
        if f.tof.zones:
            for sensor in f.tof.zones:
                if ZoneGrid.from_flat(sensor, width=4).valid_count > 0:
                    saw_zones = True
    assert last is not None
    c = counts(last)
    print(f"[selftest-salud] frames procesados : {frames}")
    print(f"[selftest-salud] sensores          : OK={c[Status.OK]} "
          f"REVISAR={c[Status.WARN]} FALLA={c[Status.DEAD]} SIN DATO={c[Status.NODATA]}")
    print(f"[selftest-salud] peor estado       : {STATUS_LABEL[worst_status(last)]}")
    print(f"[selftest-salud] zonas ToF         : {'presentes' if saw_zones else 'AUSENTES'}")
    for it in last:
        print(f"   {it.label:18s} {STATUS_LABEL[it.status]:9s} {it.detail}")
    if not saw_zones:
        print("[selftest-salud] FALLO: no llegaron zonas del simulador")
        return 1
    print("[selftest-salud] OK")
    return 0


def run_selftest_tof_setup(frames: int = 200) -> int:
    """Smoke headless de la config de ToF: corre frames del sim por el parser y
    ejercita la lógica pura (orientar zonas, sugerir+vetar por pared, roundtrip,
    comandos a firmware) sin abrir ventana. Devuelve 0 si OK."""
    from .protocol_top import parse_line_top
    from .simulator_top import SimulatorTop
    from .tof_layout import TofLayout

    if hasattr(sys.stdout, "reconfigure"):
        try:
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        except Exception:  # noqa: BLE001
            pass

    cfg = TofLayout()
    sim = SimulatorTop(rate_hz=20.0)
    last = None
    oriented_ok = True
    for _ in range(frames):
        last = parse_line_top(sim.next_line())
        if last.tof.zones:
            for idx, raw in enumerate(last.tof.zones):
                disp = cfg.oriented_cells(raw, idx)
                if len(disp) != len(raw):
                    oriented_ok = False
    assert last is not None

    rows = cfg.suggest_vetoed_rows()
    cfg.apply_row_veto(rows)
    back = TofLayout.from_dict(cfg.to_dict())
    cmds = cfg.to_firmware_commands()
    n_live = sum(1 for c in cmds if c.supported_now)
    n_pend = sum(1 for c in cmds if not c.supported_now)

    print(f"[selftest-tof] frames procesados : {frames}")
    print(f"[selftest-tof] zonas en el stream: {'SÍ' if last.tof.zones else 'no (pendiente firmware)'}")
    print(f"[selftest-tof] orientar zonas    : {'OK' if oriented_ok else 'FALLO'}")
    print(f"[selftest-tof] filas sugeridas a vetar (pared {cfg.wall.wall_height_mm:.0f}mm): {rows}")
    print(f"[selftest-tof] roundtrip config  : {'OK' if back.to_dict() == cfg.to_dict() else 'FALLO'}")
    print(f"[selftest-tof] comandos firmware : {n_live} listos + {n_pend} pendientes")
    if not oriented_ok or back.to_dict() != cfg.to_dict():
        print("[selftest-tof] FALLO")
        return 1
    print("[selftest-tof] OK")
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
    if args.readme:
        from .help_text import GUIDE
        if hasattr(sys.stdout, "reconfigure"):
            try:
                sys.stdout.reconfigure(encoding="utf-8", errors="replace")
            except Exception:  # noqa: BLE001
                pass
        print(GUIDE)
        return 0
    if args.list_ports:
        return list_ports()
    if args.selftest:
        if args.monitor:
            from . import gui_shell
            return gui_shell.smoke(args.selftest_frames)
        if args.field:
            from . import gui_field
            return gui_field.selftest(args.selftest_frames)
        if args.polar:
            from . import gui_polar
            return gui_polar.selftest(args.selftest_frames)
        if args.tof_360:
            from . import gui_tof_360
            return gui_tof_360.selftest(args.selftest_frames)
        if args.timeline:
            from . import gui_timeline
            return gui_timeline.selftest(args.selftest_frames)
        if args.cam_fusion:
            from . import gui_cam_fusion
            return gui_cam_fusion.selftest(args.selftest_frames)
        if args.tof_setup:
            return run_selftest_tof_setup(args.selftest_frames)
        if args.top_salud:
            return run_selftest_top_salud(args.selftest_frames)
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
        # La CONSOLA unificada es el default. Los flags de vista son atajos que la
        # abren arrancando en esa vista. --top/--arquero = vistas viejas standalone.
        top_views = {"field": "cancha", "polar": "polar", "tof_360": "tof360",
                     "top_salud": "salud", "cam_fusion": "cam", "timeline": "timeline",
                     "tof_setup": "tofcfg"}
        if _wants_shell(args):
            start_key = next((k for d, k in top_views.items() if getattr(args, d)), None)
            # Config POR ROBOT: resolver el N° de serie del puerto que va a usar la
            # fuente → archivo de config keyed por serial (R1 y R2 no se cruzan).
            # Sim/replay (sin port_spec) → None → archivo legacy único.
            config_path = None
            dev = getattr(source, "port_spec", None)
            if dev is not None:
                from .sources import autodetect_port, serial_for_port
                from .tof_layout import config_path_for_serial
                resolved = autodetect_port() if str(dev).lower() == "auto" else dev
                config_path = config_path_for_serial(serial_for_port(resolved))
            from . import gui_shell
            gui_shell.run_shell(source, recorder=recorder, start_key=start_key,
                                config_path=config_path)
        elif args.top:
            from . import gui_top
            gui_top.run_top(source, recorder=recorder)
        else:  # args.arquero
            from . import gui_gk
            gui_gk.run_gk(source, recorder=recorder)
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
