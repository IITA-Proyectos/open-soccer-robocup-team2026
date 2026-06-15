"""gui_field.py — Vista de CANCHA (X-Y fija) de la placa TOP.

Dibuja la cancha completa y, dentro, el ROBOT en su posición con orientación + la
ESTELA de su recorrido, y la PELOTA con su estela. Convención de cancha (memoria
field_axis_convention): +X = lateral (ancho, 1820), +Y = arco-a-arco (largo, 2430).

PRINCIPIO (corrección María 2026-06-15): la app MUESTRA lo que el TOP ya calcula
y manda a CENTRAL — NO recalcula nada. Lo único que calcula es la ESTELA histórica.
  • Pose del robot = `snap.my_x/y/heading` del WorldSnapshot (lo que el TOP envía a
    CENTRAL; main_top.cpp:176-200). Se dibuja tal cual; si `snap.valid` es False se
    avisa, no se inventa.
  • La pelota viene RELATIVA al robot en el snapshot (snap.ball_x/y =
    cameras_get_ball_*; main_top.cpp:205-206). Para ubicarla en la cancha se la rota
    con la PROPIA pose del snapshot (no se recalcula la pose; sólo se compone el
    relativo con lo que ya manda el TOP).
  • La calidad de la pose/pelota depende de la calibración del firmware (homografía,
    TASK-022). La app es FIEL al dato que recibe CENTRAL; si está sin calibrar, se ve
    sin calibrar (justamente sirve para diagnosticar eso). No lo corrige ni lo maquilla.

Sin display: `--field --selftest`.
"""
from __future__ import annotations

import math
import tkinter as tk
from collections import deque
from tkinter import ttk
from typing import Deque, List, Optional, Tuple

from .protocol_top import TopFrame
from .sources import FrameSource
from .tof_layout import load_or_default

FIELD_W_MM = 1820        # ancho (X, lateral)
FIELD_H_MM = 2430        # largo (Y, arco-a-arco)
GOAL_W_MM = 450          # ancho de boca de arco (aprox, solo dibujo)
MARGIN_PX = 26
TRAIL_MAX = 400


# ── Geometría PURA (testeable, sin Tk) ──────────────────────────────────────
def robot_rel_to_field(rel_x_mm: float, rel_y_mm: float,
                       robot_x_mm: float, robot_y_mm: float,
                       heading_deg: float) -> Tuple[float, float]:
    """Pasa una detección RELATIVA al robot (rel_x=+derecha, rel_y=+frente) a
    coordenadas de CANCHA, dada la pose del robot. Heading = CW+ desde +Y (igual
    convención que el radar: 0°=frente apunta a +Y)."""
    a = math.radians(heading_deg)
    fx, fy = math.sin(a), math.cos(a)        # vector "frente" del robot en cancha
    rx, ry = math.cos(a), -math.sin(a)       # vector "derecha" del robot en cancha
    return (robot_x_mm + rel_y_mm * fx + rel_x_mm * rx,
            robot_y_mm + rel_y_mm * fy + rel_x_mm * ry)


def field_to_px(x_mm: float, y_mm: float, draw_w: float, draw_h: float,
                margin: float = MARGIN_PX,
                field_w: float = FIELD_W_MM, field_h: float = FIELD_H_MM
                ) -> Tuple[float, float]:
    """Cancha (X 0..ancho, Y 0..largo) → píxel. +Y arriba en pantalla (Y=largo
    arriba)."""
    px = margin + (x_mm / field_w) * draw_w
    py = margin + ((field_h - y_mm) / field_h) * draw_h
    return px, py


