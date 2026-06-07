#!/usr/bin/env python3
"""
udp_send.py — Manda una línea por UDP al ESP32 Telemetry Bridge.

Útil para probar el path comando → ESP32 → Teensy (calibración remota).

Uso:
    python udp_send.py 192.168.1.42 8764 'CAL CARPET'
    python udp_send.py 192.168.1.42 8764 'RATE 50'

Sin dependencias externas. Python 3.8+.
"""

from __future__ import annotations
import socket
import sys


def main() -> int:
    if len(sys.argv) < 4:
        print("uso: udp_send.py <ip> <port> <comando>", file=sys.stderr)
        return 2
    ip = sys.argv[1]
    port = int(sys.argv[2])
    cmd = " ".join(sys.argv[3:])
    if not cmd.endswith("\n"):
        cmd += "\n"
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(cmd.encode("utf-8"), (ip, port))
    print(f"[ok] enviado a {ip}:{port}: {cmd!r}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
