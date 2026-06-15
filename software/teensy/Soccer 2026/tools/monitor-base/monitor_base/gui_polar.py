"""gui_polar.py — Vista POLAR (cenital) de la placa TOP.

El robot al centro, su frente hacia ARRIBA. Por cada ToF dibuja su CONO de
detección en la dirección donde está montado, y dentro del cono la PROFUNDIDAD
que mide cada zona (las 4 columnas reparten el ancho del cono; cada zona se marca
a su distancia, coloreada cerca=rojo / lejos=verde). Por cada cámara dibuja su
cono (frontal y trasero) e indica SI VE PELOTA y dónde, y SI VE ARCO (amarillo /
azul) y dónde. Más el ultrasonido y un vector de velocidad de la pelota.

Respeta la config de --tof-setup (tof_layout.json): la POSICIÓN de cada ToF marca
hacia dónde apunta su cono, la ORIENTACIÓN reordena las zonas, y las zonas VETADAS
no se dibujan. Así esta vista y la de configuración cuentan la misma historia.

⚠ Honesto: el FOV real de los ToF/cámaras y el azimut-por-zona exacto están sin
confirmar (el firmware aún no los expone). El reparto de columnas dentro del cono
es una APROXIMACIÓN; sirve para ver "qué tan profundo llega cada zona y en qué
dirección general", no una nube de puntos calibrada.

Sin display: `--polar --selftest` (no importa este módulo).
"""
from __future__ import annotations

import math
import tkinter as tk
from tkinter import ttk
from typing import List, Optional, Tuple

from .protocol_top import TopFrame
from .sources import FrameSource
from .tof_layout import N_ZONES, load_or_default
from .zones import NO_READING_COLOR, zone_color

RADAR_PX = 560
MAX_RANGE_MM = 2500.0
RANGE_RINGS_MM = (500, 1000, 2000)
TOF_HFOV_DEG = 45.0       # FOV horizontal aprox. de cada ToF (sin confirmar)
CAM_HFOV_DEG = 60.0       # FOV horizontal aprox. de cada cámara (sin confirmar)
GRID_W = 4

# Bearing (0=frente, horario+) según la POSICIÓN configurada del ToF.
POSITION_BEARING = {"FRONT": 0.0, "RIGHT": 90.0, "BACK": 180.0, "LEFT": 270.0}


# ── Geometría PURA (testeable, sin Tk) ──────────────────────────────────────
def position_bearing(pos: str) -> float:
    return POSITION_BEARING.get(pos, 0.0)


def column_azimuths(bearing: float, hfov: float = TOF_HFOV_DEG, w: int = GRID_W) -> List[float]:
    """Azimut (grados, misma convención del radar) del centro de cada columna del
    sensor montado mirando a `bearing`. La columna 0 (izquierda de la grilla) cae
    al lado de menor ángulo (CCW = a la izquierda del frente del sensor)."""
    step = hfov / w
    return [bearing + (c - (w - 1) / 2.0) * step for c in range(w)]


def zone_polar_points(display_cells: List[Optional[int]], bearing: float,
                      enabled: Optional[List[bool]] = None,
                      hfov: float = TOF_HFOV_DEG, w: int = GRID_W
                      ) -> List[Tuple[float, int, int]]:
    """Para cada zona CON lectura (y no vetada), devuelve (azimut, profundidad_mm,
    zona_idx). Las 4 filas de una columna comparten azimut → se ven como puntos a
    distinta profundidad sobre el mismo rayo (la 'profundidad de cada zona')."""
    az = column_azimuths(bearing, hfov, w)
    out: List[Tuple[float, int, int]] = []
    for z in range(min(len(display_cells), w * w)):
        if enabled is not None and z < len(enabled) and not enabled[z]:
            continue
        val = display_cells[z]
        if val is None:
            continue
        col = z % w
        out.append((az[col], int(val), z))
    return out


def polar_xy(angle_deg: float, dist_mm: float, scale: float, cx: float, cy: float,
             max_mm: float = MAX_RANGE_MM):
    d = min(dist_mm, max_mm * 0.96)
    a = math.radians(angle_deg)
    return cx + math.sin(a) * d * scale, cy - math.cos(a) * d * scale


