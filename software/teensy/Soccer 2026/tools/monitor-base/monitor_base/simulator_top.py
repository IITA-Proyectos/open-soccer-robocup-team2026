"""simulator_top.py — Telemetría TOP sintética (sin robot). FASE 2.

Emite líneas JSON con el formato de telemetry_top.cpp para desarrollar/probar la
vista TOP de la app sin hardware. Determinista (sin RNG).
"""
from __future__ import annotations

import json
import math
from typing import Dict


class SimulatorTop:
    def __init__(self, num_tof: int = 4, rate_hz: float = 20.0):
        self.num_tof = num_tof
        self.dt_ms = int(round(1000.0 / rate_hz))
        self.seq = 0
        self.t_ms = 0

    def next_obj(self) -> Dict:
        s = self.seq
        t = self.t_ms / 1000.0
        # Pelota oscilando adelante.
        ball_x = int(round(150 * math.sin(s * 0.05)))
        ball_y = 250 + int(round(80 * math.cos(s * 0.07)))
        ball_vis = 1 if (s % 30) < 25 else 0          # parpadea de vez en cuando
        ball_vx = int(round(150 * 0.05 * math.cos(s * 0.05) * 20))
        # IMU rotando despacio.
        hdg = 30.0 * math.sin(t * 0.2)
        # ToF variando; el índice 2 a veces sin lectura.
        d = []
        for i in range(self.num_tof):
            base = 200 + 150 * i + int(round(100 * math.sin(s * 0.1 + i)))
            if i == 2 and (s % 20) < 4:
                d.append(65535)
            else:
                d.append(max(40, base))
        hc = 300 + int(round(50 * math.sin(s * 0.08)))
        tof_real = [x for x in d if x != 65535] + [hc]
        tmin = min(tof_real) if tof_real else 65535
        # Arco amarillo adelante, azul atrás.
        gy_ang = int(round(100 * (5.0 * math.sin(s * 0.03))))
        gb_ang = int(round(100 * (180.0 - 5.0 * math.sin(s * 0.03))))
        line_present = 1 if (s % 40) < 20 else 0   # v2: la línea aparece/desaparece
        obj = {
            "v": 2, "seq": s, "t_ms": self.t_ms,
            "cam": {
                "fok": 1, "bok": 1, "bvis": ball_vis, "bx": ball_x, "by": ball_y,
                "bconf": 80 if ball_vis else 0, "bvx": ball_vx, "bvy": -20,
                "gy_vis": 1, "gy_ang": gy_ang, "gy_dist": 1200,
                "gb_vis": 1, "gb_ang": gb_ang, "gb_dist": 2400,
                "crc": 0, "resync": 0,
            },
            # v2: per-cámara — la frontal ve pelota + arco amarillo; la trasera el azul.
            "camf": {
                "bvis": ball_vis, "bx": ball_x, "by": ball_y,
                "gy_vis": 1, "gy_ang": gy_ang, "gy_dist": 1200,
                "gb_vis": 0, "gb_ang": 0, "gb_dist": 0,
            },
            "camb": {
                "bvis": 0, "bx": 0, "by": 0,
                "gy_vis": 0, "gy_ang": 0, "gy_dist": 0,
                "gb_vis": 1, "gb_ang": gb_ang, "gb_dist": 2400,
            },
            "imu": {
                "hdg": round(hdg, 2), "left": round(hdg - 0.3, 2),
                "right": round(hdg + 0.3, 2), "disagree": 0.6,
                "lok": 1, "rok": 1, "valid": 1,
            },
            "tof": {"n": self.num_tof, "d": d, "hc": hc, "min": tmin},
            "snap": {
                "valid": 1, "x": int(round(50 * math.sin(t * 0.3))),
                "y": int(round(30 * t)) % 600, "hdg_cd": int(round(hdg * 100)),
                "conf": 75,
                "bx": ball_x, "by": ball_y, "bvis": ball_vis,
                "bconf": 80 if ball_vis else 0, "bvx": ball_vx, "bvy": -20,
                "opp_ang": gy_ang, "opp_dist": 1200, "opp_vis": 1,
                "own_vis": 0, "own_ang": 0, "own_dist": 0,
                "obst": tmin, "ref": 1, "flags": 0x18,
            },
            # v2: lo que llega de la base (DOWN) — OTOS + línea + vector de escape.
            "base": {
                "pfresh": 1, "px": int(round(50 * math.sin(t * 0.3))),
                "py": int(round(30 * t)) % 600, "phdg_cd": int(round(hdg * 100)),
                "pconf": 70,
                "vfresh": 1, "vx": 100, "vy": -10, "omega": int(round(hdg * 5)), "slip": 0,
            },
            "line": {
                "fresh": 1, "schema": 2, "valid": 1,
                "angle_cd": 4500 if line_present else -32768,
                "escape_cd": -13500 if line_present else -32768,
                "pen_mm": 60 if line_present else 65535,
                "cross_mm": -32768,
                "present": line_present, "sensors": 7 if line_present else 0,
                "events": 0x01 if line_present else 0, "quality": 80, "age_ms": 2,
            },
            "diag": {"frames_sent": 1000 + s},
        }
        self.seq += 1
        self.t_ms += self.dt_ms
        return obj

    def next_line(self) -> str:
        return json.dumps(self.next_obj(), separators=(",", ":"))
