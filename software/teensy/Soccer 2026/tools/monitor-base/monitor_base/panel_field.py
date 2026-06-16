"""panel_field.py — Panel de CANCHA (X-Y fija) embebible en el shell unificado.

Refactor de gui_field.FieldApp a Panel: el mismo canvas de cancha (límite, media
cancha, círculo central, arcos) con el ROBOT en su pose + ESTELA, la PELOTA
relativa ubicada con esa pose + estela, y el panel lateral de info, el checkbox
"Comparar con OTOS" y el botón "Borrar estelas". Sin ventana/loop/fuente propios
(el shell se los provee).

PRINCIPIO (igual que la GUI vieja, corrección María 2026-06-15): la app MUESTRA lo
que el TOP ya calcula y manda a CENTRAL — NO recalcula nada. Lo único que calcula
es la ESTELA histórica. La pose viene de `snap.my_x/y/heading`; la pelota viene
RELATIVA al robot y se compone con esa pose vía `robot_rel_to_field`. El overlay
OTOS (cyan, opcional) dibuja la pose de la odometría de la base como cross-check.

La LÓGICA PURA (robot_rel_to_field, field_to_px, Trail, pose_gap_mm) se IMPORTA de
gui_field — ya está testeada — NO se copia. Acá solo vive la capa Tk: build() arma
los widgets en el frame que da el shell; render(f) acumula la estela y redibuja.

Este panel NO manda comandos (sends_commands=False); el banner de SIMULADOR y la
status bar los pone el shell global, así que acá no van.
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk
from typing import Optional, Tuple

from .gui_field import (
    FIELD_H_MM,
    FIELD_W_MM,
    GOAL_W_MM,
    MARGIN_PX,
    Trail,
    draw_ball_glyph,
    draw_faded_trail,
    draw_robot_glyph,
    field_to_px,
    pose_gap_mm,
    robot_rel_to_field,
)
from .panel import Panel
from .protocol_top import TopFrame
from .shell_theme import BG, FG, MUTED, PANEL
from .tof_layout import load_or_default
from .tooltip import attach_tooltip
from .tooltips_text import tip

# Colores propios de la cancha (verde césped + marcas), independientes de la
# paleta industrial: la cancha es un "área de dato" con su semántica de color.
_FIELD_BG = "#0c3a18"      # césped
_FIELD_LINE = "#cfe"       # límite
_MID = "#3a7a4a"           # media cancha + círculo central
_GOAL_DOWN = "#4a78e6"     # arco abajo (azul)
_GOAL_UP = "#e6d44a"       # arco arriba (amarillo)
_ROBOT_FILL = "#2b6"
_ROBOT_OUTLINE = "#bfe"
_TRAIL_ROBOT = "#2bd27a"
_TRAIL_BALL = "#c9821f"
_BALL_FILL = "#ff8c2a"
_BALL_OUTLINE = "#ffd"
_OTOS_TRAIL = "#33c4d6"
_OTOS_FILL = "#1f7e8c"
_OTOS_OUTLINE = "#7fe6f2"
_INFO_BG = "#0c0f12"


class FieldPanel(Panel):
    title = "Cancha"
    key = "cancha"
    icon = "⬡"
    sends_commands = False

    def build(self, parent: tk.Frame) -> None:
        cfg = load_or_default(self.ctx.config_path)
        self.field_w = float(cfg.wall.field_width_mm or FIELD_W_MM)
        self.field_h = float(cfg.wall.field_height_mm or FIELD_H_MM)

        # Estado (estelas + contador) en la instancia.
        self.robot_trail = Trail()
        self.ball_trail = Trail()
        self.otos_trail = Trail()        # overlay opcional (cross-check vs OTOS)
        self.frame_count = 0

        # Tamaño de dibujo (mantiene proporción de la cancha).
        self.draw_h = 560.0
        self.draw_w = self.draw_h * self.field_w / self.field_h
        self.cw = self.draw_w + 2 * MARGIN_PX
        self.ch = self.draw_h + 2 * MARGIN_PX

        self.header = tk.Label(
            parent, text="esperando datos…", anchor="w",
            font=("Consolas", 11, "bold"), bg=BG, fg=FG, padx=8, pady=6)
        self.header.pack(fill="x")
        tk.Label(
            parent, bg=BG, fg=MUTED, anchor="w", padx=8, pady=2,
            font=("Segoe UI", 9), justify="left", wraplength=int(self.cw + 320),
            text=("Muestra la POSE que el TOP manda a CENTRAL (snapshot my_x/y/hdg) "
                  "tal cual. La app solo agrega la ESTELA. Pelota = relativa del "
                  "snapshot ubicada con esa pose.")).pack(fill="x")

        body = ttk.Frame(parent, padding=8)
        body.pack(fill="both", expand=True)
        self.canvas = tk.Canvas(body, width=self.cw, height=self.ch,
                                bg=_FIELD_BG, highlightthickness=0)
        self.canvas.grid(row=0, column=0, sticky="n")

        side = ttk.Frame(body)
        side.grid(row=0, column=1, sticky="n", padx=(10, 0))
        self.info = tk.Text(side, width=40, height=14, font=("Consolas", 10),
                            bg=_INFO_BG, fg=FG, relief="flat", highlightthickness=0)
        self.info.pack()
        # Overlay OTOS opcional: pose recalculada por la odometría de la base
        # (cyan) para COMPARAR con la pose del snapshot (verde). En R2 el OTOS
        # está deshabilitado → no aparece (honesto).
        self.show_otos = tk.BooleanVar(value=False)
        otos_chk = ttk.Checkbutton(side, text="Comparar con OTOS (cyan)",
                                   variable=self.show_otos)
        otos_chk.pack(anchor="w", pady=(6, 0))
        attach_tooltip(otos_chk, tip("field.otos"))
        clr_btn = ttk.Button(side, text="↺ Borrar estelas", command=self._clear_trails)
        clr_btn.pack(pady=6, anchor="w")
        attach_tooltip(clr_btn, tip("field.clear"))

        self._draw_field()

    # ── Geometría ───────────────────────────────────────────────────────────
    def _fpx(self, x_mm: float, y_mm: float) -> Tuple[float, float]:
        return field_to_px(x_mm, y_mm, self.draw_w, self.draw_h,
                           field_w=self.field_w, field_h=self.field_h)

    def _draw_field(self) -> None:
        c = self.canvas
        x0, y0 = self._fpx(0, 0)
        x1, y1 = self._fpx(self.field_w, self.field_h)
        c.create_rectangle(x0, y1, x1, y0, outline=_FIELD_LINE, width=2)        # límite
        cxmid, _ = self._fpx(self.field_w / 2, 0)
        _, ymid = self._fpx(0, self.field_h / 2)
        c.create_line(x0, ymid, x1, ymid, fill=_MID)                            # media cancha
        r = (self.field_w * 0.18) / self.field_w * self.draw_w
        c.create_oval(cxmid - r, ymid - r, cxmid + r, ymid + r, outline=_MID)
        c.create_oval(cxmid - 2, ymid - 2, cxmid + 2, ymid + 2, fill=_MID, outline="")  # punto central
        # Arcos (Y=largo = arriba, Y=0 = abajo).
        gx0, _ = self._fpx(self.field_w / 2 - GOAL_W_MM / 2, 0)
        gx1, _ = self._fpx(self.field_w / 2 + GOAL_W_MM / 2, 0)
        c.create_line(gx0, y0, gx1, y0, fill=_GOAL_DOWN, width=5)               # arco abajo
        c.create_line(gx0, y1, gx1, y1, fill=_GOAL_UP, width=5)                 # arco arriba
        # Capa dinámica via tag "dyn".

    # ── Render ──────────────────────────────────────────────────────────────
    def render(self, f: TopFrame) -> None:
        self.frame_count += 1
        self._accumulate(f)
        self._draw_dynamic(f)
        self.header.configure(
            text=(f"seq={f.seq}  v{f.v}     "
                  f"estela robot: {len(self.robot_trail)} pts   "
                  f"pelota: {len(self.ball_trail)} pts   frames: {self.frame_count}"))

    def _accumulate(self, f: TopFrame) -> None:
        # Pose = lo que el TOP manda a CENTRAL (snapshot). NO se recalcula.
        if f.snap.valid:
            self.robot_trail.push(float(f.snap.my_x_mm), float(f.snap.my_y_mm))
            if f.snap.ball_visible:
                bx, by = robot_rel_to_field(f.snap.ball_x_mm, f.snap.ball_y_mm,
                                            f.snap.my_x_mm, f.snap.my_y_mm,
                                            f.snap.my_heading_deg)
                self.ball_trail.push(bx, by)
        # OTOS (overlay opcional): acumular siempre (se dibuja solo si se habilita).
        if f.base.pose_fresh:
            self.otos_trail.push(float(f.base.pose_x_mm), float(f.base.pose_y_mm))

    def _draw_dynamic(self, f: TopFrame) -> None:
        c = self.canvas
        c.delete("dyn")
        # Estelas atenuadas: la cabeza (actual) es lo más fuerte y la cola se
        # desvanece hacia el césped con la edad (lecturas viejas desaparecen).
        draw_faded_trail(c, self.robot_trail.points(), self._fpx, _FIELD_BG,
                         _TRAIL_ROBOT, w_min=1.0, w_max=3.0)
        draw_faded_trail(c, self.ball_trail.points(), self._fpx, _FIELD_BG,
                         _TRAIL_BALL, w_min=1.0, w_max=2.0)
        # Robot en la pose del snapshot (lo que el TOP manda a CENTRAL).
        if f.snap.valid:
            self._draw_robot(f.snap.my_x_mm, f.snap.my_y_mm, f.snap.my_heading_deg)
            # Pelota: relativa del snapshot, ubicada con la pose del snapshot.
            if f.snap.ball_visible:
                bx, by = robot_rel_to_field(f.snap.ball_x_mm, f.snap.ball_y_mm,
                                            f.snap.my_x_mm, f.snap.my_y_mm,
                                            f.snap.my_heading_deg)
                px, py = self._fpx(bx, by)
                draw_ball_glyph(c, px, py, _BALL_FILL, _BALL_OUTLINE, _FIELD_BG)
        # Overlay OTOS (opcional): estela + robot en CYAN para comparar.
        if self.show_otos.get():
            draw_faded_trail(c, self.otos_trail.points(), self._fpx, _FIELD_BG,
                             _OTOS_TRAIL, w_min=1.0, w_max=2.0)
            if f.base.pose_fresh:
                self._draw_robot(f.base.pose_x_mm, f.base.pose_y_mm,
                                 f.base.pose_heading_deg, fill=_OTOS_FILL,
                                 outline=_OTOS_OUTLINE)
        self._render_info(f)

    def _draw_robot(self, x_mm: float, y_mm: float, heading_deg: float,
                    fill: str = _ROBOT_FILL, outline: str = _ROBOT_OUTLINE) -> None:
        px, py = self._fpx(x_mm, y_mm)
        draw_robot_glyph(self.canvas, px, py, heading_deg, fill=fill, outline=outline)

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
        otos = ""
        if self.show_otos.get():
            if f.base.pose_fresh:
                gap = (pose_gap_mm(s.my_x_mm, s.my_y_mm, f.base.pose_x_mm, f.base.pose_y_mm)
                       if s.valid else None)
                otos = (f"\n-- OTOS (comparación) --\n"
                        f"pose OTOS       : x={f.base.pose_x_mm:+5d} y={f.base.pose_y_mm:+5d} "
                        f"hdg={f.base.pose_heading_deg:+.1f}°\n"
                        f"|snap − OTOS|   : {'—' if gap is None else f'{gap:.0f} mm'}  "
                        f"(marcos pueden diferir; mirá la FORMA)\n")
            else:
                otos = "\n-- OTOS (comparación) --\nsin OTOS fresco (en R2 está deshabilitado)\n"
        self._set_text(self.info,
            f"SNAPSHOT→CENTRAL : {snapst}\n"
            f"pose robot      : {pose}\n"
            f"heading válido  : {yn(f.imu.heading_valid)}\n"
            f"estela robot    : {len(self.robot_trail)} puntos\n"
            f"\n"
            f"¿PELOTA?        : {yn(s.ball_visible)}\n"
            f"pelota (rel.)   : x={s.ball_x_mm:+5d} y={s.ball_y_mm:+5d}\n"
            f"estela pelota   : {len(self.ball_trail)} puntos\n"
            f"{otos}"
            f"\n"
            f"cancha          : {int(self.field_w)} × {int(self.field_h)} mm\n"
            f"frames          : {self.frame_count}\n")

    def _clear_trails(self) -> None:
        self.robot_trail.clear()
        self.ball_trail.clear()
        self.otos_trail.clear()
        self.ctx.log("info", "estelas de la cancha borradas")

    def _set_text(self, w: tk.Text, text: str) -> None:
        w.delete("1.0", "end")
        w.insert("1.0", text)
