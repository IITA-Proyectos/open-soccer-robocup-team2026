#!/usr/bin/env python3
"""diag_capture.py — captura del Serial del `diag_down` + veredicto automatico.

Test masivo de sensores de luz placa DOWN en 3 lecturas:

    python diag_capture.py --port COM10 --label carpet --duration 2
    python diag_capture.py --port COM10 --label blanco --duration 2
    python diag_capture.py --port COM10 --label negro  --duration 2
    python diag_capture.py --verdict

Las 3 capturas se guardan en `.captures/<label>.json`. `--verdict` combina las
3 y muestra una tabla por sensor con OK / SOSPECHOSO / MUERTO.

⚠️ El Serial Monitor (Arduino IDE / VSCode) debe estar CERRADO — el puerto
serial es exclusivo. Si esta tomado, el script falla con "Access denied".
"""
import argparse
import json
import os
import re
import sys
import time

CAPTURE_DIR = ".captures"
# Match "S<idx>:<value><marker>" where marker is '*' (white) or ' ' (no white).
LINE_RE = re.compile(r'S(\d+):(\d+)([\*\s])')


def capture(port, label, duration):
    import serial
    os.makedirs(CAPTURE_DIR, exist_ok=True)

    samples = {}  # sensor_idx -> list of (raw_value, sees_white_bool)
    try:
        ser = serial.Serial(port, 115200, timeout=0.5)
    except serial.SerialException as e:
        print(f"!!! No pude abrir {port}: {e}")
        print("    Tip: cerra el Serial Monitor de Arduino IDE / VSCode.")
        sys.exit(2)

    print(f"Capturando '{label}' por {duration}s desde {port}...")
    start = time.time()
    lines_seen = 0
    while time.time() - start < duration:
        try:
            raw = ser.readline()
        except Exception:
            continue
        line = raw.decode('utf-8', errors='replace').strip()
        if not line.startswith('LUZ '):
            continue
        lines_seen += 1
        for m in LINE_RE.finditer(line):
            idx = int(m.group(1))
            val = int(m.group(2))
            white = m.group(3) == '*'
            samples.setdefault(idx, []).append((val, white))
    ser.close()

    if not samples:
        print(f"!!! NO se leyeron lineas 'LUZ ...' en {duration}s.")
        print("    Posibles causas: monitor abierto en otro programa, diag_down")
        print("    no esta flasheado, o la placa esta apagada.")
        sys.exit(1)

    stats = {}
    for idx, vals in sorted(samples.items()):
        nums = [v for v, _ in vals]
        whites = [w for _, w in vals]
        stats[idx] = {
            'n': len(nums),
            'min': min(nums),
            'max': max(nums),
            'avg': round(sum(nums) / len(nums), 1),
            'white_pct': round(100 * sum(whites) / len(whites), 0),
        }

    print(f"  {lines_seen} lineas LUZ leidas. {len(stats)} sensores detectados.")
    for idx in sorted(stats):
        s = stats[idx]
        wmark = "*" if s['white_pct'] >= 50 else " "
        print(f"    S{idx:2d}: avg={s['avg']:7.1f}  range=[{s['min']:4d}-{s['max']:4d}]"
              f"  white={int(s['white_pct']):3d}% {wmark}")

    path = os.path.join(CAPTURE_DIR, f"{label}.json")
    with open(path, 'w') as f:
        json.dump({'label': label, 'stats': stats}, f, indent=2)
    print(f"Guardado en {path}")


def verdict():
    """Combina carpet + blanco + negro y da veredicto por sensor."""
    captures = {}
    for label in ['carpet', 'blanco', 'negro']:
        path = os.path.join(CAPTURE_DIR, f"{label}.json")
        if not os.path.isfile(path):
            print(f"!!! Falta captura '{label}' ({path}). Capturala primero.")
            sys.exit(1)
        with open(path) as f:
            captures[label] = json.load(f)['stats']

    # JSON keys son strings — normalizo a int para ordenar.
    def get(c, idx):
        return c.get(str(idx), c.get(idx, {}))

    sensors = sorted({int(k) for c in captures.values() for k in c.keys()})

    print()
    print("=" * 80)
    print("VEREDICTO POR SENSOR (carpet vs blanco vs negro)")
    print("Criterio: rango = abs(blanco_avg - negro_avg)")
    print("  OK         : rango >= 300  (sensor responde bien a la luz)")
    print("  SOSPECHOSO : 50 <= rango < 300  (responde poco)")
    print("  MUERTO     : rango < 50  (no responde)")
    print("=" * 80)
    print(f"{'Sensor':>6}  {'carpet':>8}  {'blanco':>8}  {'negro':>8}  {'rango':>7}  Veredicto")
    print("-" * 80)

    ok = warn = dead = 0
    dead_list = []
    warn_list = []
    for idx in sensors:
        c = get(captures['carpet'], idx)
        b = get(captures['blanco'], idx)
        n = get(captures['negro'], idx)
        c_avg = c.get('avg', 0)
        b_avg = b.get('avg', 0)
        n_avg = n.get('avg', 0)
        rango = abs(b_avg - n_avg)

        if rango >= 300:
            verd = "OK"; ok += 1
        elif rango >= 50:
            verd = "SOSPECHOSO"; warn += 1; warn_list.append(idx)
        else:
            verd = "MUERTO"; dead += 1; dead_list.append(idx)

        print(f"  S{idx:2d}    {c_avg:8.1f}  {b_avg:8.1f}  {n_avg:8.1f}  {rango:7.1f}  {verd}")

    print("-" * 80)
    total = ok + warn + dead
    print(f"  Total: {total} sensores  |  OK: {ok}  Sospechosos: {warn}  Muertos: {dead}")
    if dead_list:
        print(f"  MUERTOS  : {dead_list}")
    if warn_list:
        print(f"  SOSPECHOSOS: {warn_list}")
    print("=" * 80)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description='Captura/veredicto de diag_down.')
    p.add_argument('--port', help='Puerto serial (ej. COM10)')
    p.add_argument('--label', help='Etiqueta: carpet | blanco | negro')
    p.add_argument('--duration', type=float, default=2.0, help='Segundos a capturar')
    p.add_argument('--verdict', action='store_true',
                   help='Combina las 3 capturas y da veredicto')
    args = p.parse_args()

    if args.verdict:
        verdict()
    elif args.port and args.label:
        capture(args.port, args.label, args.duration)
    else:
        p.print_help()
        sys.exit(1)
