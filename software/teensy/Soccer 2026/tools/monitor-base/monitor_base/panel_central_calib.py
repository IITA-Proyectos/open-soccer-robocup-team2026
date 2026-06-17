"""panel_central_calib.py — Panel "CALIBRAR CENTRAL" (embebible en el shell).

Banco de calibración del MOVIMIENTO de la placa CENTRAL, todo desde la app:
edita las potencias de rueda (pisos de PWM, eficiencias, PWM de avance recto) y el
PID de rumbo con giroscopio, y los persiste a EEPROM. Manda los comandos de texto
que el firmware (flasheado con el flag de calibración) ya entiende:

  SET MINPWM <idx 0..2> <val>   piso de PWM por motor (0=del-izq,1=del-der,2=trasera)  0..149
  SET EFF    <idx 0..2> <val>   eficiencia ×100 por rueda                              50..200
  SET FWDL <val> / SET FWDR <val>   PWM rueda izq/der en avance recto                  0..149
  SET GKP/GKI/GKD <val>             PID de rumbo (giroscopio) ×1000                     0..32000
  GET                                el firmware responde con "ccfg" (ver protocol_central)
  SAVE                               persiste a EEPROM

Los valores actuales del robot llegan en CentralFrame.ccfg (sub-objeto OPCIONAL).
Cuando llega un frame con ccfg, los campos se refrescan (salvo el que el operador
está editando ahora mismo, para no pisarle lo que tipea). Validación local con
clamp ANTES de mandar, y feedback visual del envío.

Consume CentralFrame (protocol_central.py, schema v1). MANDA comandos
(sends_commands=True) vía self.ctx.send. DEFENSIVO: nunca tumba Tk con un frame
corto ni con un campo vacío.
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk
from typing import Callable, Dict, List, Optional

from .panel import Panel
from .protocol_central import CalibConfig, CentralFrame
from .shell_theme import (ACCENT, BAD, BG, BG2, CARD, FG, MUTED, OK, PANEL,
                         WARN)

# Rangos (los MISMOS que valida el firmware en src/shared/central_config.h).
PWM_MIN, PWM_MAX = 0, 149
EFF_MIN, EFF_MAX = 50, 200
GAIN_MIN, GAIN_MAX = 0, 32000


def _clamp(v: int, lo: int, hi: int) -> int:
    return lo if v < lo else hi if v > hi else v


class _Field:
    """Un campo editable: label + Entry + rango + cómo se traduce a comando SET."""

    def __init__(self, parent: tk.Widget, row: int, label: str,
                 lo: int, hi: int, cmd_fn: Callable[[int], str]):
        self.lo = lo
        self.hi = hi
        self.cmd_fn = cmd_fn          # int_clampeado -> "SET ..."
        self.var = tk.StringVar(value="")
        self._editing = False

        tk.Label(parent, text=label, anchor="w", bg=BG, fg=FG,
                 font=("Segoe UI", 10), width=22).grid(
            row=row, column=0, sticky="w", padx=(0, 6), pady=2)
        self.entry = tk.Entry(parent, textvariable=self.var, width=8,
                              font=("Consolas", 10), bg=PANEL, fg=FG,
                              insertbackground=FG, relief="flat",
                              highlightthickness=1, highlightbackground="#222b36",
                              justify="right")
        self.entry.grid(row=row, column=1, sticky="w", pady=2)
        # marcar "editando" mientras tiene foco para no pisarle lo que tipea con un GET.
        self.entry.bind("<FocusIn>", lambda _e: self._set_editing(True))
        self.entry.bind("<FocusOut>", lambda _e: self._set_editing(False))
        tk.Label(parent, text=f"[{lo}..{hi}]", anchor="w", bg=BG, fg=MUTED,
                 font=("Consolas", 8), width=11).grid(
            row=row, column=2, sticky="w", padx=(6, 0))

    def _set_editing(self, on: bool) -> None:
        self._editing = on

    @property
    def editing(self) -> bool:
        return self._editing

    def set_value(self, v: int) -> None:
        """Refresca desde el robot (no pisa si el operador está editando)."""
        if self._editing:
            return
        self.var.set(str(int(v)))

    def value_clamped(self) -> Optional[int]:
        """Lee el Entry, parsea y clampea al rango. None si está vacío/no numérico."""
        s = self.var.get().strip()
        if s == "":
            return None
        try:
            raw = int(float(s))
        except ValueError:
            return None
        v = _clamp(raw, self.lo, self.hi)
        if v != raw:                      # se salió de rango → reflejar el clamp en la UI
            self.var.set(str(v))
        return v

    def command(self) -> Optional[str]:
        v = self.value_clamped()
        if v is None:
            return None
        return self.cmd_fn(v)


class CentralCalibPanel(Panel):
    title = "Calibrar CENTRAL"
    key = "central_calib"
    icon = "⚙"
    board = "central"
    sends_commands = True

    # ── Layout ────────────────────────────────────────────────────────────────
    def build(self, parent: tk.Frame) -> None:
        self.header = tk.Label(
            parent, text="esperando datos del robot…", anchor="w",
            font=("Consolas", 11, "bold"), bg=BG, fg=FG, padx=8, pady=6)
        self.header.pack(fill="x")

        body = ttk.Frame(parent, padding=8)
        body.pack(fill="both", expand=True)

        self.fields: Dict[str, _Field] = {}

        # ── Columna 1: potencias de rueda (pisos PWM + eficiencias) ──
        col0 = tk.LabelFrame(body, text="POTENCIAS DE RUEDA", bg=BG, fg=MUTED,
                             font=("Segoe UI", 9, "bold"), padx=8, pady=6,
                             highlightbackground="#222b36", relief="solid", bd=1)
        col0.grid(row=0, column=0, sticky="nw", padx=(0, 10))

        r = 0
        self.fields["min0"] = _Field(col0, r, "Piso PWM del-izq", PWM_MIN, PWM_MAX,
                                     lambda v: f"SET MINPWM 0 {v}"); r += 1
        self.fields["min1"] = _Field(col0, r, "Piso PWM del-der", PWM_MIN, PWM_MAX,
                                     lambda v: f"SET MINPWM 1 {v}"); r += 1
        self.fields["min2"] = _Field(col0, r, "Piso PWM trasera", PWM_MIN, PWM_MAX,
                                     lambda v: f"SET MINPWM 2 {v}"); r += 1
        ttk.Separator(col0, orient="horizontal").grid(
            row=r, column=0, columnspan=3, sticky="we", pady=4); r += 1
        self.fields["eff0"] = _Field(col0, r, "Eficiencia del-izq ×100", EFF_MIN, EFF_MAX,
                                     lambda v: f"SET EFF 0 {v}"); r += 1
        self.fields["eff1"] = _Field(col0, r, "Eficiencia del-der ×100", EFF_MIN, EFF_MAX,
                                     lambda v: f"SET EFF 1 {v}"); r += 1
        self.fields["eff2"] = _Field(col0, r, "Eficiencia trasera ×100", EFF_MIN, EFF_MAX,
                                     lambda v: f"SET EFF 2 {v}"); r += 1

        # ── Columna 2: avance recto + PID de rumbo ──
        col1 = tk.LabelFrame(body, text="AVANCE RECTO  ·  PID DE GIRO (giroscopio)",
                             bg=BG, fg=MUTED, font=("Segoe UI", 9, "bold"),
                             padx=8, pady=6, highlightbackground="#222b36",
                             relief="solid", bd=1)
        col1.grid(row=0, column=1, sticky="nw")

        r = 0
        self.fields["fwdl"] = _Field(col1, r, "PWM avance izq", PWM_MIN, PWM_MAX,
                                     lambda v: f"SET FWDL {v}"); r += 1
        self.fields["fwdr"] = _Field(col1, r, "PWM avance der", PWM_MIN, PWM_MAX,
                                     lambda v: f"SET FWDR {v}"); r += 1
        ttk.Separator(col1, orient="horizontal").grid(
            row=r, column=0, columnspan=3, sticky="we", pady=4); r += 1
        self.fields["gkp"] = _Field(col1, r, "PID giro Kp ×1000", GAIN_MIN, GAIN_MAX,
                                    lambda v: f"SET GKP {v}"); r += 1
        self.fields["gki"] = _Field(col1, r, "PID giro Ki ×1000", GAIN_MIN, GAIN_MAX,
                                    lambda v: f"SET GKI {v}"); r += 1
        self.fields["gkd"] = _Field(col1, r, "PID giro Kd ×1000", GAIN_MIN, GAIN_MAX,
                                    lambda v: f"SET GKD {v}"); r += 1

        # ── Barra de acciones ──
        actions = tk.Frame(parent, bg=BG2, padx=8, pady=8)
        actions.pack(fill="x")
        tk.Button(actions, text="Leer del robot (GET)", command=self._on_get,
                  bg=CARD, fg=FG, activebackground="#1d2a36", activeforeground=ACCENT,
                  relief="flat", font=("Segoe UI", 10), padx=10, pady=4).pack(side="left")
        tk.Button(actions, text="Aplicar todo", command=self._on_apply_all,
                  bg=CARD, fg=FG, activebackground="#1d2a36", activeforeground=ACCENT,
                  relief="flat", font=("Segoe UI", 10, "bold"), padx=10, pady=4
                  ).pack(side="left", padx=(8, 0))
        # GUARDAR destacado: es la acción que PERSISTE a EEPROM.
        tk.Button(actions, text="GUARDAR en EEPROM (SAVE)", command=self._on_save,
                  bg=ACCENT, fg="#06231a", activebackground=OK, activeforeground="#06231a",
                  relief="flat", font=("Segoe UI", 10, "bold"), padx=12, pady=4
                  ).pack(side="right")

        self.status = tk.Label(parent, text="", anchor="w", bg=BG, fg=MUTED,
                               font=("Consolas", 9), padx=8, pady=4)
        self.status.pack(fill="x", side="bottom")

        self._got_ccfg = False

    # ── Acciones ────────────────────────────────────────────────────────────────
    def _on_get(self) -> None:
        self.ctx.send("GET")
        self._set_status("→ GET enviado (esperando ccfg del robot…)", info=True)

    def _on_apply_all(self) -> None:
        cmds: List[str] = []
        for f in self.fields.values():
            c = f.command()
            if c is not None:
                cmds.append(c)
        if not cmds:
            self._set_status("⚠ no hay valores válidos para aplicar", warn=True)
            return
        for c in cmds:
            self.ctx.send(c)
        self._set_status(f"→ {len(cmds)} comandos SET enviados "
                         f"(NO persisten hasta GUARDAR)")

    def _on_save(self) -> None:
        # Mandamos los SET primero (lo editado) y luego SAVE, para que se persista
        # exactamente lo que está en pantalla.
        self._on_apply_all()
        self.ctx.send("SAVE")
        self._set_status("✔ SAVE enviado — calibración persistida a EEPROM", ok=True)

    # ── Render ────────────────────────────────────────────────────────────────
    def render(self, f: CentralFrame) -> None:
        seq = getattr(f, "seq", 0)
        v = getattr(f, "v", "?")
        ccfg = getattr(f, "ccfg", None)
        if isinstance(ccfg, CalibConfig):
            self._apply_ccfg(ccfg)
            self._got_ccfg = True
            tag = "ccfg=SÍ (valores del robot)"
        else:
            tag = "ccfg=— (firmware sin calibración o todavía sin GET)"
        self.header.configure(text=f"seq={seq}  v{v}     {tag}")

    def _apply_ccfg(self, c: CalibConfig) -> None:
        mp = list(c.min_pwm) + [0, 0, 0]
        ef = list(c.eff) + [100, 100, 100]
        self.fields["min0"].set_value(mp[0])
        self.fields["min1"].set_value(mp[1])
        self.fields["min2"].set_value(mp[2])
        self.fields["eff0"].set_value(ef[0])
        self.fields["eff1"].set_value(ef[1])
        self.fields["eff2"].set_value(ef[2])
        self.fields["fwdl"].set_value(c.fwd_l)
        self.fields["fwdr"].set_value(c.fwd_r)
        self.fields["gkp"].set_value(c.gkp)
        self.fields["gki"].set_value(c.gki)
        self.fields["gkd"].set_value(c.gkd)

    # ── Helpers ───────────────────────────────────────────────────────────────
    def _set_status(self, text: str, *, ok: bool = False, warn: bool = False,
                    info: bool = False) -> None:
        color = OK if ok else WARN if warn else ACCENT if info else MUTED
        self.status.configure(text=text, fg=color)
