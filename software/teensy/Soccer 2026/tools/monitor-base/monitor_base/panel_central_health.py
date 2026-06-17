"""panel_central_health.py — Panel "SALUD CENTRAL" (embebible en el shell).

Salud de los ENLACES que la placa CENTRAL RECIBE (no de sus sensores: la CENTRAL
no tiene sensores propios — es el cerebro). Semáforos verde/rojo + contadores:

  • TOP→CENTRAL : f.top.fresh (verde/rojo) · fr/crc/rsy/badsz · age ms.
  • DOWN→CENTRAL: f.down.line_fresh + valid · rx/crc/lost/rsy · age ms.
      OJO: `lost` (frames perdidos por SEQ) y `crc` (frames corruptos) se muestran
      SEPARADOS — NO se suman. Son fallas de naturaleza distinta.
  • Yaw OTOS    : dial + número (f.otos.hdg), griseado si f.otos.fresh=False.
  • loop_us     : max_us / ema_us del ciclo del cerebro, con ALERTA si max supera
      un umbral (el loop largo = el robot reacciona tarde).
  • Eco snapshot: pelota visible + arco propio (polar) + árbitro + flags.

AVISO HONESTO: la pose my_x/my_y del robot NO viaja en este contrato (= GAP del
TOP). Se rotula explícito para que nadie lea "0,0" como una posición real.

Consume CentralFrame (protocol_central.py, schema v1). NO manda comandos
(sends_commands=False). Reusa STATUS_COLOR/STATUS_LABEL de health.py para verse
igual que el panel de salud del TOP. DEFENSIVO: getattr con default + try/except,
nunca tumba Tk con un frame corto.
"""
from __future__ import annotations

import math
import tkinter as tk
from tkinter import ttk

from .health import STATUS_COLOR, STATUS_LABEL, Status
from .panel import Panel
from .protocol_central import CentralFrame, decode_snap_flags
from .shell_theme import BG, FG, MUTED, PANEL
from .tooltip import attach_tooltip
from .tooltips_text import tip

# Umbral de alerta del loop: por encima, el ciclo del cerebro está largo (reacción
# tardía). 20 ms = la CENTRAL apunta a >=50 Hz; si el peor caso lo supera, ámbar.
LOOP_WARN_US = 20_000
LOOP_BAD_US = 40_000

DIAL_PX = 150        # tamaño del dial del yaw OTOS
REFEREE = {0: "STOP", 1: "START", 2: "HALFTIME", 3: "RESET"}