class Trail:
    """Estela: buffer acotado de posiciones (x,y) en mm de cancha."""
    def __init__(self, maxlen: int = TRAIL_MAX):
        self._d: Deque[Tuple[float, float]] = deque(maxlen=maxlen)

    def push(self, x: float, y: float) -> None:
        # No re-agrega si casi no se movió (evita acumular ruido parado).
        if self._d:
            lx, ly = self._d[-1]
            if abs(lx - x) < 2 and abs(ly - y) < 2:
                return
        self._d.append((x, y))

    def points(self) -> List[Tuple[float, float]]:
        return list(self._d)

    def clear(self) -> None:
        self._d.clear()

    def __len__(self) -> int:
        return len(self._d)


# ── GUI ─────────────────────────────────────────────────────────────────────
class FieldApp:
    def __init__(self, root: tk.Tk, source: FrameSource, poll_ms: int = 80,
                 recorder=None, config_path: Optional[str] = None):
        self.root = root
        self.source = source
        self.poll_ms = poll_ms
        self.recorder = recorder
        cfg = load_or_default(config_path)
        self.field_w = float(cfg.wall.field_width_mm or FIELD_W_MM)
        self.field_h = float(cfg.wall.field_height_mm or FIELD_H_MM)
        self.last: Optional[TopFrame] = None
        self.frame_count = 0
        self._is_sim = getattr(source, "is_sim", False)
        self.robot_trail = Trail()
        self.ball_trail = Trail()

        # Tamaño de dibujo (mantiene proporción de la cancha).
        self.draw_h = 560.0
        self.draw_w = self.draw_h * self.field_w / self.field_h
        self.cw = self.draw_w + 2 * MARGIN_PX
        self.ch = self.draw_h + 2 * MARGIN_PX

        root.title("IITA Soccer — Vista de CANCHA (TOP) — " + source.describe())
        self._build_layout()
        self._draw_field()
        self.source.start()
        self.root.after(self.poll_ms, self._tick)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_layout(self) -> None:
        if self._is_sim:
            tk.Label(self.root, text="⚠ SIMULADOR — datos FALSOS, no es el robot",
                     bg="#b32d00", fg="#ffe27a", font=("Segoe UI", 12, "bold"), pady=5).pack(fill="x")
        tk.Label(self.root, bg="#11151a", fg="#ffd27a", anchor="w", padx=8, pady=3,
                 font=("Segoe UI", 9),
                 text="Muestra la POSE que el TOP manda a CENTRAL (snapshot my_x/y/hdg) tal cual. "
                      "La app solo agrega la ESTELA. Pelota = relativa del snapshot ubicada con esa pose.").pack(fill="x")
        main = ttk.Frame(self.root, padding=8); main.pack(fill="both", expand=True)
        self.canvas = tk.Canvas(main, width=self.cw, height=self.ch,
                                bg="#0c3a18", highlightthickness=0)
        self.canvas.grid(row=0, column=0, sticky="n")
        side = ttk.Frame(main); side.grid(row=0, column=1, sticky="n", padx=(10, 0))
        self.info = tk.Text(side, width=40, height=12, font=("Consolas", 10),
                            bg="#0c0f12", fg="#d8e0e8", relief="flat")
        self.info.pack()
        ttk.Button(side, text="↺ Borrar estelas", command=self._clear_trails).pack(pady=6, anchor="w")
        self.status = ttk.Label(self.root, text="iniciando…", anchor="w",
                                relief="sunken", padding=4)
        self.status.pack(fill="x", side="bottom")

    def _fpx(self, x_mm: float, y_mm: float) -> Tuple[float, float]:
        return field_to_px(x_mm, y_mm, self.draw_w, self.draw_h,
                           field_w=self.field_w, field_h=self.field_h)

    def _draw_field(self) -> None:
        c = self.canvas
        x0, y0 = self._fpx(0, 0)
        x1, y1 = self._fpx(self.field_w, self.field_h)
        c.create_rectangle(x0, y1, x1, y0, outline="#cfe", width=2)            # límite
        cxmid, _ = self._fpx(self.field_w / 2, 0)
        _, ymid = self._fpx(0, self.field_h / 2)
        c.create_line(x0, ymid, x1, ymid, fill="#3a7a4a")                      # media cancha
        r = (self.field_w * 0.18) / self.field_w * self.draw_w
        c.create_oval(cxmid - r, ymid - r, cxmid + r, ymid + r, outline="#3a7a4a")
        # Arcos (Y=largo = arriba, Y=0 = abajo).
        gx0, _ = self._fpx(self.field_w / 2 - GOAL_W_MM / 2, 0)
        gx1, _ = self._fpx(self.field_w / 2 + GOAL_W_MM / 2, 0)
        c.create_line(gx0, y0, gx1, y0, fill="#4a78e6", width=5)               # arco abajo (azul)
        c.create_line(gx0, y1, gx1, y1, fill="#e6d44a", width=5)               # arco arriba (amarillo)
        # Capa dinámica via tag "dyn".

    def _tick(self) -> None:
        frames = self.source.poll()
        for f in frames:
            self.frame_count += 1
            if self.recorder is not None:
                self.recorder.write(f)
            self.last = f
            self._accumulate(f)
        if frames:
            self._render(frames[-1])
        elif self.last is None:
            self._set_status("esperando datos… (¿flasheaste un env con monitor y conectaste la TOP?)")
        self.root.after(self.poll_ms, self._tick)

    def _accumulate(self, f: TopFrame) -> None:
        # Pose = lo que el TOP manda a CENTRAL (snapshot). NO se recalcula.
        if f.snap.valid:
            self.robot_trail.push(float(f.snap.my_x_mm), float(f.snap.my_y_mm))
            if f.snap.ball_visible:
                bx, by = robot_rel_to_field(f.snap.ball_x_mm, f.snap.ball_y_mm,
                                            f.snap.my_x_mm, f.snap.my_y_mm,
                                            f.snap.my_heading_deg)
                self.ball_trail.push(bx, by)

    def _render(self, f: TopFrame) -> None:
        c = self.canvas
        c.delete("dyn")
        # Estela del robot.
        pts = self.robot_trail.points()
        if len(pts) >= 2:
            flat = []
            for x, y in pts:
                px, py = self._fpx(x, y)
                flat += [px, py]
            c.create_line(*flat, fill="#2bd27a", width=2, tags="dyn")
        # Estela de la pelota (punteada = distancia sin calibrar).
        bpts = self.ball_trail.points()
        if len(bpts) >= 2:
            flat = []
            for x, y in bpts:
                px, py = self._fpx(x, y)
                flat += [px, py]
            c.create_line(*flat, fill="#c9821f", width=1, dash=(3, 3), tags="dyn")
        # Robot en la pose del snapshot (lo que el TOP manda a CENTRAL).
        if f.snap.valid:
            self._draw_robot(f.snap.my_x_mm, f.snap.my_y_mm, f.snap.my_heading_deg)
            # Pelota: relativa del snapshot, ubicada con la pose del snapshot.
            if f.snap.ball_visible:
                bx, by = robot_rel_to_field(f.snap.ball_x_mm, f.snap.ball_y_mm,
                                            f.snap.my_x_mm, f.snap.my_y_mm,
                                            f.snap.my_heading_deg)
                px, py = self._fpx(bx, by)
                c.create_oval(px - 7, py - 7, px + 7, py + 7, fill="#ff8c2a",
                              outline="#ffd", tags="dyn")
        self._render_info(f)

    def _draw_robot(self, x_mm: float, y_mm: float, heading_deg: float) -> None:
        c = self.canvas
        px, py = self._fpx(x_mm, y_mm)
        a = math.radians(heading_deg)
        # Frente = +Y a heading 0; vector frente en PX (Y invertido en pantalla).
        L = 16
        fx, fy = math.sin(a), math.cos(a)
        nose = (px + fx * L, py - fy * L)
        # Base del triángulo (perpendicular).
        bx_, by_ = math.cos(a), -math.sin(a)
        bl = (px - fx * 7 + bx_ * 9, py + fy * 7 - by_ * 9)
        br = (px - fx * 7 - bx_ * 9, py + fy * 7 + by_ * 9)
        c.create_polygon(*nose, *bl, *br, fill="#2b6", outline="#bfe", tags="dyn")
        c.create_oval(px - 2, py - 2, px + 2, py + 2, fill="#bfe", outline="", tags="dyn")

    def _render_info(self, f: TopFrame) -> None:
        def yn(b):
            return "SÍ" if b else "no"
        s = f.snap
        if s.valid:
            pose = (f"x={s.my_x_mm:+5d}  y={s.my_y_mm:+5d}  "
                    f"hdg={s.my_heading_deg:+.1f}°  conf={s.my_confidence}")
            snapst = "snapshot VÁLIDO ✓ (lo que va a CENTRAL)"
        else:
            pose = "—  (snapshot no válido)"
            snapst = "⚠ snapshot NO válido (el TOP todavía no manda pose)"
        self._set_text(self.info,
            f"SNAPSHOT→CENTRAL : {snapst}\n"
            f"pose robot      : {pose}\n"
            f"heading válido  : {yn(f.imu.heading_valid)}\n"
            f"estela robot    : {len(self.robot_trail)} puntos\n"
            f"\n"
            f"¿PELOTA?        : {yn(s.ball_visible)}\n"
            f"pelota (rel.)   : x={s.ball_x_mm:+5d} y={s.ball_y_mm:+5d}\n"
            f"estela pelota   : {len(self.ball_trail)} puntos\n"
            f"\n"
            f"cancha          : {int(self.field_w)} × {int(self.field_h)} mm\n"
            f"frames          : {self.frame_count}\n")

    def _clear_trails(self) -> None:
        self.robot_trail.clear()
        self.ball_trail.clear()
        self._set_status("estelas borradas")

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


