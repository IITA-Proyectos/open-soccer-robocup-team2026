"""gui_cam_fusion.py — VISTA V6: COMPARADOR DE FUSIÓN DE CÁMARA (anti-pelota-fantasma).

Pone lado a lado lo que ve la cámara FRONTAL (camf), la TRASERA (camb) y el
resultado FUSIONADO (cam): pelota (x/y), arco amarillo y arco azul (visible +
ángulo + distancia). El punto de la vista es delatar la "PELOTA FANTASMA": cuando
las dos cámaras ven la pelota en lugares MUY distintos, la fusión las promedia y
teletransporta la pelota a un punto donde NO está (bug real conocido del
`fuse_ball_dual`). Cuando |camf.ball − camb.ball| supera un umbral con AMBAS
cámaras viéndola, la vista lo RESALTA EN ROJO y dicta el veredicto
"FUSIÓN SOSPECHOSA".

La lógica de decisión es PURA y vive acá mismo (ball_disagreement_mm / is_phantom
/ fusion_verdict), host-testeada en tests/test_cam_fusion.py. Esta capa de Tk solo
dibuja. Reusa el pipeline ya testeado: FrameSource → poll() → parser v2 (camf/camb
son CamPer; cam es Cam). Sin display: usar `--cam-fusion --selftest`.
"""
from __future__ import annotations

import math
import tkinter as tk
from tkinter import ttk
from typing import Optional

from .protocol_top import CamPer, TopFrame
from .sources import FrameSource

# Umbral por defecto (mm) de desacuerdo de pelota entre cámaras para gritar
# "fantasma". 400 mm ≈ media cancha de ancho de campo de juego; con ambas
# cámaras viendo la pelota, un delta así sólo puede ser una alucinación de una.
DEFAULT_PHANTOM_THRESH_MM = 400.0

# Colores de veredicto (texto sobre fondo del header).
OK_COLOR = "#2e7d32"        # acuerdo: verde
PHANTOM_COLOR = "#c0392b"   # sospecha: rojo
IDLE_COLOR = "#555a60"      # no se puede juzgar (alguna no ve): gris


# ── Lógica PURA (host-testeada) ──────────────────────────────────────────────

def ball_disagreement_mm(camf: CamPer, camb: CamPer) -> Optional[float]:
    """Distancia euclídea (mm) entre la pelota que ve la frontal y la que ve la
    trasera. None si ALGUNA de las dos NO ve la pelota (no hay nada que comparar:
    el desacuerdo no está definido). Con ambas visibles, un valor grande = las
    cámaras discrepan → la fusión (promedio) inventa una posición intermedia."""
    if not (camf.ball_visible and camb.ball_visible):
        return None
    dx = float(camf.ball_x_mm - camb.ball_x_mm)
    dy = float(camf.ball_y_mm - camb.ball_y_mm)
    return math.hypot(dx, dy)


def is_phantom(camf: CamPer, camb: CamPer,
               thresh_mm: float = DEFAULT_PHANTOM_THRESH_MM) -> bool:
    """True si AMBAS cámaras ven la pelota PERO en posiciones que difieren más de
    `thresh_mm`: el clásico de la pelota fantasma (el promedio fusionado cae en un
    punto donde ninguna cámara vio nada). False si alguna no la ve (no juzgable) o
    si concuerdan."""
    d = ball_disagreement_mm(camf, camb)
    return d is not None and d > thresh_mm


def fusion_verdict(camf: CamPer, camb: CamPer,
                   thresh_mm: float = DEFAULT_PHANTOM_THRESH_MM) -> str:
    """Veredicto textual de la fusión. Uno de:
      • "FUSIÓN SOSPECHOSA" — ambas ven la pelota y discrepan > umbral (fantasma).
      • "FUSIÓN OK"         — ambas ven la pelota y concuerdan.
      • "SIN COMPARACIÓN"   — alguna no ve la pelota (no hay desacuerdo definido)."""
    d = ball_disagreement_mm(camf, camb)
    if d is None:
        return "SIN COMPARACIÓN"
    return "FUSIÓN SOSPECHOSA" if d > thresh_mm else "FUSIÓN OK"


