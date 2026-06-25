"""Lee N segundos de telemetría del TOP y PROMEDIA las 16 zonas crudas de cada ToF (4x4).

Las zonas del ToF son ruidosas frame a frame; con el robot en una posición CONOCIDA,
promediar muchas muestras saca la lectura ESTABLE de cada zona, para compararla con la
geometría teórica (distancia a las paredes) y deducir la fórmula. SOLO LECTURA: manda
STREAM ON + PING (como probe_top_serial), nada de config.

Uso (desde tools/monitor-base/):  python tof_zonas_promedio.py [--port COM15] [--secs 75]
"""
import argparse
import statistics
import time

import serial
from monitor_base.protocol_top import parse_line_top
from monitor_base.sources import autodetect_port

ap = argparse.ArgumentParser()
ap.add_argument("--port", default=None, help="COMx (autodetecta si se omite)")
ap.add_argument("--secs", type=float, default=75.0)
a = ap.parse_args()
port = a.port or autodetect_port() or "COM15"
print(f"abriendo {port} @115200, leyendo {a.secs:.0f}s (robot QUIETO en la posición conocida)...")
s = serial.Serial(port, 115200, timeout=0.4)
s.reset_input_buffer()
s.write(b"STREAM ON\n"); s.flush()

acc = [[[] for _ in range(16)] for _ in range(4)]   # acc[sensor][zona] = muestras
# Los DOS BNO por separado + el fusionado + el centinela (para ver cuál da el heading bueno):
#   hdg = fusionado (i.heading_valid), left/right = cada BNO (i.left_ok/right_ok), sent = centinela.
imu_acc = {"hdg": [], "left": [], "right": [], "sent": []}
nframes = 0
t0 = time.time(); last_ping = t0
try:
    while time.time() - t0 < a.secs:
        if time.time() - last_ping > 1.0:
            s.write(b"PING\n"); s.flush(); last_ping = time.time()
        raw = s.readline()
        if not raw:
            continue
        line = raw.decode("ascii", "replace").strip()
        if not line.startswith("{"):
            continue
        try:
            fr = parse_line_top(line)
        except Exception:
            continue
        im = getattr(fr, "imu", None)
        if im is not None:
            if getattr(im, "heading_valid", False): imu_acc["hdg"].append(float(im.heading_deg))
            if getattr(im, "left_ok", False):       imu_acc["left"].append(float(im.left_deg))
            if getattr(im, "right_ok", False):       imu_acc["right"].append(float(im.right_deg))
            if getattr(im, "sentinel_ok", False):    imu_acc["sent"].append(float(im.sentinel_deg))
        if fr.tof and fr.tof.zones:
            nframes += 1
            for i, zs in enumerate(fr.tof.zones[:4]):
                for z, v in enumerate(zs[:16]):
                    if v is not None:
                        acc[i][z].append(int(v))
finally:
    try:
        s.write(b"STREAM OFF\n"); s.flush()
    except Exception:
        pass
    s.close()

print(f"{nframes} frames con zonas.")
NOM = {"hdg": "FUSIONADO", "left": "BNO-LEFT ", "right": "BNO-RIGHT", "sent": "CENTINELA"}
for k in ("hdg", "left", "right", "sent"):
    v = imu_acc[k]
    if v:
        m = statistics.mean(v); sd = statistics.pstdev(v) if len(v) > 1 else 0.0
        print(f"  {NOM[k]}: {m:7.1f}° ± {sd:5.1f}  (n{len(v)}, min {min(v):.1f} max {max(v):.1f})")
    else:
        print(f"  {NOM[k]}: sin lecturas válidas")
print()
LAB = ["FRONT", "BACK", "RIGHT", "LEFT"]
for i in range(4):
    print(f"=== ToF{i} {LAB[i]}  (zona CRUDA fila*4+col: media±std nMuestras) ===")
    for row in range(4):
        cells = []
        for col in range(4):
            z = row * 4 + col
            v = acc[i][z]
            if len(v) >= 3:
                cells.append(f"{statistics.mean(v):5.0f}±{statistics.pstdev(v):3.0f} n{len(v):<4}")
            elif v:
                cells.append(f"{statistics.mean(v):5.0f}  (n{len(v)})  ")
            else:
                cells.append("   --          ")
        print("  ".join(cells))
    print()
