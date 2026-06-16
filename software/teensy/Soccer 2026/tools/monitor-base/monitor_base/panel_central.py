"""panel_central.py — Panel "CEREBRO" de la placa CENTRAL (embebible en el shell).

Vista del cerebro de la CENTRAL: lo que la FSM DECIDE y lo que EJECUTA, lado a
lado, sin recalcular nada (FIEL al frame):

  • Estado FSM resaltado: rol (ATK/GK) · estado · ¿partido corriendo? (f.fsm.*).
  • Comando APLICADO por la FSM: vx / vy / w / spin (f.cmd.*).
  • BARRAS de PWM REAL por rueda (f.pwm[0..2], signed -255..255): la barra crece a
    la DERECHA si el PWM es +, a la IZQUIERDA si es − (signo = sentido de giro de
    esa rueda). Es el PWM que de verdad sale al motor — delata pisos, saturación,
    o un motor sin comando.

Consume CentralFrame (protocol_central.py, schema v1). NO manda comandos
(sends_commands=False): el banner de SIM y la status bar los pone el shell.

DEFENSIVO (igual que panel_base / panel_field): un frame corto o con un campo
ausente no debe tumbar Tk. Los accesos sensibles van por getattr/try-except con
default; render nunca levanta dentro del callback de Tk.
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk
from typing import Optional

from .panel import Panel
from .protocol_central import CentralFrame
from .shell_theme import (ACCENT, BAD, BG, FG, MUTED, OK, PANEL, WARN)
from .tooltip import attach_tooltip
from .tooltips_text import tip

PWM_MAX = 255                 # fondo de escala de la barra (PWM signed -255..255)
BAR_W, BAR_H = 460, 26        # tamaño de cada barra de rueda
ROW_GAP = 12
WHEEL_LABELS = ("M0", "M1", "M2")

# Colores de la barra de PWM por signo (avance vs reversa de esa rueda).
_PWM_POS = "#2ecc40"          # PWM + (verde)
_PWM_NEG = "#ff9f1c"          # PWM − (ámbar)
_PWM_ZERO = "#3a4450"         # quieto
_BAR_BG = "#0c0f12"
_BAR_MID = "#3a6"


class CentralPanel(Panel):
    title = "Cerebro"
    key = "cerebro"
    icon = "◉"
    board = "central"
    sends_commands = False

    # ── Layout ────────────────────────────────────────────────────────────────
    def build(self, parent: tk.Frame) -> None:
        self.frame_count = 0
        self._last_t: Optional[int] = None
        self._hz = 0.0
        self._bars: list = []        # (bg_rect, fill_rect, text_id) por rueda

        self.header = tk.Label(parent, text="esperando datos…", anchor="w",
                               font=("Consolas", 11, "bold"), bg=BG, fg=FG,
                               padx=8, pady=6)
        self.header.pack(fill="x")

        body = ttk.Frame(parent, padding=8)
        body.pack(fill="both", expand=True)

        # ── Estado FSM (resaltado) ──
        ttk.Label(body, text="ESTADO FSM", style="Muted.TLabel").grid(
            row=0, column=0, sticky="w")
        self.fsm_lbl = tk.Label(body, text="—", anchor="w", justify="left",
                                font=("Segoe UI", 16, "bold"), bg=PANEL, fg=FG,
                                padx=12, pady=10, width=34)
        self.fsm_lbl.grid(row=1, column=0, sticky="we", pady=(0, 10))
        attach_tooltip(self.fsm_lbl, tip("central.fsm"))

        # ── Comando aplicado ──
        ttk.Label(body, text="COMANDO APLICADO (lo que pide la FSM)",
                  style="Muted.TLabel").grid(row=2, column=0, sticky="w")
        self.cmd_txt = tk.Text(body, width=44, height=5, font=("Consolas", 11),
                               bg=PANEL, fg=FG, relief="flat", highlightthickness=0)
        self.cmd_txt.grid(row=3, column=0, sticky="we", pady=(0, 10))
        attach_tooltip(self.cmd_txt, tip("central.cmd"))

        # ── Barras de PWM real por rueda ──
        ttk.Label(body, text="PWM REAL POR RUEDA  (− ◄ centro ► +  ·  el que sale al motor)",
                  style="Muted.TLabel").grid(row=4, column=0, sticky="w")
        self.pwm_canvas = tk.Canvas(
            body, width=BAR_W, height=len(WHEEL_LABELS) * (BAR_H + ROW_GAP) + 6,
            bg=_BAR_BG, highlightthickness=0)
        self.pwm_canvas.grid(row=5, column=0, sticky="w")
        attach_tooltip(self.pwm_canvas, tip("central.pwm"))
        self._build_pwm_bars()

    def _build_pwm_bars(self) -> None:
        c = self.pwm_canvas
        mid = BAR_W / 2
        for i, lbl in enumerate(WHEEL_LABELS):
            y0 = 6 + i * (BAR_H + ROW_GAP)
            y1 = y0 + BAR_H
            # marco + línea de cero (centro)
            c.create_rectangle(2, y0, BAR_W - 2, y1, outline="#222b36", width=1)
            c.create_line(mid, y0, mid, y1, fill=_BAR_MID, dash=(2, 2))
            fill = c.create_rectangle(mid, y0 + 3, mid, y1 - 3, fill=_PWM_ZERO,
                                      outline="")
            txt = c.create_text(mid, (y0 + y1) / 2, text=f"{lbl} 0", fill="#fff",
                                font=("Consolas", 10, "bold"))
            self._bars.append((y0, y1, fill, txt))

    # ── Render ────────────────────────────────────────────────────────────────
    def render(self, f: CentralFrame) -> None:
        self.frame_count += 1
        self._update_hz(f)
        self._render_fsm(f)
        self._render_cmd(f)
        self._render_pwm(f)
        seq = getattr(f, "seq", 0)
        v = getattr(f, "v", "?")
        self.header.configure(
            text=(f"seq={seq}  v{v}     {self._hz:.0f} Hz   "
                  f"frames: {self.frame_count}"))

    def _update_hz(self, f: CentralFrame) -> None:
        t = getattr(f, "t_ms", None)
        if self._last_t is not None and t is not None and t > self._last_t:
            inst = 1000.0 / (t - self._last_t)
            self._hz = inst if self._hz == 0 else 0.85 * self._hz + 0.15 * inst
        self._last_t = t

    def _render_fsm(self, f: CentralFrame) -> None:
        fsm = getattr(f, "fsm", None)
        role = getattr(fsm, "role", "?")
        state = getattr(fsm, "state", "?")
        match = bool(getattr(fsm, "match", False))
        role_name = {"ATK": "DELANTERO", "GK": "ARQUERO"}.get(role, role)
        # Color de fondo por estado del partido: corriendo = verde, parado = ámbar.
        if match:
            bg, fg = "#143d22", OK
            mtxt = "▶ PARTIDO"
        else:
            bg, fg = "#3a2e10", WARN
            mtxt = "■ PARADO"
        self.fsm_lbl.configure(
            text=f"{role_name}  ·  {state}\n{mtxt}", bg=bg, fg=fg)

    def _render_cmd(self, f: CentralFrame) -> None:
        cmd = getattr(f, "cmd", None)
        vx = getattr(cmd, "vx", 0)
        vy = getattr(cmd, "vy", 0)
        w = getattr(cmd, "w", 0)
        spin = getattr(cmd, "spin", 0)
        self._set_text(self.cmd_txt,
            f"vx (lateral)  : {vx:+6d} mm/s\n"
            f"vy (al arco)  : {vy:+6d} mm/s\n"
            f"w  (giro)     : {w:+6d} cdeg/s\n"
            f"spin (impulso): {spin:+6d} pwm\n")

    def _render_pwm(self, f: CentralFrame) -> None:
        c = self.pwm_canvas
        mid = BAR_W / 2
        half = BAR_W / 2 - 6
        pwm = getattr(f, "pwm", None) or []
        for i, (y0, y1, fill_id, txt_id) in enumerate(self._bars):
            try:
                val = int(pwm[i]) if i < len(pwm) else 0
            except (TypeError, ValueError):
                val = 0
            frac = max(-1.0, min(1.0, val / float(PWM_MAX)))
            x = mid + frac * half
            if val > 0:
                color = _PWM_POS
            elif val < 0:
                color = _PWM_NEG
            else:
                color = _PWM_ZERO
            # rectángulo desde el centro hacia el lado del signo
            c.coords(fill_id, min(mid, x), y0 + 3, max(mid, x), y1 - 3)
            c.itemconfig(fill_id, fill=color)
            # texto: poner el número del lado opuesto a la barra para que se lea
            tx = mid + (-1 if val >= 0 else 1) * (half * 0.5)
            c.coords(txt_id, tx, (y0 + y1) / 2)
            c.itemconfig(txt_id, text=f"{WHEEL_LABELS[i]} {val:+d}")

    # ── Helpers ───────────────────────────────────────────────────────────────
    def _set_text(self, widget: tk.Text, text: str) -> None:
        widget.delete("1.0", "end")
        widget.insert("1.0", text)