# ── GUI (Tk) — no se testea ───────────────────────────────────────────────────

def _goal_text(visible: bool, ang: float, dist: int) -> str:
    if not visible:
        return "no visible"
    return f"{ang:+6.1f}°  {dist:5d} mm"


class CamFusionApp:
    def __init__(self, root: tk.Tk, source: FrameSource, poll_ms: int = 100,
                 recorder=None, thresh_mm: float = DEFAULT_PHANTOM_THRESH_MM):
        self.root = root
        self.source = source
        self.poll_ms = poll_ms
        self.recorder = recorder
        self.thresh_mm = thresh_mm
        self.last: Optional[TopFrame] = None
        self.frame_count = 0
        self.last_seq = -1
        self.dropped = 0
        self.phantom_count = 0
        self._is_sim = getattr(source, "is_sim", False)
        self._last_board: Optional[str] = None
        # Widgets por columna (camf / camb / cam fusionado) y celdas.
        self._cols: dict = {}

        root.title("IITA Soccer — Fusión de cámara (anti-fantasma) — " + source.describe())
        self._build_layout()
        self.source.start()
        self.root.after(self.poll_ms, self._tick)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ── Layout ────────────────────────────────────────────────────────────
    def _build_layout(self) -> None:
        if self._is_sim:
            tk.Label(self.root, text="⚠ SIMULADOR — datos FALSOS, no es el robot",
                     bg="#b32d00", fg="#ffe27a",
                     font=("Segoe UI", 12, "bold"), pady=5).pack(fill="x")

        # Header: seq / Hz / contadores. width fijo (chars) anti-parpadeo.
        self.header = tk.Label(self.root, text="iniciando…", anchor="w", width=92,
                               font=("Consolas", 11, "bold"),
                               bg="#11151a", fg="#cfe", padx=8, pady=6)
        self.header.pack(fill="x")

        # Veredicto grande (cambia de color cuando hay fantasma).
        self.verdict = tk.Label(self.root, text="esperando…", anchor="center",
                                font=("Segoe UI", 16, "bold"),
                                fg="#fff", bg=IDLE_COLOR, pady=8)
        self.verdict.pack(fill="x")

        body = ttk.Frame(self.root, padding=8)
        body.pack(fill="both", expand=True)

        # Tres columnas lado a lado: FRONTAL | TRASERA | FUSIONADO.
        self._cols["camf"] = self._make_col(body, 0, "CÁMARA FRONTAL", "#1b3a5b")
        self._cols["camb"] = self._make_col(body, 1, "CÁMARA TRASERA", "#3a2b5b")
        self._cols["cam"] = self._make_col(body, 2, "FUSIONADO (cam)", "#2b3b2b")
        for c in range(3):
            body.columnconfigure(c, weight=1, uniform="cols")

        self.status = ttk.Label(self.root, text="", anchor="w",
                                relief="sunken", padding=4)
        self.status.pack(fill="x", side="bottom")

    def _make_col(self, parent: ttk.Frame, col: int, title: str, bg: str) -> dict:
        # width/height fijos + pack_propagate(False): el texto cambia cada frame
        # (coords de pelota); sin esto la columna se redimensiona y la ventana
        # parpadea (mismo bug que gui_top_health 2026-06-14).
        frame = tk.Frame(parent, bd=1, relief="solid", bg=bg,
                         width=210, height=190, padx=8, pady=6)
        frame.grid(row=0, column=col, padx=4, pady=2, sticky="nsew")
        frame.pack_propagate(False)
        tk.Label(frame, text=title, font=("Segoe UI", 10, "bold"),
                 fg="#fff", bg=bg, anchor="w").pack(anchor="w")

        ball = tk.Label(frame, font=("Consolas", 11, "bold"), fg="#ffe27a",
                        bg=bg, anchor="w", justify="left")
        ball.pack(anchor="w", pady=(6, 2))
        yellow = tk.Label(frame, font=("Consolas", 9), fg="#ffd24d",
                          bg=bg, anchor="w", justify="left")
        yellow.pack(anchor="w")
        blue = tk.Label(frame, font=("Consolas", 9), fg="#6db3ff",
                        bg=bg, anchor="w", justify="left")
        blue.pack(anchor="w")
        return {"frame": frame, "title": title, "bg": bg,
                "ball": ball, "yellow": yellow, "blue": blue}

    # ── Loop ──────────────────────────────────────────────────────────────
    def _tick(self) -> None:
        frames = self.source.poll()
        for f in frames:
            self._consume(f)
        if hasattr(self.source, "last_text"):
            bt = self.source.last_text()
            if bt:
                self._last_board = bt
        had_error = self._drain_errors()
        if frames:
            self._render(frames[-1])
        elif not had_error and self.last is None:
            msg = "esperando datos…"
            if self._last_board:
                msg += f" la placa dice: «{self._last_board}»"
            self._set_status(msg)
        self.root.after(self.poll_ms, self._tick)

    def _consume(self, f: TopFrame) -> None:
        self.frame_count += 1
        if self.last_seq >= 0:
            gap = f.seq - self.last_seq - 1
            if gap > 0:
                self.dropped += gap
        self.last_seq = f.seq
        if is_phantom(f.camf, f.camb, self.thresh_mm):
            self.phantom_count += 1
        if self.recorder is not None:
            self.recorder.write(f)
        self.last = f

    def _drain_errors(self) -> bool:
        msgs = []
        while True:
            try:
                msgs.append(self.source.errors.get_nowait())
            except Exception:  # noqa: BLE001
                break
        if msgs:
            self._set_status("⚠ " + " | ".join(msgs[-2:]))
            return True
        return False

    def _render(self, f: TopFrame) -> None:
        self._render_per(self._cols["camf"], f.camf)
        self._render_per(self._cols["camb"], f.camb)
        self._render_fused(self._cols["cam"], f)

        d = ball_disagreement_mm(f.camf, f.camb)
        verdict = fusion_verdict(f.camf, f.camb, self.thresh_mm)
        phantom = is_phantom(f.camf, f.camb, self.thresh_mm)

        if d is None:
            color = IDLE_COLOR
            sub = "alguna cámara no ve la pelota"
        elif phantom:
            color = PHANTOM_COLOR
            sub = f"Δpelota = {d:.0f} mm  >  umbral {self.thresh_mm:.0f} mm"
        else:
            color = OK_COLOR
            sub = f"Δpelota = {d:.0f} mm  ≤  umbral {self.thresh_mm:.0f} mm"
        self.verdict.configure(text=f"{verdict}   ·   {sub}", bg=color)

        # Resaltar en ROJO la pelota de las DOS cámaras cuando es fantasma.
        ball_fg = "#ff6b5b" if phantom else "#ffe27a"
        self._cols["camf"]["ball"].configure(fg=ball_fg)
        self._cols["camb"]["ball"].configure(fg=ball_fg)

        self.header.configure(
            text=(f"seq={f.seq}  frames={self.frame_count}  perdidos={self.dropped}  "
                  f"v{f.v}     fantasmas={self.phantom_count}  "
                  f"umbral={self.thresh_mm:.0f}mm"))
        self._set_status(f"src OK · {self.source.describe()}")

    def _render_per(self, col: dict, cam: CamPer) -> None:
        if cam.ball_visible:
            col["ball"].configure(text=f"PELOTA  x={cam.ball_x_mm:+5d}  y={cam.ball_y_mm:+5d}")
        else:
            col["ball"].configure(text="PELOTA  — no la ve —")
        col["yellow"].configure(
            text="AMA  " + _goal_text(cam.yellow_visible, cam.yellow_angle_deg,
                                      cam.yellow_distance_mm))
        col["blue"].configure(
            text="AZU  " + _goal_text(cam.blue_visible, cam.blue_angle_deg,
                                      cam.blue_distance_mm))

    def _render_fused(self, col: dict, f: TopFrame) -> None:
        c = f.cam
        if c.ball_visible:
            col["ball"].configure(text=f"PELOTA  x={c.ball_x_mm:+5d}  y={c.ball_y_mm:+5d}")
        else:
            col["ball"].configure(text="PELOTA  — no la ve —")
        col["ball"].configure(fg="#ffe27a")  # el fusionado no se resalta
        col["yellow"].configure(
            text="AMA  " + _goal_text(c.yellow_visible, c.yellow_angle_deg,
                                      c.yellow_distance_mm))
        col["blue"].configure(
            text="AZU  " + _goal_text(c.blue_visible, c.blue_angle_deg,
                                      c.blue_distance_mm))

    # ── Helpers ───────────────────────────────────────────────────────────
    def _set_status(self, text: str) -> None:
        self.status.config(text=text)

    def _on_close(self) -> None:
        try:
            self.source.stop()
            if self.recorder is not None:
                self.recorder.close()
        finally:
            self.root.destroy()


