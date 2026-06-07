#!/usr/bin/env python3
"""
udp_listener.py — Receptor UDP standalone para el ESP32 Telemetry Bridge.

Escucha datagramas UDP (uno por línea JSON Lines, sin '\\n' final), los imprime
a stdout y opcionalmente los guarda a un archivo.

Uso típico:
    python udp_listener.py
    python udp_listener.py --port 8765 --log session.jsonl
    python udp_listener.py --filter '"src":"down"'

Sin dependencias externas. Funciona con Python 3.8+.
"""

from __future__ import annotations
import argparse
import socket
import signal
import sys
import time
from pathlib import Path


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    p.add_argument(
        "--host",
        default="0.0.0.0",
        help="interfaz local donde escuchar (default: todas)",
    )
    p.add_argument(
        "--port",
        type=int,
        default=8765,
        help="puerto UDP de escucha (default 8765, coincide con UDP_OUT_PORT del ESP32)",
    )
    p.add_argument(
        "--log",
        type=Path,
        default=None,
        help="archivo donde grabar líneas recibidas (append). Default: solo stdout.",
    )
    p.add_argument(
        "--filter",
        default=None,
        help="solo imprimir/grabar líneas que CONTIENEN este texto (filtro simple).",
    )
    p.add_argument(
        "--stats-secs",
        type=int,
        default=10,
        help="cada cuántos segundos imprimir un resumen de tasa (default 10; 0 = off).",
    )
    return p.parse_args()


def main() -> int:
    args = parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    try:
        sock.bind((args.host, args.port))
    except OSError as e:
        print(f"[error] no pude bindear {args.host}:{args.port} — {e}", file=sys.stderr)
        return 2

    log_fp = None
    if args.log is not None:
        args.log.parent.mkdir(parents=True, exist_ok=True)
        log_fp = args.log.open("ab")  # append binario para conservar bytes exactos
        print(f"[log] grabando a {args.log}", file=sys.stderr)

    print(
        f"[ready] escuchando UDP en {args.host}:{args.port}"
        + (f" — filtro={args.filter!r}" if args.filter else "")
        + " — Ctrl-C para cortar.",
        file=sys.stderr,
    )

    running = True

    def stop(_signum, _frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)

    packets = 0
    bytes_in = 0
    last_stats_ts = time.monotonic()

    while running:
        try:
            sock.settimeout(0.5)
            try:
                data, src = sock.recvfrom(8192)
            except socket.timeout:
                # chequear stats periódicas
                if args.stats_secs > 0 and time.monotonic() - last_stats_ts >= args.stats_secs:
                    print(
                        f"[stats] {packets} paquetes, {bytes_in} bytes recibidos",
                        file=sys.stderr,
                    )
                    last_stats_ts = time.monotonic()
                continue

            packets += 1
            bytes_in += len(data)

            # El ESP32 envía la línea sin '\n' final (lo recortamos antes). El log
            # estándar conviene con '\n' para que el archivo sea JSON Lines.
            line = data.rstrip(b"\r\n")
            if args.filter is not None and args.filter.encode() not in line:
                continue

            sys.stdout.buffer.write(line + b"\n")
            sys.stdout.buffer.flush()
            if log_fp is not None:
                log_fp.write(line + b"\n")

        except KeyboardInterrupt:
            break
        except OSError as e:
            print(f"[error] socket: {e}", file=sys.stderr)
            time.sleep(0.5)

    if log_fp is not None:
        log_fp.close()
    sock.close()
    print(
        f"[done] {packets} paquetes, {bytes_in} bytes en total.",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
