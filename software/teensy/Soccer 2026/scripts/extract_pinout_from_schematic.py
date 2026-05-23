#!/usr/bin/env python3
"""extract_pinout_from_schematic.py — extrae el pinout completo del Teensy 4.0
y los CD4051 de la placa DOWN, parseando el schematic EasyEDA en formato JSON.

Uso:
    python extract_pinout_from_schematic.py
    python extract_pinout_from_schematic.py --json <path al SCH_*.json>

Output: tablas en stdout con Teensy U7 (todos los pines del header → net del
schematic), CD4051 U1–U4 (pines clave + canales → S? sensores), OTOS U5/U6.

Algoritmo:
  1. Parsea el JSON (shapes EasyEDA).
  2. Union-find sobre las wires (W~) y junctions (J~) para reconstruir
     conectividad eléctrica.
  3. Identifica netlabels (F~) por coordenadas y los asocia al grupo
     eléctrico correspondiente.
  4. Extrae pines de cada componente Uxx (LIB con `comment~Uxx~`) con sus
     coords absolutas + label + número.
  5. Cruza: para cada pin del componente, encuentra el nombre de net
     asociado al mismo grupo eléctrico.

Ver `hardware/electronics/2026-05-19-pinout-down-extraido-schematic.md` para
los resultados completos formateados.
"""
import argparse
import json
import re
import sys
from collections import defaultdict

DEFAULT_JSON = ("../../../hardware/electronics/pcb_design/down_board/"
                "SCH_Roboliga_2026_Futbol_2026-04-12.json")

# CD4051 channel pin map (canal 0..7 → pin chip)
CH_TO_PIN = {0: '13', 1: '14', 2: '15', 3: '12', 4: '1', 5: '5', 6: '2', 7: '4'}


def load_shapes(json_path):
    with open(json_path, 'r', encoding='utf-8') as f:
        d = json.load(f)
    return d['schematics'][0]['dataStr']['shape']


def build_graph(shapes):
    """Union-find sobre wires + junctions; devuelve (parent_dict, groups_by_root)."""
    parent = {}

    def find(c):
        while parent.get(c, c) != c:
            parent[c] = parent.get(parent[c], parent[c])
            c = parent[c]
        return c

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb

    def add(c):
        if c not in parent:
            parent[c] = c

    def rd(v):
        return round(v, 0)

    for sh in shapes:
        if sh.startswith('W~'):
            m = re.match(r'W~([\d\.\-\s]+)~', sh)
            if not m:
                continue
            nums = [float(x) for x in m.group(1).split()]
            pts = [(rd(nums[i]), rd(nums[i+1])) for i in range(0, len(nums)-1, 2)]
            for p in pts:
                add(p)
            for i in range(len(pts) - 1):
                union(pts[i], pts[i+1])
        elif sh.startswith('J~'):
            t = sh.split('~')
            try:
                add((rd(float(t[1])), rd(float(t[2]))))
            except (IndexError, ValueError):
                pass

    return parent, find, add, rd


def extract_nets(shapes, parent, find, add, rd):
    """Devuelve dict: net_name -> list de coords (absolutas) donde está el F~ label."""
    nets = defaultdict(list)
    for sh in shapes:
        if not sh.startswith('F~'):
            continue
        m = re.search(r'\^\^(-?\d+\.?\d*)~(-?\d+\.?\d*)\^\^([^~]+)~', sh)
        if m:
            c = (rd(float(m.group(1))), rd(float(m.group(2))))
            nets[m.group(3)].append(c)
            add(c)
    return nets


def extract_pins(shapes, prefix, parent, add, rd):
    """Pines de un componente Uxx. Cada pin: {'x', 'y', 'num', 'name'}."""
    for sh in shapes:
        if not sh.startswith('LIB~') or f'comment~{prefix}~' not in sh:
            continue
        pins = []
        for part in sh.split('#@$'):
            if not part.startswith('P~'):
                continue
            subs = part.split('^^')
            h = subs[0].split('~')
            try:
                x, y = float(h[4]), float(h[5])
            except (IndexError, ValueError):
                continue
            name, num = None, None
            for s in subs[1:]:
                st = s.split('~')
                if len(st) >= 8 and st[5] in ('start', 'end'):
                    t = st[4]
                    if t and t.isdigit():
                        num = t
                    elif t and not t.isdigit():
                        name = t
            pins.append({'x': rd(x), 'y': rd(y), 'num': num, 'name': name})
            add((rd(x), rd(y)))
        return pins
    return []


def net_of_pin(pin, parent, find, nets):
    """Encuentra el nombre de la net conectada a un pin (via union-find)."""
    groups = defaultdict(list)
    for c in parent:
        groups[find(c)].append(c)
    pc = (pin['x'], pin['y'])
    if pc not in parent:
        return None
    for c in groups[find(pc)]:
        for nname, coords in nets.items():
            if c in coords:
                return nname
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--json', default=DEFAULT_JSON,
                    help=f'Path al schematic JSON (default: {DEFAULT_JSON})')
    args = ap.parse_args()

    shapes = load_shapes(args.json)
    parent, find, add, rd = build_graph(shapes)
    nets = extract_nets(shapes, parent, find, add, rd)

    def net_of(pin):
        return net_of_pin(pin, parent, find, nets)

    print("=" * 75)
    print("TEENSY U7 — pin del header → net del schematic")
    print("=" * 75)
    u7_pins = extract_pins(shapes, 'U7', parent, add, rd)
    for p in sorted(u7_pins, key=lambda x: int(x['num']) if x['num'] else 0):
        print(f"  Pin#{p['num']:>3s} (label={(p['name'] or '?'):>10s})  →  net={net_of(p)}")
    print()

    print("=" * 75)
    print("CD4051 U1–U4 — Selectores A/B/C, COM, INH + Canales → Sensores")
    print("=" * 75)
    for un in ['U1', 'U2', 'U3', 'U4']:
        pins = extract_pins(shapes, un, parent, add, rd)
        pin_by_num = {p['num']: p for p in pins}
        print(f"\n--- {un} ---")
        for pn in ['11', '10', '9', '3', '6']:
            if pn in pin_by_num:
                p = pin_by_num[pn]
                print(f"  pin{pn:>3s} ({(p['name'] or '?'):>12s})  →  net={net_of(p)}")
        print(f"  channels:")
        for ch in range(8):
            pn = CH_TO_PIN[ch]
            if pn in pin_by_num:
                print(f"    ch{ch}  (pin{pn})  →  net={net_of(pin_by_num[pn])}")

    print()
    print("=" * 75)
    print("OTOS U5, U6 — pines I²C")
    print("=" * 75)
    for un in ['U5', 'U6']:
        print(f"\n--- {un} ---")
        for p in extract_pins(shapes, un, parent, add, rd):
            print(f"  pin{p['num']:>3s} ({(p['name'] or '?'):>10s})  →  net={net_of(p)}")


if __name__ == '__main__':
    main()
