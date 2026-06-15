"""panel_cam_fusion.py — Panel COMPARADOR DE FUSIÓN DE CÁMARA (anti-pelota-fantasma).

Refactor de la VISTA V6 (gui_cam_fusion) a Panel embebible: pone lado a lado lo
que ve la cámara FRONTAL (camf), la TRASERA (camb) y el resultado FUSIONADO (cam),
con el veredicto FUSIÓN OK / SOSPECHOSA / SIN COMPARACIÓN. El punto es delatar la
"PELOTA FANTASMA": cuando las dos cámaras ven la pelota en lugares MUY distintos,
la fusión las promedia y la teletransporta a un punto donde NO está (bug real del
`fuse_ball_dual`). Cuando ambas la ven y discrepan más del umbral, el veredicto se
pone en ROJO y la pelota de ambas cámaras se resalta.

Sin ventana/loop/fuente propios (el shell se los provee). La LÓGICA PURA
(ball_disagreement_mm / is_phantom / fusion_verdict) se IMPORTA de gui_cam_fusion
(ya host-testeada en tests/test_cam_fusion.py); acá solo está la capa Tk.
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk

from .gui_cam_fusion import (
    DEFAULT_PHANTOM_THRESH_MM,
    ball_disagreement_mm,
    fusion_verdict,
    is_phantom,
)
from .panel import Panel
from .protocol_top import TopFrame
from .shell_theme import BAD, BG, FG, MUTED, OK

# Veredicto: colores tomados de la paleta del shell (consola industrial).
OK_COLOR = OK
PHANTOM_COLOR = BAD
IDLE_COLOR = MUTED

BALL_FG = "#ffe27a"        # pelota normal (amarillo golf)
BALL_FG_PHANTOM = "#ff6b5b"  # pelota resaltada cuando hay fantasma


def _goal_text(visible: bool, ang: float, dist: int) -> str:
    if not visible:
        return "no visible"
    return f"{ang:+6.1f}°  {dist:5d} mm"


class CamFusionPanel(Panel):
    title = "Cámara"
    key = "cam"
    icon = "◳"
    sends_commands = False

    def build(self, parent: tk.Frame) -> None:
        # Estado de la instancia (antes vivía en CamFusionApp).
        self.thresh_mm: float = DEFAULT_PHANTOM_THRESH_MM
        self.frame_count = 0
        self.last_seq = -1
        self.dropped = 0
        self.phantom_count = 0
        self._cols: dict = {}

        # Header: seq / frames / perdidos / fantasmas / umbral. width fijo
        # (chars) anti-parpadeo.
        self.header = tk.Label(parent, text="esperando datos…", anchor="w", width=92,
                               font=("Consolas", 11, "bold"),
                               bg=BG, fg=FG, padx=8, pady=6)
        self.header.pack(fill="x")

        # Veredicto grande (cambia de color cuando hay fantasma).
        self.verdict = tk.Label(parent, text="esperando…", anchor="center",
                                font=("Segoe UI", 16, "bold"),
                                fg="#fff", bg=IDLE_COLOR, pady=8)
        self.verdict.pack(fill="x")

        body = ttk.Frame(parent, padding=8)
        body.pack(fill="both", expand=True)

        # Tres columnas lado a lado: FRONTAL | TRASERA | FUSIONADO.
        self._cols["camf"] = self._make_col(body, 0, "CÁMARA FRONTAL", "#1b3a5b")
        self._cols["camb"] = self._make_col(body, 1, "CÁMARA TRASERA", "#3a2b5b")
        self._cols["cam"] = self._make_col(body, 2, "FUSIONADO (cam)", "#2b3b2b")
        for c in range(3):
            body.columnconfigure(c, weight=1, uniform="cols")

    def _make_col(self, parent: ttk.Frame, col: int, title: str, bg: str) -> dict:
        # width/height fijos + pack_propagate(False): el texto cambia cada frame
        # (coords de pelota); sin esto la columna se redimensiona y parpadea.
        frame = tk.Frame(parent, bd=1, relief="solid", bg=bg,
                         width=210, height=190, padx=8, pady=6)
        frame.grid(row=0, column=col, padx=4, pady=2, sticky="nsew")
        frame.pack_propagate(False)
        tk.Label(frame, text=title, font=("Segoe UI", 10, "bold"),
                 fg="#fff", bg=bg, anchor="w").pack(anchor="w")

        ball = tk.Label(frame, font=("Consolas", 11, "bold"), fg=BALL_FG,
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

    # ── Render ──────────────────────────────────────────────────────────────
    def render(self, f: TopFrame) -> None:
        # Contadores (antes en _consume del loop; ahora se actualizan al render).
        self.frame_count += 1
        if self.last_seq >= 0:
            gap = f.seq - self.last_seq - 1
            if gap > 0:
                self.dropped += gap
        self.last_seq = f.seq
        phantom = is_phantom(f.camf, f.camb, self.thresh_mm)
        if phantom:
            self.phantom_count += 1

        self._render_per(self._cols["camf"], f.camf)
        self._render_per(self._cols["camb"], f.camb)
        self._render_per(self._cols["cam"], f.cam, highlight=False)

        d = ball_disagreement_mm(f.camf, f.camb)
        verdict = fusion_verdict(f.camf, f.camb, self.thresh_mm)

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
        ball_fg = BALL_FG_PHANTOM if phantom else BALL_FG
        self._cols["camf"]["ball"].configure(fg=ball_fg)
        self._cols["camb"]["ball"].configure(fg=ball_fg)

        # Evento al shell: la primera vez que se detecta un fantasma, avisar.
        if phantom and self.phantom_count == 1:
            self.ctx.log("warn", "Fusión SOSPECHOSA: posible pelota fantasma "
                                  f"(Δ={d:.0f} mm > {self.thresh_mm:.0f} mm)")

        self.header.configure(
            text=(f"seq={f.seq}  frames={self.frame_count}  perdidos={self.dropped}  "
                  f"v{f.v}     fantasmas={self.phantom_count}  "
                  f"umbral={self.thresh_mm:.0f}mm"))

    def _render_per(self, col: dict, cam, highlight: bool = True) -> None:
        """Dibuja una columna. `cam` es CamPer (camf/camb) o Cam (fusionado): ambos
        comparten ball_visible/ball_x_mm/ball_y_mm/{yellow,blue}_*. `highlight=False`
        para el fusionado, que nunca se resalta en rojo (su color lo fija render)."""
        if cam.ball_visible:
            col["ball"].configure(
                text=f"PELOTA  x={cam.ball_x_mm:+5d}  y={cam.ball_y_mm:+5d}")
        else:
            col["ball"].configure(text="PELOTA  — no la ve —")
        if not highlight:
            col["ball"].configure(fg=BALL_FG)
        col["yellow"].configure(
            text="AMA  " + _goal_text(cam.yellow_visible, cam.yellow_angle_deg,
                                      cam.yellow_distance_mm))
        col["blue"].configure(
            text="AZU  " + _goal_text(cam.blue_visible, cam.blue_angle_deg,
                                      cam.blue_distance_mm))
