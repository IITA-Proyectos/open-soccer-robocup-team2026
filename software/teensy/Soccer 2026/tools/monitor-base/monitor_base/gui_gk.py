"""gui_gk.py — Vista de ARQUERO (seguidor de línea + OTOS) de la app de abajo.

Pensada para PROBAR EN BANCO el seguidor de línea del arquero, que se queda
centrado sobre la línea de su arco (atrás) y barre lateralmente. Muestra:

  • Medidor de CROSS-TRACK grande (mm a la línea, objetivo 0 = centrado) + el
    ángulo de la línea (paralelismo).
  • Arco TRASERO del anillo resaltado (los sensores que ven la línea del arco) +
    flecha de la línea detectada.
  • Estela de la trayectoria por ODOMETRÍA OTOS (pose fusionada + cada OTOS
    izq/der por separado, para ver el diferencial / que avance derecho).

Consume Frame de la base (protocol.py, schema v2). Sin display: no importar este
módulo (usar --selftest).
"""
from __future__ import annotations

import math
import tkinter as tk
from collections import deque
from tkinter import ttk
from typing import Optional

from . import geometry
from .boot_status import BootStatusTracker
from .protocol import Frame
from .sources import FrameSource

# Sensores "traseros" = los que miran hacia atrás del robot (y < umbral): los que
# ven la línea del arco cuando el arquero está de frente a la cancha.
BACK_INDICES = [i for i in range(geometry.SENSOR_COUNT)
                if geometry.SENSOR_POS[i][1] < -5.0]

TRAIL_CANVAS = 300
RING_CANVAS = 300
GAUGE_W, GAUGE_H = 620, 56
XTRACK_RANGE_MM = 150.0     # fondo de escala del medidor de cross-track


