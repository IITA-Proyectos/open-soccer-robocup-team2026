"""panel_central_field.py — Panel de CANCHA visto desde la CENTRAL (embebible).

Vista X-Y absoluta de la cancha con el robot dibujado en la pose que la CENTRAL
RECIBE en su WorldSnapshot (`snap.my_x/y/my_heading_deg`), la pelota relativa
ubicada con esa pose, los arcos polares (propio y rival, proyectados desde la
pose), el vector de COMANDO que la FSM está aplicando (`cmd.vx/vy`), un dial
pequeño de heading en una esquina, y la estela del robot.

Diseño FIEL: la app NO recalcula nada — dibuja lo que la CENTRAL ya tiene en el
frame. Si `snap.my_pose_conf == 0` el TOP no está anclando: en ese caso se
dibuja la cancha con el robot al CENTRO + una etiqueta roja "POSE NO ANCLADA"
y la pelota/arcos se ubican relativos al centro (honesto: no se inventa pose).

Reutiliza primitivas YA testeadas de gui_field:
  • field_to_px / MARGIN_PX / FIELD_W_MM / FIELD_H_MM / GOAL_W_MM
  • Trail (estela acotada)
  • draw_robot_glyph / draw_ball_glyph / draw_faded_trail

Y la geometría PURA propia del marco CENTRAL (CCW+) vive en
central_field_geometry.py (testeada aparte).

NOTA sobre el signo del heading para el glyph: `draw_robot_glyph` fue diseñado
con la convención CW+ del TOP (radar). Acá la canónica es CCW+ → para que el
robot APUNTE al lado correcto en la pantalla, le pasamos `-heading_deg`. La
pelota y los arcos NO van por el glyph, van por la geometría propia (CCW+).
"""
from __future__ import annotations

import math
import tkinter as tk
from tkinter import ttk
from typing import Tuple

from .central_field_geometry import (
    polar_to_field,
    rotate_robot_frame_to_field,
)
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
)
from .panel import Panel
from .protocol_central import CentralFrame
from .shell_theme import BAD, BG, FG, MUTED
from .tof_layout import load_or_default

# Paleta del césped (igual que panel_field para coherencia visual).
_FIELD_BG = "#0c3a18"
_FIELD_LINE = "#cfe"
_MID = "#3a7a4a"
_GOAL_DOWN = "#4a78e6"      # arco propio (abajo, +Y=0) — azul (mismo código que TOP)
_GOAL_UP = "#e6d44a"        # arco rival (arriba, +Y=2430) — amarillo
_ROBOT_FILL = "#2b6"
_ROBOT_OUTLINE = "#bfe"
_ROBOT_NOPOSE_FILL = "#403030"        # robot fantasma (cuando pose no ancla)
_ROBOT_NOPOSE_OUTLINE = "#8a4040"
_TRAIL_ROBOT = "#2bd27a"
_BALL_FILL = "#ff8c2a"
_BALL_OUTLINE = "#ffd"
_CMD_VEC = "#ffd166"        # vector de comando (ámbar claro)
_GOAL_OWN_MARKER = "#7aa9ff"
_GOAL_OPP_MARKER = "#ffe27a"
_INFO_BG = "#0c0f12"

# Dial del heading (esquina del canvas).
_DIAL_R = 28
_DIAL_PAD = 12
_DIAL_BG = "#0b1f12"
_DIAL_RING = "#3a7a4a"
_DIAL_TICK = "#bfe"
_DIAL_NEEDLE = "#2bd27a"
_DIAL_NEEDLE_INVALID = "#7d8896"

# Vector de comando: factor para que se vea sin tapar todo. 1 mm/s ≈ 0.18 px en
# una cancha de 560 px de alto / 2430 mm → un comando de 400 mm/s sale ~70 px.
_CMD_SCALE = 0.18


