"""registrar_placa.py — Registra el Teensy USB CONECTADO como un robot/placa.

Conectá UNA sola placa y corré (desde tools/monitor-base/):

    python registrar_placa.py --robot 1 --board top
    python registrar_placa.py --robot 1 --board down
    python registrar_placa.py --robot 2 --board top --name "Robot 2 · TOP (cámara nueva)"

Lee el N° de serie USB del Teensy (NO abre el puerto → no pelea con la app si está
abierta) y lo guarda en monitor_base/robot_registry.json. La app usa ese registro
para mostrar la identidad del robot y NO cruzar config entre R1 y R2.

Listar lo registrado:   python registrar_placa.py --list
"""
from __future__ import annotations

import argparse
import json
import sys

from monitor_base.robot_registry import identify, load_registry, registry_path
from monitor_base.sources import list_serial_ports

if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:  # noqa: BLE001
        pass


def _board_label(board: str) -> str:
    return {"top": "TOP", "down": "BASE", "central": "CENTRAL"}.get(board, board.upper())


def main() -> int:
    ap = argparse.ArgumentParser(description="Registrar el Teensy conectado como robot/placa.")
    ap.add_argument("--robot", type=int, help="número de robot (1, 2, …)")
    ap.add_argument("--board", choices=("top", "down", "central"),
                    help="placa: top (superior), down (base) o central (cerebro)")
    ap.add_argument("--name", default=None, help="nombre lindo (default 'Robot N · TOP/BASE')")
    ap.add_argument("--list", action="store_true", help="listar lo registrado y salir")
    args = ap.parse_args()

    if args.list:
        reg = load_registry()
        if not reg:
            print("registro vacío (todavía no registraste ninguna placa).")
            return 0
        print(f"registro ({registry_path()}):")
        for ser, e in reg.items():
            print(f"  {ser:<14} robot={e.get('robot')} board={e.get('board')} · {e.get('name')}")
        return 0

    if args.robot is None or args.board is None:
        print("ERROR: pasá --robot N y --board top|down (o --list). Ej: --robot 1 --board top")
        return 2

    teensy = [p for p in list_serial_ports() if p["is_teensy"]]
    allp = list_serial_ports()
    if len(teensy) != 1:
        print(f"ERROR: esperaba 1 Teensy conectado, hay {len(teensy)}.")
        print("Puertos:", [(p["device"], p.get("serial_number")) for p in allp])
        print("Conectá SOLO la placa que querés registrar y reintentá.")
        return 1
    ser = teensy[0].get("serial_number")
    dev = teensy[0]["device"]
    if not ser:
        print(f"ERROR: Teensy en {dev} sin N° de serie USB legible → no puedo registrarlo.")
        return 1

    name = args.name or f"Robot {args.robot} · {_board_label(args.board)}"
    reg = load_registry()
    prev = reg.get(str(ser))
    reg[str(ser)] = {"robot": args.robot, "board": args.board, "name": name}
    with open(registry_path(), "w", encoding="utf-8") as f:
        json.dump(reg, f, ensure_ascii=False, indent=2)

    print(f"DETECTADO : {dev}  serial={ser!r}")
    if prev and prev != reg[str(ser)]:
        print(f"(reemplaza registro previo: {prev})")
    print(f"GUARDADO  : {name}  (robot={args.robot}, board={args.board})")
    print(f"           en {registry_path()}")
    rid = identify(ser, load_registry())
    print(f"VERIFICADO: la app lo verá como '{rid.name}'  (known={rid.known})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
