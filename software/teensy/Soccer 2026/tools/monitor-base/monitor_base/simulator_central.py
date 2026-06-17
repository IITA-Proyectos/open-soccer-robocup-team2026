"""simulator_central.py — Telemetría CENTRAL sintética (sin robot).

Emite líneas JSON con el formato del contrato CENTRAL v1 (espejo de
protocol_central.py) para desarrollar/probar la vista del "cerebro" sin hardware.
Determinista (sin RNG): dos instancias dan exactamente las mismas líneas.

Modela un delantero (ATK) que oscila persiguiendo la pelota: el comando vx/vy/w y
el PWM por rueda laten; los enlaces TOP y DOWN están frescos casi siempre y de vez
en cuando "parpadean" a viejo (age sube, fresh=0) para ejercitar el semáforo de
salud; el árbitro alterna STOP/START; la pelota aparece y desaparece.
"""
from __future__ import annotations

import json
import math
from typing import Dict


class SimulatorCentral:
    def __init__(self, rate_hz: float = 20.0):
        self.dt_ms = int(round(1000.0 / rate_hz))
        self.seq = 0
        self.t_ms = 0

    def next_obj(self) -> Dict:
        s = self.seq
        t = self.t_ms / 1000.0

        # Pelota oscilando adelante (relativa al robot). Parpadea de vez en cuando.
        ball_x = int(round(140 * math.sin(s * 0.05)))
        ball_y = 220 + int(round(70 * math.cos(s * 0.07)))
        ball_vis = 1 if (s % 30) < 24 else 0

        # Heading OTOS girando despacio.
        hdg = 25.0 * math.sin(t * 0.2)

        # FSM: delantero; alterna entre BUSCAR (sin pelota) y ATACAR (con pelota).
        role = "ATK"
        state = "ATTACK" if ball_vis else "SEARCH"
        match = 1 if (s % 80) < 60 else 0          # árbitro: PLAY la mayor parte

        # Comando de movimiento (lo que pide la FSM): persigue la pelota.
        vx = int(round(ball_x * 1.2)) if ball_vis else 0
        vy = int(round(180 * math.cos(s * 0.06))) if (ball_vis and match) else 0
        w = int(round(hdg * 100))                  # cdeg/s
        spin = 50 if (state == "SEARCH" and match) else 0

        # PWM real por rueda (3 motores omni). Derivado tosco del comando para que
        # las barras del panel se muevan; signed -255..255.
        base = vy // 2 if match else 0
        pwm = [
            max(-255, min(255, base + vx // 3)),
            max(-255, min(255, base - vx // 3 + spin)),
            max(-255, min(255, -base + w // 20)),
        ]

        # Enlace TOP→CENTRAL: fresco casi siempre; parpadea a viejo en una ventana.
        top_stale = (s % 70) >= 66
        top = {
            "rxB": 11 * (s + 1),
            "fr": s + 1,
            "crc": s // 50,
            "rsy": s // 200,
            "badsz": 0,
            "fresh": 0 if top_stale else 1,
            "age": 320 if top_stale else (2 + (s % 5)),
        }

        # Enlace DOWN→CENTRAL: fresco casi siempre; la línea es válida cuando hay
        # línea presente (alterna). lost y crc son contadores SEPARADOS.
        down_stale = (s % 90) >= 86
        line_present = (s % 40) < 22
        down = {
            "rx": s + 1,
            "crc": s // 60,
            "lost": s // 120,
            "rsy": s // 180,
            "badsch": 0,
            "line_fresh": 0 if down_stale else 1,
            "valid": 1 if (line_present and not down_stale) else 0,
            "ev": 0x01 if line_present else 0,
            "age": 280 if down_stale else (3 + (s % 4)),
        }

        # OTOS: heading fresco casi siempre (parpadea a viejo en otra ventana).
        otos_fresh = 0 if (s % 100) >= 96 else 1

        obj = {
            "central": 1, "v": 1, "seq": s, "t_ms": self.t_ms,
            "fsm": {"role": role, "state": state, "match": match},
            "cmd": {"vx": vx, "vy": vy, "w": w, "spin": spin},
            "pwm": pwm,
            "top": top,
            "down": down,
            "otos": {"fresh": otos_fresh, "hdg": round(hdg, 2)},
            "snap": {
                "ball_vis": ball_vis, "ball_x": ball_x, "ball_y": ball_y,
                "goal_own_ang": round(180.0 - 10.0 * math.sin(s * 0.03), 2),
                "goal_own_dist": 1800 + int(round(200 * math.cos(s * 0.04))),
                "referee": 1 if match else 0,
                "flags": (0x08 if match else 0) | 0x10,   # MATCH_RUNNING | HEADING_VALID
                # Campos AMPLIADOS 2026-06-17. Simulamos un robot que se mueve por
                # el medio-arco propio (Y ~ 500-800 mm) y oscila lateralmente; pose
                # válida con confianza alta para ejercitar la vista de cancha.
                "my_x": int(round(910 + 350 * math.sin(s * 0.04))),
                "my_y": int(round(650 + 120 * math.cos(s * 0.06))),
                "my_hdg": round(hdg, 2),
                "hdg_valid": 1,
                "my_conf": 80 if (s % 200) < 180 else 0,   # parpadea a 0 (sin ancla)
                "ball_vx": int(round(ball_x * 0.5)) if ball_vis else 0,
                "ball_vy": int(round(-50 * math.sin(s * 0.06))) if ball_vis else 0,
                "goal_opp_vis": 1 if (s % 50) < 35 else 0,
                "goal_opp_ang": round(-3.0 + 12.0 * math.sin(s * 0.05), 2),
                "goal_opp_dist": 1700 + int(round(250 * math.sin(s * 0.03))),
            },
            "loop": {
                "max_us": 8000 + int(round(3000 * abs(math.sin(s * 0.02)))),
                "ema_us": 2200 + int(round(400 * math.sin(s * 0.05))),
            },
            # Eco de calibración (como el firmware con -DCENTRAL_EEPROM_CALIB) → ejercita el
            # panel "Calibrar CENTRAL" en --sim-central, sin robot. Valores típicos R2.
            "ccfg": {
                "min_pwm": [70, 70, 107], "eff": [100, 100, 115],
                "fwd_l": 120, "fwd_r": 118, "gkp": 1500, "gki": 10, "gkd": 300,
            },
        }
        self.seq += 1
        self.t_ms += self.dt_ms
        return obj

    def next_line(self) -> str:
        return json.dumps(self.next_obj(), separators=(",", ":"))