class CentralFieldPanel(Panel):
    """Vista de CANCHA desde la CENTRAL (qué ve el cerebro)."""
    title = "Cancha (CENTRAL)"
    key = "cancha-central"
    icon = "⬢"
    board = "central"
    sends_commands = False

    # ── Layout ──────────────────────────────────────────────────────────────
    def build(self, parent: tk.Frame) -> None:
        cfg = load_or_default(self.ctx.config_path)
        self.field_w = float(cfg.wall.field_width_mm or FIELD_W_MM)
        self.field_h = float(cfg.wall.field_height_mm or FIELD_H_MM)

        self.robot_trail = Trail()
        self.frame_count = 0

        # Tamaño del canvas (mantiene proporción de la cancha).
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
            font=("Segoe UI", 9), justify="left",
            wraplength=int(self.cw + 340),
            text=("Pose, pelota y arcos = lo que la CENTRAL recibe en su "
                  "WorldSnapshot. Vector de comando = lo que la FSM aplica. "
                  "Si la pose no está anclada (conf=0) se avisa en rojo y se "
                  "dibuja la pelota/arcos relativos al centro (no se inventa).")
        ).pack(fill="x")

        body = ttk.Frame(parent, padding=8)
        body.pack(fill="both", expand=True)
        self.canvas = tk.Canvas(body, width=self.cw, height=self.ch,
                                bg=_FIELD_BG, highlightthickness=0)
        self.canvas.grid(row=0, column=0, sticky="n")

        side = ttk.Frame(body)
        side.grid(row=0, column=1, sticky="n", padx=(10, 0))
        ttk.Label(side, text="QUÉ VE LA CENTRAL", style="Muted.TLabel").pack(anchor="w")
        self.info = tk.Text(side, width=42, height=16, font=("Consolas", 10),
                            bg=_INFO_BG, fg=FG, relief="flat", highlightthickness=0)
        self.info.pack(pady=(2, 6))
        self.pose_warn = tk.Label(side, text="", anchor="w", justify="left",
                                  font=("Segoe UI", 9, "bold"), bg=BG, fg=BAD,
                                  wraplength=320)
        self.pose_warn.pack(anchor="w")
        ttk.Button(side, text="↺ Borrar estela",
                   command=self._clear_trail).pack(pady=8, anchor="w")

        self._draw_field()

    # ── Geometría helpers ───────────────────────────────────────────────────
    def _fpx(self, x_mm: float, y_mm: float) -> Tuple[float, float]:
        return field_to_px(x_mm, y_mm, self.draw_w, self.draw_h,
                           field_w=self.field_w, field_h=self.field_h)

    def _draw_field(self) -> None:
        c = self.canvas
        x0, y0 = self._fpx(0, 0)
        x1, y1 = self._fpx(self.field_w, self.field_h)
        c.create_rectangle(x0, y1, x1, y0, outline=_FIELD_LINE, width=2)
        cxmid, _ = self._fpx(self.field_w / 2, 0)
        _, ymid = self._fpx(0, self.field_h / 2)
        c.create_line(x0, ymid, x1, ymid, fill=_MID)
        r = (self.field_w * 0.18) / self.field_w * self.draw_w
        c.create_oval(cxmid - r, ymid - r, cxmid + r, ymid + r, outline=_MID)
        c.create_oval(cxmid - 2, ymid - 2, cxmid + 2, ymid + 2,
                      fill=_MID, outline="")
        # Arcos: Y=0 = arco PROPIO (abajo), Y=field_h = arco RIVAL (arriba).
        gx0, _ = self._fpx(self.field_w / 2 - GOAL_W_MM / 2, 0)
        gx1, _ = self._fpx(self.field_w / 2 + GOAL_W_MM / 2, 0)
        c.create_line(gx0, y0, gx1, y0, fill=_GOAL_DOWN, width=5)
        c.create_line(gx0, y1, gx1, y1, fill=_GOAL_UP, width=5)

    # ── Render ──────────────────────────────────────────────────────────────
    def render(self, f: CentralFrame) -> None:
        self.frame_count += 1
        snap = getattr(f, "snap", None)
        if snap is None:
            return                                 # frame degenerado: nada que dibujar
        self._accumulate(snap)
        self._draw_dynamic(f)
        seq = getattr(f, "seq", 0)
        v = getattr(f, "v", "?")
        self.header.configure(
            text=(f"seq={seq}  v{v}     "
                  f"estela: {len(self.robot_trail)} pts   "
                  f"frames: {self.frame_count}"))

    def _accumulate(self, snap) -> None:
        # Solo acumulamos estela si HAY pose anclada (conf>0). Si no, la "pose"
        # son 0,0 del firmware viejo / no-ancla → mentiría dibujar trayectoria.
        if getattr(snap, "has_pose", False):
            try:
                self.robot_trail.push(float(snap.my_x), float(snap.my_y))
            except (TypeError, ValueError):
                pass

    def _draw_dynamic(self, f: CentralFrame) -> None:
        c = self.canvas
        c.delete("dyn")
        snap = f.snap
        cmd = getattr(f, "cmd", None)
        has_pose = bool(getattr(snap, "has_pose", False))

        # Pose efectiva del robot para dibujo. Si no hay ancla, ponemos el robot
        # en el CENTRO de la cancha y avisamos — la pelota/arcos relativos se
        # ubicarán respecto al centro para no inventar absolutos.
        if has_pose:
            rx_mm = float(snap.my_x)
            ry_mm = float(snap.my_y)
            r_hdg = float(snap.my_heading_deg)
        else:
            rx_mm = self.field_w / 2
            ry_mm = self.field_h / 2
            # heading_valid puede seguir siendo True aunque no haya ancla (el
            # BNO funciona aunque la trilateración no haya cerrado).
            r_hdg = float(getattr(snap, "my_heading_deg", 0.0))

        # Estela del robot (solo cuando hay pose).
        if has_pose:
            draw_faded_trail(c, self.robot_trail.points(), self._fpx, _FIELD_BG,
                             _TRAIL_ROBOT, w_min=1.0, w_max=3.0)

        # Pelota: viene relativa al robot — la rotamos al marco cancha con la
        # pose efectiva (real si hay ancla, ficticia=centro si no).
        if bool(getattr(snap, "ball_vis", False)):
            dx, dy = rotate_robot_frame_to_field(
                float(snap.ball_x), float(snap.ball_y), r_hdg)
            bx, by = rx_mm + dx, ry_mm + dy
            self._draw_ball(bx, by)

        # Arcos (proyección polar desde el robot — lo que ve la CENTRAL).
        goal_own_dist = float(getattr(snap, "goal_own_dist", 0))
        goal_own_ang = float(getattr(snap, "goal_own_ang", 0.0))
        if goal_own_dist > 0:
            gx, gy = polar_to_field(rx_mm, ry_mm, r_hdg,
                                    goal_own_ang, goal_own_dist)
            self._draw_goal_marker(gx, gy, _GOAL_OWN_MARKER, "P")
        if bool(getattr(snap, "goal_opp_vis", False)):
            gx, gy = polar_to_field(rx_mm, ry_mm, r_hdg,
                                    float(getattr(snap, "goal_opp_ang", 0.0)),
                                    float(getattr(snap, "goal_opp_dist", 0)))
            self._draw_goal_marker(gx, gy, _GOAL_OPP_MARKER, "R")

        # Robot (después de las marcas para que quede arriba).
        self._draw_robot(rx_mm, ry_mm, r_hdg, has_pose=has_pose)

        # Vector de comando saliendo del robot.
        if cmd is not None:
            self._draw_cmd_vector(rx_mm, ry_mm, r_hdg,
                                  float(cmd.vx), float(cmd.vy))

        # Dial de heading (esquina superior derecha del canvas).
        self._draw_heading_dial(r_hdg, bool(getattr(snap, "heading_valid", False)))

        # Aviso "POSE NO ANCLADA" sobre el centro.
        if not has_pose:
            cpx, cpy = self._fpx(self.field_w / 2, self.field_h / 2)
            self.canvas.create_text(
                cpx, cpy - 40, text="POSE NO ANCLADA (conf=0)",
                fill=BAD, font=("Segoe UI", 11, "bold"), tags="dyn")

        self._render_info(f, has_pose=has_pose)
        self._render_pose_warn(snap, has_pose=has_pose)

    # ── Glifos ──────────────────────────────────────────────────────────────
    def _draw_robot(self, x_mm: float, y_mm: float, heading_deg: float,
                    has_pose: bool) -> None:
        px, py = self._fpx(x_mm, y_mm)
        # draw_robot_glyph usa convención CW+ (legacy del TOP). Acá la canónica
        # es CCW+ → pasamos -heading para que la nariz apunte al lado correcto.
        fill = _ROBOT_FILL if has_pose else _ROBOT_NOPOSE_FILL
        outline = _ROBOT_OUTLINE if has_pose else _ROBOT_NOPOSE_OUTLINE
        draw_robot_glyph(self.canvas, px, py, -heading_deg,
                         fill=fill, outline=outline, tag="dyn")

    def _draw_ball(self, x_mm: float, y_mm: float) -> None:
        px, py = self._fpx(x_mm, y_mm)
        draw_ball_glyph(self.canvas, px, py, _BALL_FILL, _BALL_OUTLINE,
                        _FIELD_BG, tag="dyn")

    def _draw_goal_marker(self, x_mm: float, y_mm: float, color: str,
                          letter: str) -> None:
        px, py = self._fpx(x_mm, y_mm)
        r = 7
        self.canvas.create_oval(px - r, py - r, px + r, py + r,
                                outline=color, width=2, tags="dyn")
        self.canvas.create_text(px, py, text=letter, fill=color,
                                font=("Segoe UI", 8, "bold"), tags="dyn")

    def _draw_cmd_vector(self, robot_x_mm: float, robot_y_mm: float,
                         heading_deg: float, cmd_vx: float,
                         cmd_vy: float) -> None:
        # Rotamos el comando (marco robot) a marco cancha (mm/s) y de ahí
        # directo a píxeles (la conversión px↔mm se hace acá inline porque la
        # flecha sale del PIXEL del robot, no de coordenadas físicas que crucen
        # field_to_px — sería redundante).
        dxm, dym = rotate_robot_frame_to_field(cmd_vx, cmd_vy, heading_deg)
        # Magnitud en mm/s; escalamos en mm para que la flecha quepa.
        magnitude = math.hypot(dxm, dym)
        if magnitude < 1.0:
            return                                 # sin comando significativo
        # Escala: queremos ~ _CMD_SCALE px por mm/s. Convertimos a un punto fin
        # en marco cancha multiplicando por (mm_por_px de la cancha)/scale_inv.
        # Más simple aún: trabajamos directo en píxeles.
        px0, py0 = self._fpx(robot_x_mm, robot_y_mm)
        # dxm,dym están en mm/s. Multiplicar por _CMD_SCALE da px directo.
        # Pero ojo: en _fpx el eje Y de pantalla es INVERTIDO respecto a la
        # cancha (Y=0 abajo). dxm es +X cancha → +X pantalla; dym es +Y cancha
        # → -Y pantalla.
        px1 = px0 + dxm * _CMD_SCALE
        py1 = py0 - dym * _CMD_SCALE
        self.canvas.create_line(px0, py0, px1, py1, fill=_CMD_VEC,
                                width=3, arrow="last", arrowshape=(10, 12, 4),
                                tags="dyn")

    def _draw_heading_dial(self, heading_deg: float, valid: bool) -> None:
        c = self.canvas
        cx = self.cw - _DIAL_PAD - _DIAL_R
        cy = _DIAL_PAD + _DIAL_R
        c.create_oval(cx - _DIAL_R, cy - _DIAL_R, cx + _DIAL_R, cy + _DIAL_R,
                      outline=_DIAL_RING, fill=_DIAL_BG, width=1, tags="dyn")
        # Marca "N" arriba (al arco rival, +Y cancha).
        c.create_text(cx, cy - _DIAL_R + 8, text="N", fill=_DIAL_TICK,
                      font=("Consolas", 8, "bold"), tags="dyn")
        # Tick este/oeste/sur.
        c.create_line(cx + _DIAL_R - 6, cy, cx + _DIAL_R - 2, cy,
                      fill=_DIAL_RING, tags="dyn")
        c.create_line(cx - _DIAL_R + 2, cy, cx - _DIAL_R + 6, cy,
                      fill=_DIAL_RING, tags="dyn")
        c.create_line(cx, cy + _DIAL_R - 6, cx, cy + _DIAL_R - 2,
                      fill=_DIAL_RING, tags="dyn")
        # Aguja: heading CCW+ desde +Y. En pantalla +Y es ARRIBA, CCW+ en
        # canvas también es la CCW geométrica → el vector frente es
        # (-sin(h), -cos(h)) en coords de canvas (con Y hacia abajo) usando
        # +Y_cancha → arriba (signo negativo de Y_canvas).
        a = math.radians(heading_deg)
        L = _DIAL_R - 6
        nx = cx - math.sin(a) * L
        ny = cy - math.cos(a) * L
        col = _DIAL_NEEDLE if valid else _DIAL_NEEDLE_INVALID
        c.create_line(cx, cy, nx, ny, fill=col, width=2,
                      arrow="last", arrowshape=(8, 10, 3), tags="dyn")
        c.create_oval(cx - 2, cy - 2, cx + 2, cy + 2, fill=col,
                      outline="", tags="dyn")
        # Valor numérico debajo del dial.
        c.create_text(cx, cy + _DIAL_R + 8,
                      text=f"hdg {heading_deg:+.0f}°" + ("" if valid else " ⚠"),
                      fill=col, font=("Consolas", 9, "bold"), tags="dyn")

    # ── Panel lateral (texto) ───────────────────────────────────────────────
    def _render_info(self, f: CentralFrame, *, has_pose: bool) -> None:
        snap = f.snap
        cmd = getattr(f, "cmd", None)

        def yn(b: bool) -> str:
            return "SÍ" if b else "no"

        if has_pose:
            pose_line = (f"x={snap.my_x:+5d}  y={snap.my_y:+5d}  "
                         f"hdg={snap.my_heading_deg:+.1f}°  conf={snap.my_pose_conf}")
        else:
            pose_line = "—  (POSE NO ANCLADA, conf=0)"

        ball_line = (f"x={snap.ball_x:+5d} y={snap.ball_y:+5d}  "
                     f"(vx={snap.ball_vx:+4d} vy={snap.ball_vy:+4d})"
                     if bool(getattr(snap, "ball_vis", False)) else "no visible")

        gown = f"ang={snap.goal_own_ang:+6.1f}°  dist={snap.goal_own_dist} mm"
        if bool(getattr(snap, "goal_opp_vis", False)):
            gopp = f"ang={snap.goal_opp_ang:+6.1f}°  dist={snap.goal_opp_dist} mm"
        else:
            gopp = "no visible"

        if cmd is not None:
            cmd_line = (f"vx={cmd.vx:+5d}  vy={cmd.vy:+5d}  "
                        f"w={cmd.w:+5d} cdeg/s  spin={cmd.spin:+4d}")
        else:
            cmd_line = "—"

        text = (
            f"POSE          : {pose_line}\n"
            f"heading válido: {yn(bool(getattr(snap, 'heading_valid', False)))}\n"
            f"\n"
            f"PELOTA        : {ball_line}\n"
            f"\n"
            f"ARCO PROPIO   : {gown}\n"
            f"ARCO RIVAL    : {gopp}\n"
            f"\n"
            f"COMANDO FSM   : {cmd_line}\n"
        )
        self._set_text(self.info, text)

    def _render_pose_warn(self, snap, *, has_pose: bool) -> None:
        if has_pose:
            self.pose_warn.configure(text="")
        else:
            self.pose_warn.configure(
                text="⚠ POSE NO ANCLADA — el TOP no cerró trilateración (conf=0). "
                     "Robot dibujado en el centro como referencia visual.")

    def _clear_trail(self) -> None:
        self.robot_trail.clear()
        self.ctx.log("info", "estela del robot (cancha CENTRAL) borrada")

    def _set_text(self, w: tk.Text, text: str) -> None:
        w.delete("1.0", "end")
        w.insert("1.0", text)