class MonitorGkApp:
    def __init__(self, root: tk.Tk, source: FrameSource, poll_ms: int = 50,
                 recorder=None):
        self.root = root
        self.source = source
        self.poll_ms = poll_ms
        self.recorder = recorder
        self.last: Optional[Frame] = None
        self.frame_count = 0
        self.last_seq = -1
        self.dropped = 0
        self.boot = BootStatusTracker()   # qué calib cargó la placa al boot
        self.trail = deque(maxlen=500)   # (x_mm, y_mm) pose fusionada
        self._is_sim = getattr(source, "is_sim", False)

        root.title("IITA Soccer — Monitor del ARQUERO (línea + OTOS) v1 — "
                   + source.describe())
        self._build_layout()
        self.source.start()
        self.root.after(self.poll_ms, self._tick)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ── Layout ────────────────────────────────────────────────────────────
    def _build_layout(self) -> None:
        if self._is_sim:
            tk.Label(self.root,
                     text="⚠ SIMULADOR — datos FALSOS, no es el robot",
                     bg="#b32d00", fg="#ffe27a",
                     font=("Segoe UI", 12, "bold"), pady=5).pack(fill="x")

        main = ttk.Frame(self.root, padding=8)
        main.pack(fill="both", expand=True)

        ttk.Label(main, text="CROSS-TRACK — distancia a la línea (objetivo 0 = centrado)"
                  ).grid(row=0, column=0, columnspan=2, sticky="w")
        self.gauge = tk.Canvas(main, width=GAUGE_W, height=GAUGE_H,
                               bg="#0c0f12", highlightthickness=0)
        self.gauge.grid(row=1, column=0, columnspan=2, pady=(0, 8))

        # Estela OTOS (izq) + arco trasero (der).
        lf = ttk.Frame(main); lf.grid(row=2, column=0, sticky="n")
        ttk.Label(lf, text="Trayectoria por OTOS  (●fusión ·azul=izq ·rojo=der)"
                  ).pack()
        self.trail_c = tk.Canvas(lf, width=TRAIL_CANVAS, height=TRAIL_CANVAS,
                                 bg="#0b1014", highlightthickness=0)
        self.trail_c.pack()

        rf = ttk.Frame(main); rf.grid(row=2, column=1, sticky="n", padx=(10, 0))
        ttk.Label(rf, text="Anillo — sensores TRASEROS (línea del arco) resaltados"
                  ).pack()
        self.ring_c = tk.Canvas(rf, width=RING_CANVAS, height=RING_CANVAS,
                                bg="#101418", highlightthickness=0)
        self.ring_c.pack()
        self._draw_static_ring()

        # Paneles de texto.
        pf = ttk.Frame(main); pf.grid(row=3, column=0, columnspan=2, sticky="we",
                                      pady=(8, 0))
        self.line_txt = tk.Text(pf, width=44, height=8, font=("Consolas", 10),
                                bg="#0c0f12", fg="#d8e0e8", relief="flat")
        self.line_txt.grid(row=0, column=0, padx=(0, 8))
        self.otos_txt = tk.Text(pf, width=46, height=8, font=("Consolas", 10),
                                bg="#0c0f12", fg="#d8e0e8", relief="flat")
        self.otos_txt.grid(row=0, column=1)

        bottom = ttk.Frame(main, padding=(0, 8))
        bottom.grid(row=4, column=0, columnspan=2, sticky="we")
        for i, (label, cmd) in enumerate([
                ("Reset OTOS (0,0,0)", "OTOS RESET"),
                ("Limpiar estela", "__clear__"),
                ("Stream ON", "STREAM ON"),
                ("Stream OFF", "STREAM OFF")]):
            ttk.Button(bottom, text=label,
                       command=lambda c=cmd: self._action(c)).grid(row=0, column=i, padx=3)

        self.status = ttk.Label(self.root, text="iniciando…", anchor="w",
                                relief="sunken", padding=4)
        self.status.pack(fill="x", side="bottom")

    def _action(self, cmd: str) -> None:
        if cmd == "__clear__":
            self.trail.clear()
            self._set_status("estela limpiada")
            return
        if self._is_sim:
            self._set_status("SIMULADOR: comando no enviado al robot")
            return
        self.source.send(cmd)
        if cmd == "OTOS RESET":
            self.trail.clear()
        self._set_status(f"→ comando enviado: {cmd}")

    def _draw_static_ring(self) -> None:
        c = self.ring_c
        cx = cy = RING_CANVAS / 2
        min_x, min_y, max_x, max_y = geometry.bounds()
        span = max(max_x - min_x, max_y - min_y) + 24
        self._ring_scale = (RING_CANVAS - 24) / span
        self._ring_cx, self._ring_cy = cx, cy
        self._dots = []
        back = set(BACK_INDICES)
        for i in range(geometry.SENSOR_COUNT):
            x, y = geometry.SENSOR_POS[i]
            px = cx + x * self._ring_scale
            py = cy - y * self._ring_scale
            r = 8 if i in back else 4           # traseros más grandes
            outline = "#3a86ff" if i in back else "#333"
            dot = c.create_oval(px - r, py - r, px + r, py + r,
                                fill="#202020", outline=outline,
                                width=2 if i in back else 1)
            self._dots.append((dot, r))
        c.create_line(cx, cy, cx, cy - 26, fill="#2a3a2a", dash=(2, 2))
        self._line_arrow = c.create_line(cx, cy, cx, cy, fill="#ffd24a", width=4,
                                         arrow="last", state="hidden")

    # ── Loop ──────────────────────────────────────────────────────────────
    def _tick(self) -> None:
        frames = self.source.poll()
        for f in frames:
            self._consume(f)
        if hasattr(self.source, "last_text"):
            bt = self.source.last_text()
            if bt:
                self._last_board = bt
                self.boot.update(bt)
        had_error = self._drain_errors()
        if frames:
            self._render(frames[-1])
        elif not had_error:
            bt = getattr(self, "_last_board", None)
            if bt:
                self._set_status(f"esperando datos… la placa dice: «{bt}»")
            elif self.last is None:
                self._set_status("esperando datos… ¿flasheaste down_debug_telemetry y "
                                 "conectaste la batería? (la placa tarda ~2 s en bootear)")
        self.root.after(self.poll_ms, self._tick)

    def _consume(self, f: Frame) -> None:
        self.frame_count += 1
        if self.last_seq >= 0:
            gap = f.seq - self.last_seq - 1
            if gap > 0:
                self.dropped += gap
        self.last_seq = f.seq
        if self.recorder is not None:
            self.recorder.write(f)
        if f.otos.has_pose:
            self.trail.append((f.otos.x_mm, f.otos.y_mm))
        self.last = f

    def _drain_errors(self) -> bool:
        msgs = []
        while True:
            try:
                msgs.append(self.source.errors.get_nowait())
            except Exception:
                break
        if msgs:
            self._set_status("⚠ " + " | ".join(msgs[-2:]))
            return True
        return False

    def _render(self, f: Frame) -> None:
        self._render_gauge(f)
        self._render_ring(f)
        self._render_trail(f)
        self._render_panels(f)
        self._set_status(f"src OK · seq={f.seq} · frames={self.frame_count} · "
                         f"perdidos={self.dropped} · v{f.v}")

    def _render_gauge(self, f: Frame) -> None:
        g = self.gauge
        g.delete("dyn")
        mid = GAUGE_W / 2
        g.create_line(mid, 4, mid, GAUGE_H - 4, fill="#3a6", tags="dyn")  # cero
        g.create_text(mid, GAUGE_H - 8, text="0", fill="#3a6",
                      font=("Consolas", 8), tags="dyn")
        xt = f.line.cross_track_mm
        if not f.line.valid or xt is None:
            g.create_text(mid, GAUGE_H / 2, text="cross-track N/A",
                          fill="#888", font=("Consolas", 12), tags="dyn")
            return
        frac = max(-1.0, min(1.0, xt / XTRACK_RANGE_MM))
        x = mid + frac * (GAUGE_W / 2 - 12)
        a = abs(xt)
        color = "#2ecc40" if a < 20 else ("#ffdc00" if a < 60 else "#ff4136")
        g.create_rectangle(min(mid, x), 10, max(mid, x), GAUGE_H - 18,
                           fill=color, outline="", tags="dyn")
        g.create_text(x, GAUGE_H / 2, text=f"{xt:+d} mm", fill="#fff",
                      font=("Consolas", 13, "bold"), tags="dyn")

    def _render_ring(self, f: Frame) -> None:
        c = self.ring_c
        back = set(BACK_INDICES)
        for i, (dot, r) in enumerate(self._dots):
            white = f.ring.white[i] if i < len(f.ring.white) else False
            if white:
                fill = "#ffe27a"
                outline = "#ffaa00"
            else:
                fill = "#243" if i in back else "#1c1c1c"
                outline = "#3a86ff" if i in back else "#333"
            c.itemconfig(dot, fill=fill, outline=outline)
        if f.line.present and f.line.angle_deg is not None:
            a = math.radians(f.line.angle_deg)   # 0=frente, horario+
            length = 70 + (f.line.penetration_mm or 0) * 0.4
            ex = self._ring_cx + math.sin(a) * length
            ey = self._ring_cy - math.cos(a) * length
            c.coords(self._line_arrow, self._ring_cx, self._ring_cy, ex, ey)
            c.itemconfig(self._line_arrow, state="normal")
        else:
            c.itemconfig(self._line_arrow, state="hidden")

    def _render_trail(self, f: Frame) -> None:
        c = self.trail_c
        c.delete("dyn")
        pts = list(self.trail)
        if not pts:
            c.create_text(TRAIL_CANVAS / 2, TRAIL_CANVAS / 2,
                          text="(moviendo el robot\naparece la estela)",
                          fill="#556", font=("Consolas", 9), tags="dyn")
            return
        xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
        # incluir las poses por-OTOS actuales en el encuadre
        xs += [f.otos.left_x_mm, f.otos.right_x_mm]
        ys += [f.otos.left_y_mm, f.otos.right_y_mm]
        minx, maxx, miny, maxy = min(xs), max(xs), min(ys), max(ys)
        span = max(maxx - minx, maxy - miny, 200.0)   # span mínimo 200 mm
        scale = (TRAIL_CANVAS - 30) / span
        ox = (minx + maxx) / 2
        oy = (miny + maxy) / 2

        def to_px(x, y):
            return (TRAIL_CANVAS / 2 + (x - ox) * scale,
                    TRAIL_CANVAS / 2 - (y - oy) * scale)

        # estela
        flat = []
        for (x, y) in pts:
            px, py = to_px(x, y)
            flat += [px, py]
        if len(flat) >= 4:
            c.create_line(*flat, fill="#2f6f4f", width=2, tags="dyn")
        # OTOS izq/der actuales
        if f.otos.left_ok:
            lx, ly = to_px(f.otos.left_x_mm, f.otos.left_y_mm)
            c.create_oval(lx - 4, ly - 4, lx + 4, ly + 4, fill="#3a86ff",
                          outline="", tags="dyn")
        if f.otos.right_ok:
            rx, ry = to_px(f.otos.right_x_mm, f.otos.right_y_mm)
            c.create_oval(rx - 4, ry - 4, rx + 4, ry + 4, fill="#ff4136",
                          outline="", tags="dyn")
        # robot (pose fusionada) + flecha de heading (CCW+)
        bx, by = to_px(f.otos.x_mm, f.otos.y_mm)
        c.create_oval(bx - 6, by - 6, bx + 6, by + 6, fill="#2ecc40",
                      outline="#dfd", tags="dyn")
        a = math.radians(f.otos.heading_deg)
        c.create_line(bx, by, bx - math.sin(a) * 22, by - math.cos(a) * 22,
                      fill="#dfd", width=2, arrow="last", tags="dyn")

    def _render_panels(self, f: Frame) -> None:
        ln, o = f.line, f.otos
        back_on = sum(1 for i in BACK_INDICES
                      if i < len(f.ring.white) and f.ring.white[i])
        ang = "N/A" if ln.angle_deg is None else f"{ln.angle_deg:+.1f}°"
        xt = "N/A" if ln.cross_track_mm is None else f"{ln.cross_track_mm:+d} mm"
        pen = "N/A" if ln.penetration_mm is None else f"{ln.penetration_mm} mm"
        gate = "VÁLIDO" if ln.valid else "*** NO VÁLIDO (data_valid=0) ***"
        self._set_text(self.line_txt,
            f"estado         : {gate}\n"
            f"línea presente : {'SÍ' if ln.present else 'no'}\n"
            f"CROSS-TRACK    : {xt}   (objetivo 0)\n"
            f"ángulo línea   : {ang}   (paralelo = alineado)\n"
            f"penetración    : {pen}\n"
            f"sensores línea : {ln.sensors_on_line}/32   (traseros {back_on}/{len(BACK_INDICES)})\n"
            f"eventos        : {', '.join(ln.flag_names) or '—'}\n")

        self._set_text(self.otos_txt,
            f"fusión  x/y/hdg : {o.x_mm:+7.1f} {o.y_mm:+7.1f} {o.heading_deg:+6.1f}°\n"
            f"OTOS izq x/y/h  : {o.left_x_mm:+7.1f} {o.left_y_mm:+7.1f} {o.left_heading_deg:+6.1f}°  [{'ok' if o.left_ok else '--'}]\n"
            f"OTOS der x/y/h  : {o.right_x_mm:+7.1f} {o.right_y_mm:+7.1f} {o.right_heading_deg:+6.1f}°  [{'ok' if o.right_ok else '--'}]\n"
            f"vel  vx/vy/w    : {o.vx_mm_s:+6.1f} {o.vy_mm_s:+6.1f} {o.omega_rad_s:+5.2f}\n"
            f"slip (difer.)   : {o.slip_mm_s:6.1f} mm/s\n"
            f"levantado       : {'SÍ' if f.lifted else 'no'}\n")

    # ── Helpers ───────────────────────────────────────────────────────────
    def _set_text(self, widget: tk.Text, text: str) -> None:
        widget.delete("1.0", "end")
        widget.insert("1.0", text)

    def _set_status(self, text: str) -> None:
        # Antepone de forma persistente qué calibración cargó la placa al boot
        # (de EEPROM ✓ vs FAIL-SAFE), para no perder de vista qué está operativo.
        calib = self.boot.last_calib_status()
        if calib:
            text = f"[{calib}]  {text}"
        self.status.config(text=text)

    def _on_close(self) -> None:
        try:
            self.source.stop()
            if self.recorder is not None:
                self.recorder.close()
        finally:
            self.root.destroy()


def run_gk(source: FrameSource, recorder=None) -> None:
    root = tk.Tk()
    MonitorGkApp(root, source, recorder=recorder)
    root.mainloop()
