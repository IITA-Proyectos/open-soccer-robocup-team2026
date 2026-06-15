"""panel_arquero.py — Panel de ARQUERO (seguidor de línea + OTOS) embebible.

Refactor de gui_gk.MonitorGkApp a Panel: las mismas tres piezas que la vista vieja
de banco, sin ventana/loop/fuente propios (el shell se los provee):

  • Medidor grande de CROSS-TRACK (mm a la línea, objetivo 0 = centrado), con el
    color por |xt| (verde<20 · amarillo<60 · rojo).
  • Anillo con los sensores TRASEROS resaltados (la línea del arco) + flecha de la
    línea detectada.
  • Estela de la trayectoria por ODOMETRÍA OTOS (pose fusionada + cada OTOS izq/der
    por separado, auto-encuadre) — para ver el diferencial / que avance derecho.

Consume Frame de la placa BASE (protocol.py, schema v2/v3). El dibujo se LEVANTA
de gui_gk (no se reinventa) y la geometría del anillo se IMPORTA de geometry
(SENSOR_POS, SENSOR_COUNT, bounds) — no se copia. La ESTELA (deque) vive en la
instancia y se acumula en render(f).

Este panel NO manda comandos a la placa (sends_commands=False); el banner de
SIMULADOR y la status bar los pone el shell, así que acá no van. El único botón es
"Limpiar estela", que es estado LOCAL del panel.
"""
from __future__ import annotations

import math
import tkinter as tk
from collections import deque
from tkinter import ttk
from typing import Deque, Tuple

from . import geometry
from .panel import Panel
from .protocol import Frame
from .shell_theme import BG, FG, MUTED, PANEL

# Sensores "traseros" = los que miran hacia atrás del robot (y < umbral): los que
# ven la línea del arco cuando el arquero está de frente a la cancha. Mismo
# criterio que gui_gk.BACK_INDICES.
BACK_INDICES = [i for i in range(geometry.SENSOR_COUNT)
                if geometry.SENSOR_POS[i][1] < -5.0]

TRAIL_CANVAS = 300
RING_CANVAS = 300
GAUGE_W, GAUGE_H = 620, 56
XTRACK_RANGE_MM = 150.0     # fondo de escala del medidor de cross-track
TRAIL_MAXLEN = 500