def to_xy(x_mm: float, y_mm: float, scale: float, cx: float, cy: float):
    return cx + x_mm * scale, cy - y_mm * scale


# ── GUI ─────────────────────────────────────────────────────────────────────
class PolarApp:
    def __init__(self, root: tk.Tk, source: FrameSource, poll_ms: int = 60,
                 recorder=None, config_path: Optional[str] = None):
        self.root = root
        self.source = source
        self.poll_ms = poll_ms
        self.recorder = recorder
        self.cfg = load_or_default(config_path)
        self.last: Optional[TopFrame] = None
        self.frame_count = 0
        self._is_sim = getattr(source, "is_sim", False)
        self._cx = self._cy = RADAR_PX / 2
        self._scale = (RADAR_PX / 2 - 18) / MAX_RANGE_MM

        root.title("IITA Soccer — Vista POLAR (TOP) — " + source.describe())
        self._build_layout()
        self._draw_static()
        self.source.start()
        self.root.after(self.poll_ms, self._tick)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_layout(self) -> None:
        if self._is_sim:
            tk.Label(self.root, text="⚠ SIMULADOR — datos FALSOS, no es el robot",
                     bg="#b32d00", fg="#ffe27a", font=("Segoe UI", 12, "bold"), pady=5).pack(fill="x")
        main = ttk.Frame(self.root, padding=8)
        main.pack(fill="both", expand=True)
        left = ttk.Frame(main); left.grid(row=0, column=0, sticky="n")
        self.canvas = tk.Canvas(left, width=RADAR_PX, height=RADAR_PX,
                                bg="#0b1410", highlightthickness=0)
        self.canvas.pack()
        ttk.Label(left, foreground="#888",
                  text="ARRIBA = frente · conos = ToF (prof. por zona) y cámaras · "
                       "reparto de columnas APROX (FOV sin confirmar)").pack(pady=(4, 0))
        right = ttk.Frame(main); right.grid(row=0, column=1, sticky="n", padx=(12, 0))
        ttk.Label(right, text="CÁMARAS (pelota + arcos)").pack(anchor="w")
        self.cam_txt = tk.Text(right, width=46, height=10, font=("Consolas", 10),
                               bg="#0c0f12", fg="#d8e0e8", relief="flat")
        self.cam_txt.pack(pady=(0, 6))
        ttk.Label(right, text="ToF (profundidad mínima por dirección)").pack(anchor="w")
        self.tof_txt = tk.Text(right, width=46, height=7, font=("Consolas", 10),
                               bg="#0c0f12", fg="#d8e0e8", relief="flat")
        self.tof_txt.pack(pady=(0, 6))
        self.status = ttk.Label(self.root, text="iniciando…", anchor="w",
                                relief="sunken", padding=4)
        self.status.pack(fill="x", side="bottom")

    def _draw_static(self) -> None:
        c = self.canvas
        for r_mm in RANGE_RINGS_MM:
            r = r_mm * self._scale
            c.create_oval(self._cx - r, self._cy - r, self._cx + r, self._cy + r, outline="#1d3a2a")
            c.create_text(self._cx + 3, self._cy - r - 7, text=f"{r_mm/1000:.1f}m",
                          fill="#2f5a44", font=("Consolas", 7), anchor="w")
        c.create_polygon(self._cx, self._cy - 14, self._cx - 10, self._cy + 10,
                         self._cx + 10, self._cy + 10, fill="#2b6", outline="#7fd")
        # Capas dinámicas (se borran/redibujan por frame con tags).
        # tags: tof_cone, tof_zone, cam_cone, ball, goal, us

    def _tick(self) -> None:
        frames = self.source.poll()
        for f in frames:
            self.frame_count += 1
            if self.recorder is not None:
                self.recorder.write(f)
            self.last = f
        if frames:
            self._render(frames[-1])
        elif self.last is None:
            self._set_status("esperando datos… (¿flasheaste un env con monitor y conectaste la TOP?)")
        self.root.after(self.poll_ms, self._tick)

    def _render(self, f: TopFrame) -> None:
        c = self.canvas
        c.delete("dyn")     # borra todo lo dinámico de la pasada anterior
        self._draw_tof_cones(f)
        self._draw_cam_cones(f)
        self._draw_ball(f)
        self._render_panels(f)
        self._set_status(f"src OK · seq={f.seq} · frames={self.frame_count} · v{f.v} · "
                         f"zonas ToF: {'sí' if f.tof.zones else 'pendiente firmware'}")

    def _draw_tof_cones(self, f: TopFrame) -> None:
        c = self.canvas
        zones = f.tof.zones
        for idx in range(4):
            if not self.cfg.sensor_enabled.get(idx, True):
                continue
            pos = self.cfg.position.get(idx, "FRONT")
            bearing = position_bearing(pos)
            # Cono (sector) en gris tenue.
            half = TOF_HFOV_DEG / 2.0
            e1 = polar_xy(bearing - half, MAX_RANGE_MM, self._scale, self._cx, self._cy)
            e2 = polar_xy(bearing + half, MAX_RANGE_MM, self._scale, self._cx, self._cy)
            c.create_line(self._cx, self._cy, *e1, fill="#1b3a4a", tags="dyn")
            c.create_line(self._cx, self._cy, *e2, fill="#1b3a4a", tags="dyn")
            lab = polar_xy(bearing, MAX_RANGE_MM * 0.92, self._scale, self._cx, self._cy)
            c.create_text(*lab, text=f"{idx}·{pos[:1]}", fill="#3a6a82",
                          font=("Consolas", 8), tags="dyn")
            # Zonas: profundidad de cada una.
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
            # Arcos que ve esta cámara.
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
        # Vector de velocidad de la pelota.
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
        # Profundidad mínima por dirección (de las zonas, si vienen; si no, la distancia única).
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

    def _set_status(self, text: str) -> None:
        self.status.config(text=text)

    def _on_close(self) -> None:
        try:
            self.source.stop()
            if self.recorder is not None:
                self.recorder.close()
        finally:
            self.root.destroy()


