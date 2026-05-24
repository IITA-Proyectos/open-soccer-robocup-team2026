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

DEFAULT_JSON = ("../../../../hardware/electronics/pcb_design/down_board/"
                "SCH_Roboliga_2026_Futbol_2026-04-12.json")
DEFAULT_PCB_JSON = ("../../../../hardware/electronics/pcb_design/down_board/"
                    "PCB_PCB_Roboliga_2026_Futbol_2026-04-12.json")

# CD4051 channel pin map (canal 0..7 → pin chip)
CH_TO_PIN = {0: '13', 1: '14', 2: '15', 3: '12', 4: '1', 5: '5', 6: '2', 7: '4'}

# EasyEDA PCB usa unidades de "10 mil" = 0.254 mm por unidad
PCB_PX_TO_MM = 10 * 0.0254


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


def extract_pcb_component_positions(pcb_json_path, designator_pattern=r'^F\d+$'):
    """Extrae posiciones físicas (x,y) en mm desde el centro del PCB,
    de todos los componentes cuyo designator (texto T~P~) matchea el patrón.

    Devuelve dict {designator -> (x_mm, y_mm)} con coordenadas relativas al
    centro del bounding box del PCB. +X = derecha del PCB, +Y = arriba (Y de
    EasyEDA flipeada para que crezca hacia arriba como en convención de robot).
    """
    with open(pcb_json_path, 'r', encoding='utf-8') as f:
        d = json.load(f)
    shapes = d['shape']
    bb = d['BBox']
    cx = bb['x'] + bb['width'] / 2
    cy = bb['y'] + bb['height'] / 2

    pat = re.compile(designator_pattern)
    positions = {}
    for s in shapes:
        if not s.startswith('LIB~'):
            continue
        parts = s.split('#@$')
        head_toks = parts[0].split('~')
        try:
            x = float(head_toks[1])
            y = float(head_toks[2])
        except (ValueError, IndexError):
            continue
        designator = None
        for p in parts[1:]:
            if p.startswith('TEXT~'):
                t = p.split('~')
                if len(t) >= 11 and t[1] == 'P':
                    designator = t[10]
                    break
        if designator and pat.match(designator):
            dx_mm = (x - cx) * PCB_PX_TO_MM
            dy_mm = -(y - cy) * PCB_PX_TO_MM  # flip y para convención robot
            positions[designator] = (round(dx_mm, 2), round(dy_mm, 2))
    return positions, (round(cx * PCB_PX_TO_MM, 2), round(cy * PCB_PX_TO_MM, 2))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--json', default=DEFAULT_JSON,
                    help=f'Path al schematic JSON (default: {DEFAULT_JSON})')
    ap.add_argument('--pcb-json', default=DEFAULT_PCB_JSON,
                    help=f'Path al PCB layout JSON (default: {DEFAULT_PCB_JSON})')
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

    # ============================================================
    # TABLA UNIFICADA: Sensor (Fx) -> (mux, ch) -> (pin Arduino) -> XY (mm)
    # ============================================================
    print()
    print("=" * 75)
    print("TABLA UNIFICADA — Sensores F1-F32 (fototransistores ALS-PT19)")
    print("=" * 75)
    print("Sistema de referencia: (0,0) = centro del PCB DOWN. +X = derecha")
    print("del PCB, +Y = arriba del PCB. Orientacion fisica respecto al")
    print("robot (cual eje es 'adelante') pendiente de validar con Enzo.")
    print()

    # 1. Posiciones físicas Fx del PCB
    positions, pcb_center_mm = extract_pcb_component_positions(args.pcb_json)
    bb_msg = f"Centro PCB en coords EasyEDA: ({pcb_center_mm[0]} mm, {pcb_center_mm[1]} mm)"
    print(bb_msg)
    print()

    # 2. Mapeo (mux, channel) -> net -> sensor Fx (via S? = F? por SCH)
    # Para cada U1-U4: para cada ch0-7: obtener net (esperado SX), inferir F = SX
    mux_to_arduino_sel = {  # extraido del schematic (sera regenerado abajo)
    }
    mux_com_to_arduino = {}

    rows = []  # (mux, ch, net, F, x_mm, y_mm, pin_arduino_for_COM)
    pin_to_arduino = {  # Pin del header del Teensy 4.0 -> pin Arduino logico
        # tabla basica para los pines relevantes (A0-A9, GPIO 0-23)
        13: 'A0', 12: 'A1', 11: 'SCL2', 10: 'SDA2',  # pines header 10-13
         9: 'SDA1', 8: 'SCL1', 7: 'TX5/A6', 6: 'RX5/A7',  # 6-9
         5: 'A8', 4: 'A9',     # 4-5
        14: '0/SCK', 15: '1/On/Off',
    }
    # Mapeo Teensy: para cada U-pin, ya tenemos U_pin -> net (E1..E12, O1..O4)
    # y net -> Arduino pin via U7 pin name (label de pin que es 'A0'..'A9' etc).
    # Reconstruimos esa LUT desde U7:
    u7_pins = extract_pins(shapes, 'U7', parent, add, rd)
    net_to_arduino_logical = {}  # net_name -> arduino logical pin name (str)
    for p in u7_pins:
        nm = net_of(p)
        if nm is None or p['name'] is None:
            continue
        # 'name' = label en silkscreen del header (A0..A9, etc.)
        net_to_arduino_logical[nm] = p['name']

    # Para cada mux Ui, identificar net del COM (pin 3) y de cada channel
    sensor_table = []  # rows: (F, mux, ch, net, com_arduino, x_mm, y_mm)
    for mux_idx, un in enumerate(['U1', 'U2', 'U3', 'U4'], 1):
        pins = extract_pins(shapes, un, parent, add, rd)
        pin_by_num = {p['num']: p for p in pins}
        com_net = net_of(pin_by_num['3']) if '3' in pin_by_num else None
        com_arduino = net_to_arduino_logical.get(com_net, '?')
        for ch in range(8):
            pn = CH_TO_PIN[ch]
            if pn not in pin_by_num:
                continue
            sensor_net = net_of(pin_by_num[pn])  # esperado: S1..S32
            # extraer numero del sensor del nombre del net (S1 -> 1)
            mn = re.match(r'^S(\d+)$', sensor_net or '')
            if not mn:
                continue
            sensor_num = int(mn.group(1))
            f_name = f'F{sensor_num}'
            xy = positions.get(f_name, (None, None))
            sensor_table.append({
                'F': f_name, 'S_net': sensor_net,
                'mux': un, 'ch': ch, 'com_arduino': com_arduino,
                'x_mm': xy[0], 'y_mm': xy[1],
            })

    # Imprimir ordenado por numero de sensor
    sensor_table.sort(key=lambda r: int(r['F'][1:]))
    print(f"{'Sensor':>6s} {'Net':>4s} {'Mux':>4s} {'Ch':>3s} "
          f"{'COM_Arduino':>11s} {'X(mm)':>8s} {'Y(mm)':>8s} {'R(mm)':>7s} {'theta(deg)':>10s}")
    print("-" * 75)
    for r in sensor_table:
        if r['x_mm'] is None:
            xs, ys, rr, th = '   ?  ', '   ?  ', '   ?  ', '    ?  '
        else:
            import math
            rr = math.hypot(r['x_mm'], r['y_mm'])
            th = math.degrees(math.atan2(r['y_mm'], r['x_mm']))
            xs = f"{r['x_mm']:>+7.2f}"
            ys = f"{r['y_mm']:>+7.2f}"
            rrs = f"{rr:>6.2f}"
            ths = f"{th:>+9.1f}"
            print(f"{r['F']:>6s} {r['S_net']:>4s} {r['mux']:>4s} "
                  f"{r['ch']:>3d} {r['com_arduino']:>11s} "
                  f"{xs} {ys} {rrs} {ths}")


if __name__ == '__main__':
    main()
