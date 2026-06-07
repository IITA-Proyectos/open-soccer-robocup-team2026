"""gui_top.py — GUI del campo de la placa TOP (FASE 2).

Vista RADAR robot-céntrica (el robot al centro, su frente hacia ARRIBA): dibuja
la pelota y los arcos detectados por las cámaras en su posición/ángulo relativo,
los 4 ToF como rayos, una brújula con el heading del IMU, y paneles con la
interpretación que TOP manda a la CENTRAL (WorldSnapshot). Botones IMU/stream.

Se alimenta de una FrameSource que entrega TopFrame (protocol_top). Para correr
sin display usar `--top --selftest` (no importa este módulo).
"""
from __future__ import annotations

import math
import tkinter as tk
from tkinter import ttk
from typing import Optional

from .protocol_top import TopFrame
from .sources import FrameSource

RADAR_PX = 460
MAX_RANGE_MM = 2500.0          # rango que mapea al borde del radar
RANGE_RINGS_MM = (500, 1000, 2000)


class MonitorTopApp:
    def __init__(self, root: tk.Tk, source: FrameSource, poll_ms: int = 50,
                 recorder=None):
        self.root = root
        self.source = source
        self.poll_ms = poll_ms
        self.recorder = recorder
        self.last: Optional[TopFrame] = None
        self.frame_count = 0
        self.last_seq = -1
        self.dropped = 0

        root.title("IITA Soccer — Monitor del campo (TOP) v1")
        self._cx = self._cy = RADAR_PX / 2
        self._scale = (RADAR_PX / 2 - 16) / MAX_RANGE_MM
        self._build_layout()
        self._draw_static()
        self.source.start()
        self.root.after(self.poll_ms, self._tick)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ── Layout ────────────────────────────────────────────────────────────
    def _build_layout(self) -> None:
        main = ttk.Frame(self.root, padding=8)
        main.pack(fill="both", expand=True)

        left = ttk.Frame(main)
        left.grid(row=0, column=0, sticky="n")
        self.canvas = tk.Canvas(left, width=RADAR_PX, height=RADAR_PX,
                                bg="#0b1410", highlightthickness=0)
        self.canvas.pack()
        ttk.Label(left, text="Radar robot-céntrico — ARRIBA = frente del robot",
                  foreground="#888").pack(pady=(4, 0))

        right = ttk.Frame(main)
        right.grid(row=0, column=1, sticky="n", padx=(12, 0))

        def panel(title, h):
            ttk.Label(right, text=title).pack(anchor="w")
            t = tk.Text(right, width=44, height=h, font=("Consolas", 10),
                        bg="#0c0f12", fg="#d8e0e8", relief="flat")
            t.pack(pady=(0, 6))
            return t

        self.cam_txt = panel("CÁMARAS (pelota + arcos)", 7)
        self.imu_txt = panel("IMU (heading)", 5)
        self.tof_txt = panel("ToF + HC-SR04", 4)
        self.snap_txt = panel("WORLDSNAPSHOT → CENTRAL", 9)

        bottom = ttk.Frame(main, padding=(0, 8))
        bottom.grid(row=1, column=0, columnspan=2, sticky="we")
        cmds = [
            ("IMU ZERO (apuntar al arco rival)", "IMU ZERO"),
            ("IMU SAVE (EEPROM)", "IMU SAVE"),
            ("Stream ON", "STREAM ON"),
            ("Stream OFF", "STREAM OFF"),
            ("Rate 20", "RATE 20"),
            ("Rate 50", "RATE 50"),
        ]
        for i, (label, cmd) in enumerate(cmds):
            ttk.Button(bottom, text=label,
                       command=lambda c=cmd: self._send(c)).grid(row=0, column=i, padx=3)

        self.status = ttk.Label(self.root, text="iniciando…", anchor="w",
                                relief="sunken", padding=4)
        self.status.pack(fill="x", side="bottom")

    def _send(self, cmd: str) -> None:
        self.source.send(cmd)
        self._set_status(f"→ comando enviado: {cmd}")

    # ── Geometría ─────────────────────────────────────────────────────────
    def _to_px(self, x_mm: float, y_mm: float):
        # +x derecha, +y frente → +y es hacia ARRIBA en pantalla.
        return self._cx + x_mm * self._scale, self._cy - y_mm * self._scale

    def _polar_px(self, angle_deg: float, dist_mm: float):
        # 0°=frente(+Y, arriba), horario+ → +x a la derecha.
        d = min(dist_mm, MAX_RANGE_MM * 0.96)
        a = math.radians(angle_deg)
        return (self._cx + math.sin(a) * d * self._scale,
                self._cy - math.cos(a) * d * self._scale)

    def _draw_static(self) -> None:
        c = self.canvas
        for r_mm in RANGE_RINGS_MM:
            r = r_mm * self._scale
            c.create_oval(self._cx - r, self._cy - r, self._cx + r, self._cy + r,
                          outline="#1d3a2a")
            c.create_text(self._cx + 3, self._cy - r - 7,
                          text=f"{r_mm/1000:.1f}m", fill="#2f5a44",
                          font=("Consolas", 7), anchor="w")
        # Robot: triángulo apuntando arriba (frente).
        c.create_polygon(self._cx, self._cy - 14, self._cx - 10, self._cy + 10,
                         self._cx + 10, self._cy + 10, fill="#2b6", outline="#7fd")
        c.create_line(self._cx, self._cy, self._cx, self._cy - 26,
                      fill="#3a6", dash=(2, 2))
        # Marcadores dinámicos (se recolocan en _render).
        self._ball = c.create_oval(0, 0, 0, 0, fill="#ff8c2a", outline="#ffd",
                                   state="hidden")
        self._ball_line = c.create_line(0, 0, 0, 0, fill="#7a4a1a", state="hidden")
        self._gy = c.create_oval(0, 0, 0, 0, fill="#e6d44a", outline="#fff",
                                 state="hidden")
        self._gb = c.create_oval(0, 0, 0, 0, fill="#4a78e6", outline="#fff",
                                 state="hidden")
        self._tof_rays = [c.create_line(0, 0, 0, 0, fill="#555", width=2)
                          for _ in range(6)]
        # Brújula de heading (esquina sup. izq.).
        self._comp_cx, self._comp_cy, self._comp_r = 34, 34, 22
        c.create_oval(self._comp_cx - self._comp_r, self._comp_cy - self._comp_r,
                      self._comp_cx + self._comp_r, self._comp_cy + self._comp_r,
                      outline="#456")
        c.create_text(self._comp_cx, self._comp_cy - self._comp_r - 6, text="hdg",
                      fill="#678", font=("Consolas", 7))
        self._comp_needle = c.create_line(self._comp_cx, self._comp_cy,
                                          self._comp_cx, self._comp_cy - self._comp_r,
                                          fill="#ff5", width=2, arrow="last")

    # ── Loop ──────────────────────────────────────────────────────────────
    def _tick(self) -> None:
        frames = self.source.poll()
        for f in frames:
            self._consume(f)
        if frames:
            self._render(frames[-1])
        self._drain_errors()
        self.root.after(self.poll_ms, self._tick)

    def _consume(self, f: TopFrame) -> None:
        self.frame_count += 1
        if self.last_seq >= 0:
            gap = f.seq - self.last_seq - 1
            if gap > 0:
                self.dropped += gap
        self.last_seq = f.seq
        if self.recorder is not None:
            self.recorder.write(f)
        self.last = f

    def _drain_errors(self) -> None:
        msgs = []
        while True:
            try:
                msgs.append(self.source.errors.get_nowait())
            except Exception:
                break
        if msgs:
            self._set_status("⚠ " + " | ".join(msgs[-2:]))

    def _render(self, f: TopFrame) -> None:
        c = self.canvas
        # Pelota
        if f.cam.ball_visible:
            bx, by = self._to_px(f.cam.ball_x_mm, f.cam.ball_y_mm)
            c.coords(self._ball, bx - 8, by - 8, bx + 8, by + 8)
            c.coords(self._ball_line, self._cx, self._cy, bx, by)
            c.itemconfig(self._ball, state="normal")
            c.itemconfig(self._ball_line, state="normal")
        else:
            c.itemconfig(self._ball, state="hidden")
            c.itemconfig(self._ball_line, state="hidden")
        # Arcos
        self._place_goal(self._gy, f.cam.yellow_visible,
                         f.cam.yellow_angle_deg, f.cam.yellow_distance_mm)
        self._place_goal(self._gb, f.cam.blue_visible,
                         f.cam.blue_angle_deg, f.cam.blue_distance_mm)
        # ToF — rayos a direcciones cardinales aproximadas (frente/der/atrás/izq…)
        n = min(len(f.tof.distances_mm), len(self._tof_rays))
        for i in range(len(self._tof_rays)):
            if i < n and f.tof.distances_mm[i] is not None:
                ang = (360.0 / n) * i if n else 0.0   # 0=frente, reparto uniforme
                ex, ey = self._polar_px(ang, float(f.tof.distances_mm[i]))
                c.coords(self._tof_rays[i], self._cx, self._cy, ex, ey)
                c.itemconfig(self._tof_rays[i], state="normal")
            else:
                c.itemconfig(self._tof_rays[i], state="hidden")
        # Brújula heading (CCW+)
        a = math.radians(f.imu.heading_deg)
        nx = self._comp_cx + math.sin(a) * self._comp_r
        ny = self._comp_cy - math.cos(a) * self._comp_r
        c.coords(self._comp_needle, self._comp_cx, self._comp_cy, nx, ny)

        self._render_panels(f)
        self._set_status(f"src OK · seq={f.seq} · frames={self.frame_count} · "
                         f"perdidos={self.dropped} · v{f.v}")

    def _place_goal(self, item, visible, angle_deg, dist_mm) -> None:
        if visible:
            gx, gy = self._polar_px(angle_deg, float(dist_mm))
            self.canvas.coords(item, gx - 9, gy - 9, gx + 9, gy + 9)
            self.canvas.itemconfig(item, state="normal")
        else:
            self.canvas.itemconfig(item, state="hidden")

    def _render_panels(self, f: TopFrame) -> None:
        cam, imu, tof, snap = f.cam, f.imu, f.tof, f.snap

        def yn(b):
            return "SÍ" if b else "no"

        ball = (f"x={cam.ball_x_mm:+5d} y={cam.ball_y_mm:+5d} conf={cam.ball_confidence} "
                f"v=({cam.ball_vx_mm_s:+d},{cam.ball_vy_mm_s:+d})"
                if cam.ball_visible else "—")
        self._set_text(self.cam_txt,
            f"cámara front / back : {yn(cam.front_ok)} / {yn(cam.back_ok)}\n"
            f"pelota visible      : {yn(cam.ball_visible)}\n"
            f"pelota              : {ball}\n"
            f"arco AMARILLO       : {yn(cam.yellow_visible)}  ang={cam.yellow_angle_deg:+.1f}°  d={cam.yellow_distance_mm}mm\n"
            f"arco AZUL           : {yn(cam.blue_visible)}  ang={cam.blue_angle_deg:+.1f}°  d={cam.blue_distance_mm}mm\n"
            f"enlace cam (crc/rsy): {cam.crc_errors} / {cam.resyncs}\n")

        self._set_text(self.imu_txt,
            f"heading fusionado : {imu.heading_deg:+7.1f}°\n"
            f"left / right      : {imu.left_deg:+7.1f}° / {imu.right_deg:+7.1f}°\n"
            f"desacuerdo        : {imu.disagreement_deg:5.1f}°  (>5° = problema)\n"
            f"BNO L/R · valid   : {yn(imu.left_ok)}/{yn(imu.right_ok)} · {yn(imu.heading_valid)}\n")

        def mm(v):
            return "  —  " if v is None else f"{v:5d}"
        ds = "  ".join(f"t{i}={mm(d)}" for i, d in enumerate(tof.distances_mm))
        self._set_text(self.tof_txt,
            f"{ds}\n"
            f"HC-SR04 = {mm(tof.hcsr04_mm)} mm     mínimo = {mm(tof.min_mm)} mm\n"
            f"(orientación de los rayos = reparto cardinal aprox.)\n")

        if snap.valid:
            opp = (f"ang={snap.goal_opp_angle_deg:+.1f}° d={snap.goal_opp_distance_mm}mm"
                   if snap.goal_opp_visible else "no visible")
            own = (f"ang={snap.goal_own_angle_deg:+.1f}° d={snap.goal_own_distance_mm}mm"
                   if snap.goal_own_visible else "no visible")
            obst = "libre" if snap.min_obstacle_mm is None else f"{snap.min_obstacle_mm}mm"
            self._set_text(self.snap_txt,
                f"pose propia   : x={snap.my_x_mm:+5d} y={snap.my_y_mm:+5d} "
                f"hdg={snap.my_heading_deg:+.1f}° conf={snap.my_confidence}\n"
                f"pelota        : {yn(snap.ball_visible)}  x={snap.ball_x_mm:+5d} y={snap.ball_y_mm:+5d}\n"
                f"arco RIVAL    : {opp}\n"
                f"arco PROPIO   : {own}\n"
                f"obstáculo min : {obst}\n"
                f"árbitro       : {snap.referee_name}\n"
                f"flags         : {', '.join(snap.flag_names) or '—'}\n")
        else:
            self._set_text(self.snap_txt, "snapshot no válido todavía "
                           "(esperando primer envío a CENTRAL)\n")

    # ── Helpers ───────────────────────────────────────────────────────────
    def _set_text(self, widget: tk.Text, text: str) -> None:
        widget.delete("1.0", "end")
        widget.insert("1.0", text)

    def _set_status(self, text: str) -> None:
        self.status.config(text=text)

    def _on_close(self) -> None:
        try:
            self.source.stop()
            if self.recorder is not None:
                self.recorder.close()
        finally:
            self.root.destroy()


def run_top(source: FrameSource, recorder=None) -> None:
    root = tk.Tk()
    MonitorTopApp(root, source, recorder=recorder)
    root.mainloop()
