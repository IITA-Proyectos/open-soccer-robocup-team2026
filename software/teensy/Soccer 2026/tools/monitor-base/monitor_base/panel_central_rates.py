"""panel_central_rates.py — Panel "TASAS CENTRAL" (Hz de enlaces + de cambio).

Responde a UNA pregunta operativa: ¿a qué FRECUENCIA late cada cosa? No mira
VALORES (eso lo hace Salud CENTRAL), mira TASAS:

  • Hz de LLEGADA de cada enlace, derivado de cuán seguido CAMBIAN sus contadores:
      - TOP → CENTRAL  : Hz de cambio de f.top.fr   (frames buenos TOP→CENTRAL).
      - DOWN → CENTRAL : Hz de cambio de f.down.rx   (frames recibidos DOWN→CENTRAL).
      - CENTRAL telem. : Hz de llegada del propio CentralFrame (frame_hz).
    Un contador que sube de a 1 por frame TOP/DOWN → su Hz de cambio ES la tasa
    real a la que esa placa le habla a la CENTRAL.

  • Hz de CAMBIO de campos clave del WorldSnapshot / FSM:
      - pelota  : cuán seguido se mueve ball_x (epsilon para ignorar ruido).
      - pose    : cuán seguido cambia my_x / my_y (la odometría actualizando).
      - FSM     : transiciones de estado por segundo (fsm.state).
      - línea   : down.valid entrando/saliendo (la línea aparece/desaparece).
      - HEADING : flapping de heading_valid. Si baila > FLAP_WARN_HZ → ALERTA roja:
                  el heading prendiéndose/apagándose seguido envenena todo el
                  control que dependa del yaw.

RELOJ: usamos `f.t_ms` (reloj monótono del firmware), NO time.time() del host.
Motivo: es el mismo reloj que generó la telemetría, así el Hz refleja la cadencia
REAL de la placa y no el jitter del USB/Tk del host. Riesgo conocido: si el
firmware reinicia, t_ms vuelve a 0 → FrameRateMeter.tick() ya descarta timestamps
hacia atrás (skew defensivo), así que un reset solo congela las tasas un instante
hasta re-llenar la ventana; no rompe ni miente con un Hz gigante.

Consume CentralFrame (protocol_central.py, schema v1). NO manda comandos. Toda
la matemática vive en BoardRateMeters (rate_meter.py, ya testeado); este panel
solo la cablea y la dibuja. DEFENSIVO: getattr con default, nunca tumba Tk con un
frame corto.
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk
from typing import Any, Dict

from .panel import Panel
from .protocol_central import CentralFrame
from .rate_meter import BoardRateMeters
from .shell_theme import BAD, BG, FG, MUTED, OK, PANEL, WARN

# Umbral de flapping del heading_valid: por encima de esto, el heading está
# "bailando" (prende/apaga) y el control que dependa del yaw sufre → ALERTA.
FLAP_WARN_HZ = 2.0

# Ventana/stale de los medidores. 1 s de ventana = "Hz al último segundo".
# stale 1.5 s: si una fuente deja de actualizar, su Hz cae a 0 sin quedar pegado.
WINDOW_MS = 1000
STALE_MS = 1500

# epsilon por campo: cuánto tiene que moverse un valor para contar como "cambio".
# ball_x/my_x/my_y en mm → 5 mm filtra el ruido de visión/odometría. Los demás
# son discretos (contadores, enum de estado, bool) → epsilon 0 (cualquier cambio).
EPS_BALL = 5.0
EPS_POSE = 5.0


def fields_to_watch(f: CentralFrame) -> Dict[str, Any]:
    """Mapea un CentralFrame al dict de campos a observar (lógica PURA, testeable
    sin Tk). Las claves DEBEN coincidir con las registradas en watch_field().

    DEFENSIVO: getattr con default por si llega un frame corto/raro; nunca lanza.
    """
    snap = getattr(f, "snap", None)
    top = getattr(f, "top", None)
    down = getattr(f, "down", None)
    fsm = getattr(f, "fsm", None)
    return {
        # Contadores de los enlaces: su Hz de cambio = tasa de llegada del enlace.
        "top_fr": int(getattr(top, "fr", 0) or 0),
        "down_rx": int(getattr(down, "rx", 0) or 0),
        # WorldSnapshot.
        "ball_x": int(getattr(snap, "ball_x", 0) or 0),
        "my_x": int(getattr(snap, "my_x", 0) or 0),
        "my_y": int(getattr(snap, "my_y", 0) or 0),
        "heading_valid": int(bool(getattr(snap, "heading_valid", False))),
        "down_valid": int(bool(getattr(down, "valid", False))),
        # FSM.
        "fsm_state": str(getattr(fsm, "state", "")),
    }


class CentralRatesPanel(Panel):
    title = "Tasas CENTRAL"
    key = "central_tasas"
    icon = "⏲"
    board = "central"
    sends_commands = False

    # ── Layout ────────────────────────────────────────────────────────────────
    def build(self, parent: tk.Frame) -> None:
        self.meters = BoardRateMeters(window_ms=WINDOW_MS, stale_after_ms=STALE_MS)
        # Registrar qué campos vigilar (con su epsilon).
        self.meters.watch_field("top_fr")
        self.meters.watch_field("down_rx")
        self.meters.watch_field("ball_x", epsilon=EPS_BALL)
        self.meters.watch_field("my_x", epsilon=EPS_POSE)
        self.meters.watch_field("my_y", epsilon=EPS_POSE)
        self.meters.watch_field("heading_valid")
        self.meters.watch_field("down_valid")
        self.meters.watch_field("fsm_state")
        self._last_now_ms = 0

        self.header = tk.Label(parent, text="esperando datos…", anchor="w",
                               font=("Consolas", 11, "bold"), bg=BG, fg=FG,
                               padx=8, pady=6)
        self.header.pack(fill="x")

        body = ttk.Frame(parent, padding=8)
        body.pack(fill="both", expand=True)

        # ── Bloque 1: TASAS DE LLEGADA DE LOS ENLACES (los 3 que pidió el contrato).
        ttk.Label(body, text="TASA DE LLEGADA  (Hz)", style="Muted.TLabel"
                  ).grid(row=0, column=0, columnspan=3, sticky="w")
        links = ttk.Frame(body)
        links.grid(row=1, column=0, columnspan=3, sticky="w", pady=(2, 12))
        self._tile_top = self._make_tile(links, 0, "TOP → CENTRAL")
        self._tile_down = self._make_tile(links, 1, "DOWN → CENTRAL")
        self._tile_cen = self._make_tile(links, 2, "CENTRAL telemetría")

        # ── Bloque 2: TASAS DE CAMBIO de campos clave.
        ttk.Label(body, text="TASA DE CAMBIO  (Hz)", style="Muted.TLabel"
                  ).grid(row=2, column=0, columnspan=3, sticky="w")
        changes = ttk.Frame(body)
        changes.grid(row=3, column=0, columnspan=3, sticky="w", pady=(2, 12))
        self._tile_ball = self._make_tile(changes, 0, "pelota se mueve")
        self._tile_pose = self._make_tile(changes, 1, "pose cambia")
        self._tile_fsm = self._make_tile(changes, 2, "FSM transiciona")
        self._tile_line = self._make_tile(changes, 3, "línea entra/sale")

        # ── Bloque 3: ALERTA de flapping del heading (tile grande, dedicado).
        ttk.Label(body, text="ESTABILIDAD DEL HEADING", style="Muted.TLabel"
                  ).grid(row=4, column=0, columnspan=3, sticky="w")
        self._tile_flap = self._make_tile(
            ttk.Frame(body, padding=(0, 2)), 0, "heading_valid flapping", wide=True)
        self._tile_flap["frame"].master.grid(row=5, column=0, columnspan=3, sticky="w")

    def _make_tile(self, parent: tk.Widget, col: int, title: str,
                   wide: bool = False) -> dict:
        w = 360 if wide else 168
        frame = tk.Frame(parent, bg=PANEL, bd=1, relief="solid",
                         padx=10, pady=8, width=w, height=78)
        frame.grid(row=0, column=col, padx=4, pady=2, sticky="nw")
        frame.pack_propagate(False)
        title_lbl = tk.Label(frame, text=title, anchor="w",
                             font=("Segoe UI", 9, "bold"), bg=PANEL, fg=MUTED)
        title_lbl.pack(anchor="w")
        value_lbl = tk.Label(frame, text="—", anchor="w",
                             font=("Consolas", 18, "bold"), bg=PANEL, fg=FG)
        value_lbl.pack(anchor="w")
        return {"frame": frame, "title": title_lbl, "value": value_lbl}

    def _set_tile(self, tile: dict, text: str, fg: str = FG, bg: str = PANEL) -> None:
        tile["frame"].configure(bg=bg)
        tile["title"].configure(bg=bg)
        tile["value"].configure(text=text, fg=fg, bg=bg)

    # ── Render ────────────────────────────────────────────────────────────────
    def render(self, f: CentralFrame) -> None:
        # Reloj: t_ms del firmware (ver docstring del módulo). Default al último
        # conocido si llega un frame sin t_ms (defensivo).
        try:
            now_ms = int(getattr(f, "t_ms", self._last_now_ms))
        except (TypeError, ValueError):
            now_ms = self._last_now_ms
        self._last_now_ms = now_ms

        self.meters.on_frame(now_ms, **fields_to_watch(f))

        # — Tasas de llegada de los 3 enlaces —
        hz_top = self.meters.change_hz("top_fr", now_ms)
        hz_down = self.meters.change_hz("down_rx", now_ms)
        hz_cen = self.meters.frame_hz(now_ms)
        self._set_tile(self._tile_top, self._fmt(hz_top), self._color(hz_top))
        self._set_tile(self._tile_down, self._fmt(hz_down), self._color(hz_down))
        self._set_tile(self._tile_cen, self._fmt(hz_cen), self._color(hz_cen))

        # — Tasas de cambio —
        hz_ball = self.meters.change_hz("ball_x", now_ms)
        hz_pose = max(self.meters.change_hz("my_x", now_ms),
                      self.meters.change_hz("my_y", now_ms))
        hz_fsm = self.meters.change_hz("fsm_state", now_ms)
        hz_line = self.meters.change_hz("down_valid", now_ms)
        self._set_tile(self._tile_ball, self._fmt(hz_ball))
        self._set_tile(self._tile_pose, self._fmt(hz_pose))
        self._set_tile(self._tile_fsm, self._fmt(hz_fsm))
        self._set_tile(self._tile_line, self._fmt(hz_line))

        # — Flapping del heading: rojo si supera el umbral —
        hz_flap = self.meters.change_hz("heading_valid", now_ms)
        if hz_flap > FLAP_WARN_HZ:
            self._set_tile(self._tile_flap,
                           f"{hz_flap:5.1f} Hz  ⛔ FLAPPING",
                           fg="#ffffff", bg=BAD)
        elif hz_flap > 0.0:
            self._set_tile(self._tile_flap,
                           f"{hz_flap:5.1f} Hz  (parpadea)", fg=WARN)
        else:
            self._set_tile(self._tile_flap, "  0.0 Hz  ✓ estable", fg=OK)

        seq = getattr(f, "seq", 0)
        self.header.configure(
            text=(f"seq={seq}  t={now_ms} ms     "
                  f"TOP {hz_top:4.0f}Hz · DOWN {hz_down:4.0f}Hz · "
                  f"CENTRAL {hz_cen:4.0f}Hz"))

    # ── Helpers ───────────────────────────────────────────────────────────────
    @staticmethod
    def _fmt(hz: float) -> str:
        return f"{hz:5.1f} Hz"

    @staticmethod
    def _color(hz: float) -> str:
        """Verde si hay flujo sano, gris si está muerto (0 Hz)."""
        return OK if hz > 0.0 else MUTED