class ArqueroPanel(Panel):
    title = "Arquero"
    key = "arquero"
    icon = "⊓"
    board = "down"
    sends_commands = False

    # ── Layout ────────────────────────────────────────────────────────────
    def build(self, parent: tk.Frame) -> None:
        # Estado en la instancia (la estela vive acá, se acumula en render).
        self.trail: Deque[Tuple[float, float]] = deque(maxlen=TRAIL_MAXLEN)
        self.frame_count = 0

        self.header = tk.Label(
            parent, text="esperando datos…", anchor="w",
            font=("Consolas", 11, "bold"), bg=BG, fg=FG, padx=8, pady=6)
        self.header.pack(fill="x")

        main = ttk.Frame(parent, padding=8)
        main.pack(fill="both", expand=True)

        ttk.Label(main, text="CROSS-TRACK — distancia a la línea (objetivo 0 = centrado)"
                  ).grid(row=0, column=0, columnspan=2, sticky="w")
        self.gauge = tk.Canvas(main, width=GAUGE_W, height=GAUGE_H,
                               bg=PANEL, highlightthickness=0)
        self.gauge.grid(row=1, column=0, columnspan=2, pady=(0, 8))

        # Estela OTOS (izq) + arco trasero (der).
        lf = ttk.Frame(main); lf.grid(row=2, column=0, sticky="n")
        ttk.Label(lf, text="Trayectoria por OTOS  (●fusión ·azul=izq ·rojo=der)"
                  ).pack()
        self.trail_c = tk.Canvas(lf, width=TRAIL_CANVAS, height=TRAIL_CANVAS,
                                 bg=PANEL, highlightthickness=0)
        self.trail_c.pack()

        rf = ttk.Frame(main); rf.grid(row=2, column=1, sticky="n", padx=(10, 0))
        ttk.Label(rf, text="Anillo — sensores TRASEROS (línea del arco) resaltados"
                  ).pack()
        self.ring_c = tk.Canvas(rf, width=RING_CANVAS, height=RING_CANVAS,
                                bg=PANEL, highlightthickness=0)
        self.ring_c.pack()
        self._draw_static_ring()

        # Paneles de texto.
        pf = ttk.Frame(main); pf.grid(row=3, column=0, columnspan=2, sticky="we",
                                      pady=(8, 0))
        self.line_txt = tk.Text(pf, width=44, height=8, font=("Consolas", 10),
                                bg=PANEL, fg=FG, relief="flat", highlightthickness=0)
        self.line_txt.grid(row=0, column=0, padx=(0, 8))
        self.otos_txt = tk.Text(pf, width=46, height=8, font=("Consolas", 10),
                                bg=PANEL, fg=FG, relief="flat", highlightthickness=0)
        self.otos_txt.grid(row=0, column=1)

        bottom = ttk.Frame(main, padding=(0, 8))
        bottom.grid(row=4, column=0, columnspan=2, sticky="we")
        ttk.Button(bottom, text="↺ Limpiar estela",
                   command=self._clear_trail).grid(row=0, column=0, padx=3)

    def _clear_trail(self) -> None:
        self.trail.clear()
        self.ctx.log("info", "estela del arquero limpiada")

    def _draw_static_ring(self) -> None:
        c = self.ring_c
        cx = cy = RING_CANVAS / 2
        min_x, min_y, max_x, max_y = geometry.bounds()
        span = max(max_x - min_x, max_y - min_y) + 24
        self._ring_scale = (RING_CANVAS - 24) / span
        self._ring_cx, self._ring_cy = cx, cy
        self._dots = []
        back = set(BACK_INDICES)
        for i in range(geometry.SENSOR_COUNT):
            x, y = geometry.SENSOR_POS[i]
            px = cx + x * self._ring_scale
            py = cy - y * self._ring_scale
            r = 8 if i in back else 4           # traseros más grandes
            outline = "#3a86ff" if i in back else "#333"
            dot = c.create_oval(px - r, py - r, px + r, py + r,
                                fill="#202020", outline=outline,
                                width=2 if i in back else 1)
            self._dots.append((dot, r))
        c.create_line(cx, cy, cx, cy - 26, fill="#2a3a2a", dash=(2, 2))
        self._line_arrow = c.create_line(cx, cy, cx, cy, fill="#ffd24a", width=4,
                                         arrow="last", state="hidden")

    # ── Render ──────────────────────────────────────────────────────────────
    def render(self, f: Frame) -> None:
        self.frame_count += 1
        # La estela se acumula acá (en la GUI vieja era el _consume del loop).
        if f.otos.has_pose:
            self.trail.append((f.otos.x_mm, f.otos.y_mm))
        self._render_gauge(f)
        self._render_ring(f)
        self._render_trail(f)
        self._render_panels(f)
        self.header.configure(
            text=(f"seq={f.seq}  v{f.v}     "
                  f"estela: {len(self.trail)} pts   frames: {self.frame_count}"))

    def _render_gauge(self, f: Frame) -> None:
        g = self.gauge
        g.delete("dyn")
        mid = GAUGE_W / 2
        g.create_line(mid, 4, mid, GAUGE_H - 4, fill="#3a6", tags="dyn")  # cero
        g.create_text(mid, GAUGE_H - 8, text="0", fill="#3a6",
                      font=("Consolas", 8), tags="dyn")
        xt = f.line.cross_track_mm
        if not f.line.valid or xt is None:
            g.create_text(mid, GAUGE_H / 2, text="cross-track N/A",
                          fill="#888", font=("Consolas", 12), tags="dyn")
            return
        frac = max(-1.0, min(1.0, xt / XTRACK_RANGE_MM))
        x = mid + frac * (GAUGE_W / 2 - 12)
        a = abs(xt)
        color = "#2ecc40" if a < 20 else ("#ffdc00" if a < 60 else "#ff4136")
        g.create_rectangle(min(mid, x), 10, max(mid, x), GAUGE_H - 18,
                           fill=color, outline="", tags="dyn")
        g.create_text(x, GAUGE_H / 2, text=f"{xt:+d} mm", fill="#fff",
                      font=("Consolas", 13, "bold"), tags="dyn")

    def _render_ring(self, f: Frame) -> None:
        c = self.ring_c
        back = set(BACK_INDICES)
        for i, (dot, r) in enumerate(self._dots):
            white = f.ring.white[i] if i < len(f.ring.white) else False
            if white:
                fill = "#ffe27a"
                outline = "#ffaa00"
            else:
                fill = "#243" if i in back else "#1c1c1c"
                outline = "#3a86ff" if i in back else "#333"
            c.itemconfig(dot, fill=fill, outline=outline)
        if f.line.present and f.line.angle_deg is not None:
            a = math.radians(f.line.angle_deg)   # 0=frente, horario+
            length = 70 + (f.line.penetration_mm or 0) * 0.4
            ex = self._ring_cx + math.sin(a) * length
            ey = self._ring_cy - math.cos(a) * length
            c.coords(self._line_arrow, self._ring_cx, self._ring_cy, ex, ey)
            c.itemconfig(self._line_arrow, state="normal")
        else:
            c.itemconfig(self._line_arrow, state="hidden")

    def _render_trail(self, f: Frame) -> None:
        c = self.trail_c
        c.delete("dyn")
        pts = list(self.trail)
        if not pts:
            c.create_text(TRAIL_CANVAS / 2, TRAIL_CANVAS / 2,
                          text="(moviendo el robot\naparece la estela)",
                          fill="#556", font=("Consolas", 9), tags="dyn")
            return
        xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
        # incluir las poses por-OTOS actuales en el encuadre
        xs += [f.otos.left_x_mm, f.otos.right_x_mm]
        ys += [f.otos.left_y_mm, f.otos.right_y_mm]
        minx, maxx, miny, maxy = min(xs), max(xs), min(ys), max(ys)
        span = max(maxx - minx, maxy - miny, 200.0)   # span mínimo 200 mm
        scale = (TRAIL_CANVAS - 30) / span
        ox = (minx + maxx) / 2
        oy = (miny + maxy) / 2

        def to_px(x, y):
            return (TRAIL_CANVAS / 2 + (x - ox) * scale,
                    TRAIL_CANVAS / 2 - (y - oy) * scale)

        # estela
        flat = []
        for (x, y) in pts:
            px, py = to_px(x, y)
            flat += [px, py]
        if len(flat) >= 4:
            c.create_line(*flat, fill="#2f6f4f", width=2, tags="dyn")
        # OTOS izq/der actuales
        if f.otos.left_ok:
            lx, ly = to_px(f.otos.left_x_mm, f.otos.left_y_mm)
            c.create_oval(lx - 4, ly - 4, lx + 4, ly + 4, fill="#3a86ff",
                          outline="", tags="dyn")
        if f.otos.right_ok:
            rx, ry = to_px(f.otos.right_x_mm, f.otos.right_y_mm)
            c.create_oval(rx - 4, ry - 4, rx + 4, ry + 4, fill="#ff4136",
                          outline="", tags="dyn")
        # robot (pose fusionada) + flecha de heading (CCW+)
        bx, by = to_px(f.otos.x_mm, f.otos.y_mm)
        c.create_oval(bx - 6, by - 6, bx + 6, by + 6, fill="#2ecc40",
                      outline="#dfd", tags="dyn")
        a = math.radians(f.otos.heading_deg)
        c.create_line(bx, by, bx - math.sin(a) * 22, by - math.cos(a) * 22,
                      fill="#dfd", width=2, arrow="last", tags="dyn")

    def _render_panels(self, f: Frame) -> None:
        ln, o = f.line, f.otos
        back_on = sum(1 for i in BACK_INDICES
                      if i < len(f.ring.white) and f.ring.white[i])
        ang = "N/A" if ln.angle_deg is None else f"{ln.angle_deg:+.1f}°"
        xt = "N/A" if ln.cross_track_mm is None else f"{ln.cross_track_mm:+d} mm"
        pen = "N/A" if ln.penetration_mm is None else f"{ln.penetration_mm} mm"
        gate = "VÁLIDO" if ln.valid else "*** NO VÁLIDO (data_valid=0) ***"
        self._set_text(self.line_txt,
            f"estado         : {gate}\n"
            f"línea presente : {'SÍ' if ln.present else 'no'}\n"
            f"CROSS-TRACK    : {xt}   (objetivo 0)\n"
            f"ángulo línea   : {ang}   (paralelo = alineado)\n"
            f"penetración    : {pen}\n"
            f"sensores línea : {ln.sensors_on_line}/32   (traseros {back_on}/{len(BACK_INDICES)})\n"
            f"eventos        : {', '.join(ln.flag_names) or '—'}\n")

        self._set_text(self.otos_txt,
            f"fusión  x/y/hdg : {o.x_mm:+7.1f} {o.y_mm:+7.1f} {o.heading_deg:+6.1f}°\n"
            f"OTOS izq x/y/h  : {o.left_x_mm:+7.1f} {o.left_y_mm:+7.1f} {o.left_heading_deg:+6.1f}°  [{'ok' if o.left_ok else '--'}]\n"
            f"OTOS der x/y/h  : {o.right_x_mm:+7.1f} {o.right_y_mm:+7.1f} {o.right_heading_deg:+6.1f}°  [{'ok' if o.right_ok else '--'}]\n"
            f"vel  vx/vy/w    : {o.vx_mm_s:+6.1f} {o.vy_mm_s:+6.1f} {o.omega_rad_s:+5.2f}\n"
            f"slip (difer.)   : {o.slip_mm_s:6.1f} mm/s\n"
            f"levantado       : {'SÍ' if f.lifted else 'no'}\n")

    # ── Helpers ───────────────────────────────────────────────────────────
    def _set_text(self, widget: tk.Text, text: str) -> None:
        widget.delete("1.0", "end")
        widget.insert("1.0", text)