def run_cam_fusion(source: FrameSource, recorder=None) -> None:
    """Crea el Tk root y corre la vista (igual que run_top_health)."""
    root = tk.Tk()
    CamFusionApp(root, source, recorder=recorder)
    root.mainloop()


def selftest(frames: int = 200) -> int:
    """Smoke headless de la vista V6: corre N frames del simulador TOP por el
    parser real (camf/camb son CamPer) y ejercita la lógica pura de fusión SIN
    abrir ventana. Devuelve 0 si todo anduvo.

    Verifica que: (a) el pipeline parsea, (b) la lógica corre sobre datos reales
    del sim, (c) cuando AMBAS cámaras ven la pelota es coherente con el veredicto.
    Inyecta además un par sintético fantasma para confirmar que el detector dispara."""
    import sys

    from .simulator_top import SimulatorTop

    # Consolas Windows (cp1252) no banca °/Δ → salida UTF-8 tolerante.
    if hasattr(sys.stdout, "reconfigure"):
        try:
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        except Exception:  # noqa: BLE001
            pass

    from .protocol_top import parse_line_top

    sim = SimulatorTop(rate_hz=20.0)
    last = None
    both_seen = 0
    phantom_seen = 0
    disagree_max = 0.0
    for _ in range(frames):
        last = parse_line_top(sim.next_line())
        d = ball_disagreement_mm(last.camf, last.camb)
        if d is not None:
            both_seen += 1
            disagree_max = max(disagree_max, d)
            if is_phantom(last.camf, last.camb):
                phantom_seen += 1
    assert last is not None

    # Par sintético fantasma: una a +1000, otra a −1000 → delta enorme → phantom.
    ghost_f = CamPer(ball_visible=True, ball_x_mm=1000, ball_y_mm=0,
                     yellow_visible=False, yellow_angle_deg=0.0, yellow_distance_mm=0,
                     blue_visible=False, blue_angle_deg=0.0, blue_distance_mm=0)
    ghost_b = CamPer(ball_visible=True, ball_x_mm=-1000, ball_y_mm=0,
                     yellow_visible=False, yellow_angle_deg=0.0, yellow_distance_mm=0,
                     blue_visible=False, blue_angle_deg=0.0, blue_distance_mm=0)
    ghost_d = ball_disagreement_mm(ghost_f, ghost_b)
    ghost_is_phantom = is_phantom(ghost_f, ghost_b)
    ghost_verdict = fusion_verdict(ghost_f, ghost_b)

    print(f"[selftest-cam-fusion] frames procesados   : {frames}")
    print(f"[selftest-cam-fusion] último seq           : {last.seq}")
    print(f"[selftest-cam-fusion] frames con ambas pelota: {both_seen}")
    print(f"[selftest-cam-fusion] Δpelota máx (mm)      : {disagree_max:.0f}")
    print(f"[selftest-cam-fusion] fantasmas en sim      : {phantom_seen}")
    print(f"[selftest-cam-fusion] par sintético fantasma: Δ={ghost_d:.0f}mm "
          f"phantom={ghost_is_phantom} «{ghost_verdict}»")
    if not (ghost_is_phantom and ghost_verdict == "FUSIÓN SOSPECHOSA"
            and ghost_d == 2000.0):
        print("[selftest-cam-fusion] FALLO: el detector de fantasma no disparó")
        return 1
    print("[selftest-cam-fusion] OK")
    return 0