def run_polar(source: FrameSource, recorder=None, config_path: Optional[str] = None) -> None:
    root = tk.Tk()
    PolarApp(root, source, recorder=recorder, config_path=config_path)
    root.mainloop()


def selftest(frames: int = 200) -> int:
    """Smoke headless: corre frames del sim por el parser y ejercita la geometría
    pura (conos, profundidad por zona) sin abrir ventana. Devuelve 0 si OK."""
    import sys
    from .protocol_top import parse_line_top
    from .simulator_top import SimulatorTop
    if hasattr(sys.stdout, "reconfigure"):
        try:
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        except Exception:  # noqa: BLE001
            pass
    cfg = load_or_default()
    sim = SimulatorTop(rate_hz=20.0)
    last = None
    n_points = 0
    for _ in range(frames):
        last = parse_line_top(sim.next_line())
        if last.tof.zones:
            for idx in range(min(4, len(last.tof.zones))):
                bearing = position_bearing(cfg.position.get(idx, "FRONT"))
                disp = cfg.oriented_cells(last.tof.zones[idx], idx)
                n_points += len(zone_polar_points(disp, bearing, cfg.zone_enabled.get(idx)))
    assert last is not None
    # Chequeo de geometría: las 4 columnas reparten el FOV centrado en el bearing.
    az = column_azimuths(0.0)
    span_ok = abs((az[-1] - az[0]) - TOF_HFOV_DEG * 3 / 4) < 1e-6
    print(f"[selftest-polar] frames procesados : {frames}")
    print(f"[selftest-polar] último seq         : {last.seq}")
    print(f"[selftest-polar] zonas ToF          : {'presentes' if last.tof.zones else 'no (pendiente firmware)'}")
    print(f"[selftest-polar] puntos de zona     : {n_points} (profundidades dibujables)")
    print(f"[selftest-polar] ¿ve pelota la cam? : {last.cam.ball_visible}")
    print(f"[selftest-polar] reparto de columnas: {'OK' if span_ok else 'FALLO'}")
    if not span_ok:
        print("[selftest-polar] FALLO")
        return 1
    print("[selftest-polar] OK")
    return 0