class CentralHealthPanel(Panel):
    title = "Salud CENTRAL"
    key = "central_salud"
    icon = "✚"
    board = "central"
    sends_commands = False

    # ── Layout ────────────────────────────────────────────────────────────────
    def build(self, parent: tk.Frame) -> None:
        self.header = tk.Label(parent, text="esperando datos…", anchor="w",
                               font=("Consolas", 11, "bold"), bg=BG, fg=FG,
                               padx=8, pady=6)
        self.header.pack(fill="x")

        body = ttk.Frame(parent, padding=8)
        body.pack(fill="both", expand=True)

        # Fila 0: semáforos de los enlaces TOP y DOWN.
        ttk.Label(body, text="ENLACES QUE RECIBE LA CENTRAL", style="Muted.TLabel"
                  ).grid(row=0, column=0, columnspan=2, sticky="w")
        links = ttk.Frame(body)
        links.grid(row=1, column=0, columnspan=2, sticky="w", pady=(2, 10))
        self._top_tile = self._make_link_tile(links, 0, "TOP → CENTRAL")
        self._down_tile = self._make_link_tile(links, 1, "DOWN → CENTRAL")

        # Fila 2: dial del yaw OTOS + loop_us, lado a lado.
        ttk.Label(body, text="YAW OTOS  ·  CARGA DEL LOOP", style="Muted.TLabel"
                  ).grid(row=2, column=0, columnspan=2, sticky="w")
        midrow = ttk.Frame(body)
        midrow.grid(row=3, column=0, columnspan=2, sticky="w", pady=(2, 10))

        dwrap = ttk.Frame(midrow); dwrap.grid(row=0, column=0, sticky="n")
        self.dial = tk.Canvas(dwrap, width=DIAL_PX, height=DIAL_PX, bg=PANEL,
                              highlightthickness=0)
        self.dial.pack()
        attach_tooltip(self.dial, tip("central.otos"))
        self._draw_dial_static()
        self.otos_lbl = tk.Label(dwrap, text="hdg —", font=("Consolas", 11, "bold"),
                                 bg=BG, fg=FG)
        self.otos_lbl.pack(pady=(4, 0))

        lwrap = ttk.Frame(midrow); lwrap.grid(row=0, column=1, sticky="n", padx=(16, 0))
        self.loop_lbl = tk.Label(lwrap, text="loop —", anchor="w", justify="left",
                                 font=("Consolas", 11, "bold"), bg=PANEL, fg=FG,
                                 padx=12, pady=10, width=26)
        self.loop_lbl.pack()
        attach_tooltip(self.loop_lbl, tip("central.loop"))

        # Fila 4: eco del snapshot (2026-06-17: AMPLIADO con pose XY + heading_valid
        # + pose_conf + ball_v + arco rival, ahora que el contrato los transmite).
        ttk.Label(body, text="ECO DEL SNAPSHOT (pose / pelota / arcos / árbitro)",
                  style="Muted.TLabel").grid(row=4, column=0, columnspan=2, sticky="w")
        self.snap_txt = tk.Text(body, width=58, height=11, font=("Consolas", 10),
                                bg=PANEL, fg=FG, relief="flat", highlightthickness=0)
        self.snap_txt.grid(row=5, column=0, columnspan=2, sticky="we", pady=(0, 6))

    def _make_link_tile(self, parent: ttk.Frame, col: int, title: str) -> dict:
        frame = tk.Frame(parent, bd=1, relief="solid", padx=8, pady=6,
                         width=240, height=96)
        frame.grid(row=0, column=col, padx=4, sticky="nw")
        frame.pack_propagate(False)
        title_lbl = tk.Label(frame, text=title, anchor="w",
                             font=("Segoe UI", 10, "bold"), fg="#fff")
        title_lbl.pack(anchor="w")
        badge = tk.Label(frame, text="—", anchor="w",
                         font=("Consolas", 9, "bold"), fg="#fff")
        badge.pack(anchor="w")
        detail = tk.Label(frame, text="", anchor="w", justify="left",
                          font=("Consolas", 9), fg="#eee", wraplength=224)
        detail.pack(anchor="w")
        return {"frame": frame, "title": title_lbl, "badge": badge, "detail": detail}

    def _draw_dial_static(self) -> None:
        c = self.dial
        cx = cy = DIAL_PX / 2
        r = DIAL_PX / 2 - 8
        c.create_oval(cx - r, cy - r, cx + r, cy + r, outline="#2a3340", width=2)
        # marcas cardinales: 0° arriba (frente), CCW+ como el resto del monitor.
        for ang, lbl in ((0, "0"), (90, "90"), (180, "180"), (-90, "-90")):
            a = math.radians(ang)
            tx = cx - math.sin(a) * (r - 12)
            ty = cy - math.cos(a) * (r - 12)
            c.create_text(tx, ty, text=lbl, fill="#566", font=("Consolas", 7))
        self._needle = c.create_line(cx, cy, cx, cy - r + 14, fill="#39d3a0",
                                     width=3, arrow="last")
        self._dial_cx, self._dial_cy, self._dial_r = cx, cy, r

    # ── Render ────────────────────────────────────────────────────────────────
    def render(self, f: CentralFrame) -> None:
        self._render_links(f)
        self._render_otos(f)
        self._render_loop(f)
        self._render_snap(f)
        seq = getattr(f, "seq", 0)
        v = getattr(f, "v", "?")
        top = getattr(f, "top", None)
        down = getattr(f, "down", None)
        top_ok = bool(getattr(top, "fresh", False))
        down_ok = bool(getattr(down, "line_fresh", False))
        self.header.configure(
            text=(f"seq={seq}  v{v}     "
                  f"TOP={'OK' if top_ok else 'CAÍDO'}   "
                  f"DOWN={'OK' if down_ok else 'CAÍDO'}"))

    def _set_tile(self, tile: dict, status: str, detail: str) -> None:
        color = STATUS_COLOR[status]
        tile["frame"].configure(bg=color)
        tile["title"].configure(bg=color)
        tile["badge"].configure(bg=color, text=STATUS_LABEL[status])
        tile["detail"].configure(bg=color, text=detail)

    def _render_links(self, f: CentralFrame) -> None:
        # TOP: fresh = verde; sin fresco = sin dato (gris). Sin un criterio de
        # "falla dura" en el contrato, fresco↔no-fresco es el semáforo honesto.
        top = getattr(f, "top", None)
        tfresh = bool(getattr(top, "fresh", False))
        self._set_tile(
            self._top_tile,
            Status.OK if tfresh else Status.NODATA,
            (f"fresco={'SÍ' if tfresh else 'no'}   edad={getattr(top, 'age', 0)} ms\n"
             f"frames={getattr(top, 'fr', 0)}  crc={getattr(top, 'crc', 0)}\n"
             f"resync={getattr(top, 'rsy', 0)}  badsz={getattr(top, 'badsz', 0)}\n"
             f"rxB={getattr(top, 'rxB', 0)}"))

        # DOWN: fresco Y válido = verde; fresco-pero-no-válido = revisar (llegó
        # dato pero la línea no es confiable); sin fresco = sin dato.
        down = getattr(f, "down", None)
        dfresh = bool(getattr(down, "line_fresh", False))
        dvalid = bool(getattr(down, "valid", False))
        if dfresh and dvalid:
            st = Status.OK
        elif dfresh:
            st = Status.WARN
        else:
            st = Status.NODATA
        self._set_tile(
            self._down_tile, st,
            (f"fresco={'SÍ' if dfresh else 'no'}   válido={'SÍ' if dvalid else 'NO'}\n"
             f"edad={getattr(down, 'age', 0)} ms\n"
             # lost y crc SEPARADOS (no sumar): naturalezas distintas.
             f"rx={getattr(down, 'rx', 0)}  crc={getattr(down, 'crc', 0)}\n"
             f"lost={getattr(down, 'lost', 0)}  resync={getattr(down, 'rsy', 0)}\n"
             f"badsch={getattr(down, 'badsch', 0)}  ev=0x{getattr(down, 'ev', 0):02x}"))

    def _render_otos(self, f: CentralFrame) -> None:
        otos = getattr(f, "otos", None)
        fresh = bool(getattr(otos, "fresh", False))
        try:
            hdg = float(getattr(otos, "hdg", 0.0))
        except (TypeError, ValueError):
            hdg = 0.0
        cx, cy, r = self._dial_cx, self._dial_cy, self._dial_r
        a = math.radians(hdg)        # 0=frente(arriba), CCW+ (igual que el monitor)
        ex = cx - math.sin(a) * (r - 14)
        ey = cy - math.cos(a) * (r - 14)
        color = "#39d3a0" if fresh else "#4a4f55"   # grisear si no fresco
        self.dial.coords(self._needle, cx, cy, ex, ey)
        self.dial.itemconfig(self._needle, fill=color)
        if fresh:
            self.otos_lbl.configure(text=f"hdg {hdg:+.1f}°", fg=FG)
        else:
            self.otos_lbl.configure(text=f"hdg {hdg:+.1f}°  (viejo)", fg=MUTED)

    def _render_loop(self, f: CentralFrame) -> None:
        loop = getattr(f, "loop", None)
        mx = int(getattr(loop, "max_us", 0) or 0)
        ema = int(getattr(loop, "ema_us", 0) or 0)
        if mx >= LOOP_BAD_US:
            bg, fg, tag = "#3a1414", "#ff6b6b", "⛔ LOOP LARGO"
        elif mx >= LOOP_WARN_US:
            bg, fg, tag = "#3a2e10", "#ffd166", "⚠ loop al límite"
        else:
            bg, fg, tag = PANEL, FG, "✓ loop OK"
        self.loop_lbl.configure(
            text=(f"max  : {mx:6d} µs\n"
                  f"ema  : {ema:6d} µs\n"
                  f"{tag}  (umbral {LOOP_WARN_US // 1000} ms)"),
            bg=bg, fg=fg)

    def _render_snap(self, f: CentralFrame) -> None:
        snap = getattr(f, "snap", None)
        # — pelota (relativa al robot) —
        ball_vis = bool(getattr(snap, "ball_vis", False))
        bx = getattr(snap, "ball_x", 0)
        by = getattr(snap, "ball_y", 0)
        bvx = getattr(snap, "ball_vx", 0)
        bvy = getattr(snap, "ball_vy", 0)
        # — arco propio (polar) —
        gang = getattr(snap, "goal_own_ang", 0.0)
        gdist = getattr(snap, "goal_own_dist", 0)
        # — arco rival (polar) AMPLIADO 2026-06-17 —
        goal_opp_vis = bool(getattr(snap, "goal_opp_vis", False))
        goang = getattr(snap, "goal_opp_ang", 0.0)
        godist = getattr(snap, "goal_opp_dist", 0)
        # — pose absoluta del robot AMPLIADA 2026-06-17 —
        # has_pose=True solo si my_pose_conf > 0 (TOP anclando).
        has_pose = bool(getattr(snap, "has_pose", False))
        my_x = getattr(snap, "my_x", 0)
        my_y = getattr(snap, "my_y", 0)
        my_hdg = getattr(snap, "my_heading_deg", 0.0)
        hdg_valid = bool(getattr(snap, "heading_valid", False))
        conf = getattr(snap, "my_pose_conf", 0)
        # — árbitro / flags —
        ref = int(getattr(snap, "referee", 0) or 0)
        flags = int(getattr(snap, "flags", 0) or 0)
        ref_name = REFEREE.get(ref, f"?{ref}")
        names = decode_snap_flags(flags)

        # Pose: si conf=0 el TOP NO ANCLA → no mostramos coords (sería 0,0 falso).
        if has_pose:
            pose_line = f"x={my_x:+5d} y={my_y:+5d} mm   conf={conf}/100"
        else:
            pose_line = f"NO ANCLA (conf=0) — pose desconocida"
        hdg_line = (f"hdg={my_hdg:+6.1f}°   valid={'SÍ' if hdg_valid else 'NO'}")
        ball = (f"x={bx:+5d} y={by:+5d} mm (rel.)   v=({bvx:+4d},{bvy:+4d}) mm/s"
                if ball_vis else "no la ve")
        gopp = (f"{goang:+.1f}°   {godist} mm" if goal_opp_vis else "no lo ve")

        self._set_text(self.snap_txt,
            f"pose robot    : {pose_line}\n"
            f"heading       : {hdg_line}\n"
            f"pelota        : {ball}\n"
            f"arco propio   : {gang:+.1f}°   {gdist} mm\n"
            f"arco rival    : {gopp}\n"
            f"árbitro       : {ref_name}\n"
            f"flags         : {', '.join(names) if names else '—'}\n"
            f"\n"
            f"(pose en marco CANCHA; pelota y arcos relativos al ROBOT)\n")

    # ── Helpers ───────────────────────────────────────────────────────────────
    def _set_text(self, widget: tk.Text, text: str) -> None:
        widget.delete("1.0", "end")
        widget.insert("1.0", text)