def run_field(source: FrameSource, recorder=None, config_path: Optional[str] = None) -> None:
    root = tk.Tk()
    FieldApp(root, source, recorder=recorder, config_path=config_path)
    root.mainloop()


def selftest(frames: int = 200) -> int:
    """Smoke headless: corre frames del sim, acumula estelas por OTOS y ejercita
    la transformación relativo→cancha sin abrir ventana. Devuelve 0 si OK."""
    import sys
    from .protocol_top import parse_line_top
    from .simulator_top import SimulatorTop
    if hasattr(sys.stdout, "reconfigure"):
        try:
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        except Exception:  # noqa: BLE001
            pass
    sim = SimulatorTop(rate_hz=20.0)
    rtrail, btrail = Trail(), Trail()
    last = None
    valid_seen = False
    for _ in range(frames):
        last = parse_line_top(sim.next_line())
        if last.snap.valid:
            valid_seen = True
            rtrail.push(float(last.snap.my_x_mm), float(last.snap.my_y_mm))
            if last.snap.ball_visible:
                bx, by = robot_rel_to_field(last.snap.ball_x_mm, last.snap.ball_y_mm,
                                            last.snap.my_x_mm, last.snap.my_y_mm,
                                            last.snap.my_heading_deg)
                btrail.push(bx, by)
    assert last is not None
    # Chequeo de la transformación: a heading 0, 'al frente' (rel_y) suma a +Y.
    fx, fy = robot_rel_to_field(0, 500, 100, 100, 0.0)
    geom_ok = abs(fx - 100) < 1e-6 and abs(fy - 600) < 1e-6
    print(f"[selftest-field] frames procesados : {frames}")
    print(f"[selftest-field] snapshot válido    : {valid_seen}")
    print(f"[selftest-field] estela robot/pelota: {len(rtrail)} / {len(btrail)} puntos")
    print(f"[selftest-field] transformación rel→cancha: {'OK' if geom_ok else 'FALLO'}")
    if not geom_ok:
        print("[selftest-field] FALLO")
        return 1
    print("[selftest-field] OK")
    return 0
