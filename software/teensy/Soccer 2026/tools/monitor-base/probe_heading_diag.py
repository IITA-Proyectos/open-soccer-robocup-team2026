"""Diagnóstico de HEADING del TOP: vuelca TODOS los campos de rumbo del contrato para
ver cuál es el crudo del BNO, cuál el corregido por el offset de boot, y si alguno queda
en ~0 cuando el robot mira al arco rival. SOLO LECTURA (STREAM ON + PING).

Campos:
  imu.heading_deg (fusionado, valid) | imu.left_deg (lok) | imu.right_deg (rok)
  imu.sentinel_deg (sok) | imu.disagreement_deg
  snap.my_heading_deg (snap.valid + flag HEADING_VALID 0x10)
  base.pose_heading_deg (base.pose_fresh)

Uso (desde tools/monitor-base/):  python probe_heading_diag.py [--port COM15] [--secs 15]
"""
import argparse
import statistics
import time

import serial
from monitor_base.protocol_top import parse_line_top
from monitor_base.sources import autodetect_port

ap = argparse.ArgumentParser()
ap.add_argument("--port", default=None)
ap.add_argument("--secs", type=float, default=15.0)
a = ap.parse_args()
port = a.port or autodetect_port() or "COM15"
print(f"abriendo {port} @115200, leyendo {a.secs:.0f}s. Robot QUIETO en su orientación conocida.\n")
s = serial.Serial(port, 115200, timeout=0.4)
s.reset_input_buffer()
s.write(b"STREAM ON\n"); s.flush()

# acc[campo] = lista de valores cuando el ok/valid correspondiente es True
acc = {k: [] for k in ("hdg", "left", "right", "sent", "disag", "snap", "pose")}
flags_valid = 0   # frames con flag HEADING_VALID (0x10) en snap
n = 0
t0 = time.time(); last_ping = t0; last_print = t0
try:
    while time.time() - t0 < a.secs:
        if time.time() - last_ping > 1.0:
            s.write(b"PING\n"); s.flush(); last_ping = time.time()
        raw = s.readline()
        if not raw:
            continue
        ln = raw.decode("ascii", "replace").strip()
        if not ln.startswith("{"):
            continue
        try:
            fr = parse_line_top(ln)
        except Exception:
            continue
        n += 1
        im = fr.imu
        if im.heading_valid: acc["hdg"].append(im.heading_deg)
        if im.left_ok:       acc["left"].append(im.left_deg)
        if im.right_ok:      acc["right"].append(im.right_deg)
        if im.sentinel_ok:   acc["sent"].append(im.sentinel_deg)
        acc["disag"].append(im.disagreement_deg)
        if fr.snap.valid:    acc["snap"].append(fr.snap.my_heading_deg)
        if fr.snap.flags & 0x10: flags_valid += 1
        if fr.base.pose_fresh: acc["pose"].append(fr.base.pose_heading_deg)
        # eco en vivo ~cada 1s: los rumbos crudos + el del snap
        if time.time() - last_print > 1.0:
            last_print = time.time()
            print(f"  hdg={im.heading_deg:7.1f}(v{int(im.heading_valid)}) "
                  f"left={im.left_deg:7.1f}(ok{int(im.left_ok)}) "
                  f"right={im.right_deg:7.1f}(ok{int(im.right_ok)}) "
                  f"sent={im.sentinel_deg:6.1f}(ok{int(im.sentinel_ok)}) "
                  f"snap={fr.snap.my_heading_deg:7.1f}(HV{int(bool(fr.snap.flags & 0x10))})")
finally:
    try:
        s.write(b"STREAM OFF\n"); s.flush()
    except Exception:
        pass
    s.close()

NOM = {
    "hdg": "imu.heading_deg (FUSIONADO)", "left": "imu.left_deg  (BNO L)",
    "right": "imu.right_deg (BNO R)", "sent": "imu.sentinel_deg (centinela)",
    "disag": "imu.disagreement_deg", "snap": "snap.my_heading_deg (al WorldSnapshot)",
    "pose": "base.pose_heading_deg (OTOS/DOWN)",
}
print(f"\n=== RESUMEN ({n} frames, HEADING_VALID en snap: {flags_valid}/{n}) ===")
for k in ("hdg", "left", "right", "sent", "disag", "snap", "pose"):
    v = acc[k]
    if v:
        m = statistics.mean(v); sd = statistics.pstdev(v) if len(v) > 1 else 0.0
        print(f"  {NOM[k]:42s}: {m:8.1f}° ± {sd:5.1f}  (n{len(v)}, min {min(v):.1f} max {max(v):.1f})")
    else:
        print(f"  {NOM[k]:42s}: sin lecturas válidas")
