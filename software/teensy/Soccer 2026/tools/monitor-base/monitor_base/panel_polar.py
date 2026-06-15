"""panel_polar.py — Panel POLAR (cenital) de la placa TOP (embebible en el shell).

Refactor de gui_polar.PolarApp a Panel: mismo radar cenital (robot al centro,
frente hacia ARRIBA; conos ToF con profundidad por zona, conos de cámara,
pelota/arcos/velocidad) + los dos paneles de texto laterales (cámaras y ToF),
pero sin ventana/loop/fuente/banner/status propios (el shell se los provee).

La GEOMETRÍA PURA (position_bearing, column_azimuths, zone_polar_points,
polar_xy, to_xy) se IMPORTA de gui_polar — ya está testeada, no se recopia.
render(f) es el _render de PolarApp. La config de ToF se levanta con
load_or_default(self.ctx.config_path).
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk

from .gui_polar import (
    CAM_HFOV_DEG,
    MAX_RANGE_MM,
    RADAR_PX,
    RANGE_RINGS_MM,
    TOF_HFOV_DEG,
    polar_xy,
    position_bearing,
    to_xy,
    zone_polar_points,
)
from .panel import Panel
from .protocol_top import TopFrame
from .shell_theme import FG, MUTED, PANEL
from .tof_layout import N_ZONES, load_or_default
from .zones import zone_color


class PolarPanel(Panel):
    title = "Polar"
    key = "polar"
    icon = "◎"
    sends_commands = False

    def build(self, parent: tk.Frame) -> None:
        self.cfg = load_or_default(self.ctx.config_path)
        self.frame_count = 0
        self._cx = self._cy = RADAR_PX / 2
        self._scale = (RADAR_PX / 2 - 18) / MAX_RANGE_MM

        main = ttk.Frame(parent, padding=8)
        main.pack(fill="both", expand=True)

        left = ttk.Frame(main)
        left.grid(row=0, column=0, sticky="n")
        self.canvas = tk.Canvas(left, width=RADAR_PX, height=RADAR_PX,
                                bg="#0b1410", highlightthickness=0)
        self.canvas.pack()
        ttk.Label(left, style="Muted.TLabel",
                  text="ARRIBA = frente · conos = ToF (prof. por zona) y cámaras · "
                       "reparto de columnas APROX (FOV sin confirmar)").pack(pady=(4, 0))

        right = ttk.Frame(main)
        right.grid(row=0, column=1, sticky="n", padx=(12, 0))
        ttk.Label(right, text="CÁMARAS (pelota + arcos)").pack(anchor="w")
        self.cam_txt = tk.Text(right, width=46, height=10, font=("Consolas", 10),
                               bg=PANEL, fg=FG, relief="flat", highlightthickness=0)
        self.cam_txt.pack(pady=(0, 6))
        ttk.Label(right, text="ToF (profundidad mínima por dirección)").pack(anchor="w")
        self.tof_txt = tk.Text(right, width=46, height=7, font=("Consolas", 10),
                               bg=PANEL, fg=FG, relief="flat", highlightthickness=0)
        self.tof_txt.pack(pady=(0, 6))
        self.info = ttk.Label(parent, style="Muted.TLabel", anchor="w",
                              text="esperando datos…", padding=(8, 2))
        self.info.pack(fill="x", side="bottom")

        self._draw_static()

    def _draw_static(self) -> None:
        c = self.canvas
        for r_mm in RANGE_RINGS_MM:
            r = r_mm * self._scale
            c.create_oval(self._cx - r, self._cy - r, self._cx + r, self._cy + r,
                          outline="#1d3a2a")
            c.create_text(self._cx + 3, self._cy - r - 7, text=f"{r_mm/1000:.1f}m",
                          fill="#2f5a44", font=("Consolas", 7), anchor="w")
        c.create_polygon(self._cx, self._cy - 14, self._cx - 10, self._cy + 10,
                         self._cx + 10, self._cy + 10, fill="#2b6", outline="#7fd")

    # ── Render (= PolarApp._render) ─────────────────────────────────────────
    def render(self, f: TopFrame) -> None:
        self.frame_count += 1
        c = self.canvas
        c.delete("dyn")     # borra todo lo dinámico de la pasada anterior
        self._draw_tof_cones(f)
        self._draw_cam_cones(f)
        self._draw_ball(f)
        self._render_panels(f)
        self.info.configure(
            text=f"seq={f.seq} · frames={self.frame_count} · v{f.v} · "
                 f"zonas ToF: {'sí' if f.tof.zones else 'pendiente firmware'}")

    def _draw_tof_cones(self, f: TopFrame) -> None:
        c = self.canvas
        zones = f.tof.zones
        for idx in range(4):
            if not self.cfg.sensor_enabled.get(idx, True):
                continue
            pos = self.cfg.position.get(idx, "FRONT")
            bearing = position_bearing(pos)
            half = TOF_HFOV_DEG / 2.0
            e1 = polar_xy(bearing - half, MAX_RANGE_MM, self._scale, self._cx, self._cy)
            e2 = polar_xy(bearing + half, MAX_RANGE_MM, self._scale, self._cx, self._cy)
            c.create_line(self._cx, self._cy, *e1, fill="#1b3a4a", tags="dyn")
            c.create_line(self._cx, self._cy, *e2, fill="#1b3a4a", tags="dyn")
            lab = polar_xy(bearing, MAX_RANGE_MM * 0.92, self._scale, self._cx, self._cy)
            c.create_text(*lab, text=f"{idx}·{pos[:1]}", fill="#3a6a82",
                          font=("Consolas", 8), tags="dyn")
            if not zones or idx >= len(zones):
                continue
            disp = self.cfg.oriented_cells(zones[idx], idx)
            enabled = self.cfg.zone_enabled.get(idx, [True] * N_ZONES)
            for az, depth, _z in zone_polar_points(disp, bearing, enabled):
                px, py = polar_xy(az, float(depth), self._scale, self._cx, self._cy)
                col = zone_color(depth)
                c.create_line(self._cx, self._cy, px, py, fill=col, width=1, tags="dyn")
                c.create_oval(px - 3, py - 3, px + 3, py + 3, fill=col, outline="", tags="dyn")

    def _draw_cam_cones(self, f: TopFrame) -> None:
        c = self.canvas
        for bearing, per, name, color in (
                (0.0, f.camf, "cam F", "#2a6"),
                (180.0, f.camb, "cam B", "#a52"),):
            half = CAM_HFOV_DEG / 2.0
            e1 = polar_xy(bearing - half, MAX_RANGE_MM, self._scale, self._cx, self._cy)
            e2 = polar_xy(bearing + half, MAX_RANGE_MM, self._scale, self._cx, self._cy)
            c.create_line(self._cx, self._cy, *e1, fill="#243", dash=(3, 3), tags="dyn")
            c.create_line(self._cx, self._cy, *e2, fill="#243", dash=(3, 3), tags="dyn")
            self._draw_goal(per.yellow_visible, per.yellow_angle_deg, per.yellow_distance_mm, "#e6d44a")
            self._draw_goal(per.blue_visible, per.blue_angle_deg, per.blue_distance_mm, "#4a78e6")

    def _draw_goal(self, visible: bool, angle_deg: float, dist_mm: int, color: str) -> None:
        if not visible:
            return
        gx, gy = polar_xy(angle_deg, float(dist_mm), self._scale, self._cx, self._cy)
        self.canvas.create_rectangle(gx - 8, gy - 8, gx + 8, gy + 8, fill=color,
                                     outline="#fff", tags="dyn")

    def _draw_ball(self, f: TopFrame) -> None:
        c = self.canvas
        if not f.cam.ball_visible:
            return
        bx, by = to_xy(f.cam.ball_x_mm, f.cam.ball_y_mm, self._scale, self._cx, self._cy)
        c.create_line(self._cx, self._cy, bx, by, fill="#7a4a1a", tags="dyn")
        c.create_oval(bx - 8, by - 8, bx + 8, by + 8, fill="#ff8c2a", outline="#ffd", tags="dyn")
        if f.cam.ball_vx_mm_s or f.cam.ball_vy_mm_s:
            vx, vy = to_xy(f.cam.ball_x_mm + f.cam.ball_vx_mm_s,
                           f.cam.ball_y_mm + f.cam.ball_vy_mm_s, self._scale, self._cx, self._cy)
            c.create_line(bx, by, vx, vy, fill="#ffd27a", width=2, arrow="last", tags="dyn")

    def _render_panels(self, f: TopFrame) -> None:
        def yn(b):
            return "SÍ" if b else "no"
        cam = f.cam
        ball = (f"x={cam.ball_x_mm:+5d} y={cam.ball_y_mm:+5d}  v=({cam.ball_vx_mm_s:+d},{cam.ball_vy_mm_s:+d})"
                if cam.ball_visible else "—")
        self._set_text(self.cam_txt,
            f"cámara front / back : {yn(cam.front_ok)} / {yn(cam.back_ok)}\n"
            f"¿PELOTA?            : {yn(cam.ball_visible)}\n"
            f"posición pelota     : {ball}\n"
            f"¿ARCO AMARILLO?     : {yn(cam.yellow_visible)}  ang={cam.yellow_angle_deg:+.1f}° d={cam.yellow_distance_mm}mm\n"
            f"¿ARCO AZUL?         : {yn(cam.blue_visible)}  ang={cam.blue_angle_deg:+.1f}° d={cam.blue_distance_mm}mm\n")
        lines = []
        for idx in range(4):
            pos = self.cfg.position.get(idx, "FRONT")
            mn = None
            if f.tof.zones and idx < len(f.tof.zones):
                disp = self.cfg.oriented_cells(f.tof.zones[idx], idx)
                en = self.cfg.zone_enabled.get(idx, [True] * N_ZONES)
                vals = [v for z, v in enumerate(disp) if v is not None and (z >= len(en) or en[z])]
                mn = min(vals) if vals else None
            elif idx < len(f.tof.distances_mm):
                mn = f.tof.distances_mm[idx]
            lines.append(f"ToF {idx} ({pos:5s}) : {('—' if mn is None else str(mn) + ' mm'):>9s}")
        self._set_text(self.tof_txt, "\n".join(lines) +
                       f"\nHC-SR04 = {'—' if f.tof.hcsr04_mm is None else str(f.tof.hcsr04_mm)+' mm'}\n")

    def _set_text(self, w: tk.Text, text: str) -> None:
        w.delete("1.0", "end")
        w.insert("1.0", text)
