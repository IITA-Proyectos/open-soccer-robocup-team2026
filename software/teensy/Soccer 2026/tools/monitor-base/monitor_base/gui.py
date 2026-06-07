"""gui.py — Interfaz gráfica (Tkinter) de la app de base.

Dibuja el anillo de 32 sensores en su geometría real, la línea detectada, la
interpretación que va a la CENTRAL (LineStatusV2) y la odometría OTOS; y ofrece
botones de calibración. Se alimenta de una FrameSource (serial/replay/sim).

tkinter es stdlib pero necesita display: para correr SIN ventana (CI / smoke)
usar el modo --selftest de __main__.py, que NO importa este módulo.
"""
from __future__ import annotations

import math
import tkinter as tk
from tkinter import ttk
from typing import Optional

from . import geometry
from .calibration import CalibrationAssistant
from .protocol import Frame
from .sensor_health import Health, SensorHealthTracker
from .sources import FrameSource

RING_PX = 460
SENSOR_R = 11


def _heat_color(raw: int) -> str:
    t = max(0.0, min(1.0, raw / 900.0))
    v = int(40 + 215 * t)
    return f"#{v:02x}{v:02x}{v:02x}"


class MonitorApp:
    def __init__(self, root: tk.Tk, source: FrameSource, n: int = 32,
                 poll_ms: int = 50, recorder=None):
        self.root = root
        self.source = source
        self.n = n
        self.poll_ms = poll_ms
        self.recorder = recorder
        self.health = SensorHealthTracker(n=n)
        self.calib = CalibrationAssistant(n=n)
        self.last: Optional[Frame] = None
        self.frame_count = 0
        self.last_seq = -1
        self.dropped = 0

        root.title("IITA Soccer — Monitor de la base (DOWN) v1")
        self._build_layout()
        self._compute_ring_transform()
        self._draw_static_ring()
        self.source.start()
        self.root.after(self.poll_ms, self._tick)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ── Layout ────────────────────────────────────────────────────────────
    def _build_layout(self) -> None:
        main = ttk.Frame(self.root, padding=8)
        main.pack(fill="both", expand=True)

        # Izquierda: canvas del anillo.
        left = ttk.Frame(main)
        left.grid(row=0, column=0, sticky="n")
        self.canvas = tk.Canvas(left, width=RING_PX, height=RING_PX,
                                bg="#101418", highlightthickness=0)
        self.canvas.pack()
        ttk.Label(left, text="Anillo de 32 sensores — +Y = frente del robot",
                  foreground="#888").pack(pady=(4, 0))

        # Derecha: paneles de texto.
        right = ttk.Frame(main)
        right.grid(row=0, column=1, sticky="n", padx=(12, 0))

        self.line_txt = tk.Text(right, width=42, height=12, font=("Consolas", 10),
                                bg="#0c0f12", fg="#d8e0e8", relief="flat")
        ttk.Label(right, text="LÍNEA → CENTRAL (LineStatusV2)").pack(anchor="w")
        self.line_txt.pack(pady=(0, 8))

        self.otos_txt = tk.Text(right, width=42, height=9, font=("Consolas", 10),
                                bg="#0c0f12", fg="#d8e0e8", relief="flat")
        ttk.Label(right, text="ODOMETRÍA (OTOS)").pack(anchor="w")
        self.otos_txt.pack(pady=(0, 8))

        self.health_txt = tk.Text(right, width=42, height=7, font=("Consolas", 10),
                                  bg="#0c0f12", fg="#f0c0c0", relief="flat")
        ttk.Label(right, text="SALUD DE SENSORES / CALIBRACIÓN").pack(anchor="w")
        self.health_txt.pack()

        # Abajo: botones de calibración.
        bottom = ttk.Frame(main, padding=(0, 8))
        bottom.grid(row=1, column=0, columnspan=2, sticky="we")
        cmds = [
            ("Calibrar CARPET", "CAL CARPET"),
            ("Calibrar BLANCO", "CAL WHITE"),
            ("Auto-calib ON", "CAL AUTO ON"),
            ("Auto-calib OFF", "CAL AUTO OFF"),
            ("Guardar EEPROM", "CAL SAVE"),
            ("Cargar EEPROM", "CAL LOAD"),
            ("Reset OTOS", "OTOS RESET"),
        ]
        for i, (label, cmd) in enumerate(cmds):
            ttk.Button(bottom, text=label,
                       command=lambda c=cmd: self._send(c)).grid(
                row=0, column=i, padx=3)

        # Barra de estado.
        self.status = ttk.Label(self.root, text="iniciando…", anchor="w",
                                relief="sunken", padding=4)
        self.status.pack(fill="x", side="bottom")

    def _send(self, cmd: str) -> None:
        self.source.send(cmd)
        if cmd == "CAL AUTO ON":
            self.calib.start()
        elif cmd == "CAL AUTO OFF":
            self.calib.stop()
        self._set_status(f"→ comando enviado: {cmd}")

    # ── Geometría del anillo en pantalla ──────────────────────────────────
    def _compute_ring_transform(self) -> None:
        min_x, min_y, max_x, max_y = geometry.bounds()
        span = max(max_x - min_x, max_y - min_y) + 4 * SENSOR_R
        self._scale = (RING_PX - 2 * SENSOR_R - 8) / span
        self._cx = RING_PX / 2
        self._cy = RING_PX / 2

    def _to_px(self, x_mm: float, y_mm: float):
        # +X derecha, +Y adelante → en pantalla +Y es hacia ARRIBA (y decrece).
        px = self._cx + x_mm * self._scale
        py = self._cy - y_mm * self._scale
        return px, py

    def _draw_static_ring(self) -> None:
        self._dots = []
        for i in range(self.n):
            x, y = geometry.SENSOR_POS[i]
            px, py = self._to_px(x, y)
            dot = self.canvas.create_oval(px - SENSOR_R, py - SENSOR_R,
                                          px + SENSOR_R, py + SENSOR_R,
                                          fill="#202020", outline="#404040",
                                          width=2)
            self.canvas.create_text(px, py, text=str(i), fill="#666",
                                    font=("Consolas", 7))
            self._dots.append(dot)
        # Flecha de dirección de línea + marcador de frente.
        self.canvas.create_line(self._cx, self._cy, self._cx, self._cy - 60,
                                fill="#2a3a2a", dash=(3, 3))
        self._line_arrow = self.canvas.create_line(
            self._cx, self._cy, self._cx, self._cy, fill="#ffd24a",
            width=4, arrow="last", state="hidden")

    # ── Loop de actualización ─────────────────────────────────────────────
    def _tick(self) -> None:
        frames = self.source.poll()
        for f in frames:
            self._consume(f)
        if frames:
            self._render(frames[-1])
        self._drain_errors()
        self.root.after(self.poll_ms, self._tick)

    def _consume(self, f: Frame) -> None:
        self.frame_count += 1
        if self.last_seq >= 0:
            gap = f.seq - self.last_seq - 1
            if gap > 0:
                self.dropped += gap
        self.last_seq = f.seq
        self.health.update(f.ring.raw)
        self.calib.update(f.ring.raw)
        if self.recorder is not None:
            self.recorder.write(f)
        self.last = f

    def _drain_errors(self) -> None:
        msgs = []
        while True:
            try:
                msgs.append(self.source.errors.get_nowait())
            except Exception:  # queue.Empty u otro
                break
        if msgs:
            self._set_status("⚠ " + " | ".join(msgs[-2:]))

    def _render(self, f: Frame) -> None:
        statuses = self.health.status()
        for i in range(self.n):
            white = f.ring.white[i] if i < len(f.ring.white) else False
            raw = f.ring.raw[i] if i < len(f.ring.raw) else 0
            st = statuses[i]
            fill = "#ffe27a" if white else _heat_color(raw)
            if st.is_problem:
                outline, width = "#ff4040", 3
            elif white:
                outline, width = "#ffaa00", 3
            else:
                outline, width = "#404040", 2
            self.canvas.itemconfig(self._dots[i], fill=fill, outline=outline,
                                   width=width)

        # Flecha de línea.
        if f.line.present and f.line.angle_deg is not None:
            a = math.radians(f.line.angle_deg)  # 0=frente(+Y, arriba), horario+
            length = 60 + (f.line.penetration_mm or 0) * 0.4
            ex = self._cx + math.sin(a) * length
            ey = self._cy - math.cos(a) * length
            self.canvas.coords(self._line_arrow, self._cx, self._cy, ex, ey)
            self.canvas.itemconfig(self._line_arrow, state="normal")
        else:
            self.canvas.itemconfig(self._line_arrow, state="hidden")

        self._render_line_panel(f)
        self._render_otos_panel(f)
        self._render_health_panel(f, statuses)
        rate = ""
        self._set_status(
            f"src OK · seq={f.seq} · frames={self.frame_count} · "
            f"perdidos={self.dropped} · v{f.v}{rate}")

    def _render_line_panel(self, f: Frame) -> None:
        ln = f.line
        ang = "N/A" if ln.angle_deg is None else f"{ln.angle_deg:+.1f}°"
        esc = "N/A" if ln.escape_deg is None else f"{ln.escape_deg:+.1f}°"
        pen = "N/A" if ln.penetration_mm is None else f"{ln.penetration_mm} mm"
        xt = "N/A" if ln.cross_track_mm is None else f"{ln.cross_track_mm:+d} mm"
        flags = ", ".join(ln.flag_names) if ln.flag_names else "—"
        gate = "VÁLIDO" if ln.valid else "*** NO VÁLIDO (data_valid=0) ***"
        text = (
            f"estado      : {gate}\n"
            f"línea pres. : {'SÍ' if ln.present else 'no'}\n"
            f"ángulo      : {ang}\n"
            f"escape      : {esc}\n"
            f"penetración : {pen}\n"
            f"cross-track : {xt}\n"
            f"sensores    : {ln.sensors_on_line}/32\n"
            f"calidad     : {ln.quality}/100\n"
            f"edad muestra: {ln.sample_age_ms} ms\n"
            f"eventos     : {flags}\n"
        )
        self._set_text(self.line_txt, text)

    def _render_otos_panel(self, f: Frame) -> None:
        o = f.otos
        led = lambda ok: "●ok" if ok else "○--"
        text = (
            f"OTOS conectados : {o.n}   L:{led(o.left_ok)}  R:{led(o.right_ok)}\n"
            f"posición  x / y : {o.x_mm:+8.1f} / {o.y_mm:+8.1f} mm\n"
            f"heading         : {o.heading_deg:+7.1f}°\n"
            f"vel  vx / vy    : {o.vx_mm_s:+7.1f} / {o.vy_mm_s:+7.1f} mm/s\n"
            f"omega           : {o.omega_rad_s:+7.3f} rad/s\n"
            f"slip            : {o.slip_mm_s:7.1f} mm/s\n"
            f"levantado       : {'SÍ' if f.lifted else 'no'}\n"
        )
        self._set_text(self.otos_txt, text)

    def _render_health_panel(self, f: Frame, statuses) -> None:
        probs = [s for s in statuses if s.is_problem]
        margins = f.ring.margins
        weak = [(i, m) for i, m in enumerate(margins) if m < 40]
        lines = []
        if probs:
            ids = ", ".join(f"S{s.index}({s.health.value})" for s in probs)
            lines.append(f"PROBLEMA: {ids}")
        else:
            lines.append("sensores: todos responden ✓")
        if self.calib.active:
            sus = self.calib.suspects()
            lines.append(f"auto-calib ACTIVA · {self.calib._samples} muestras"
                         f" · sospechosos: {len(sus)}")
        if weak:
            ids = ", ".join(f"S{i}={m}" for i, m in weak[:6])
            lines.append(f"margen bajo (<40): {ids}")
        self._set_text(self.health_txt, "\n".join(lines) + "\n")

    # ── Helpers ───────────────────────────────────────────────────────────
    def _set_text(self, widget: tk.Text, text: str) -> None:
        widget.delete("1.0", "end")
        widget.insert("1.0", text)

    def _set_status(self, text: str) -> None:
        self.status.config(text=text)

    def _on_close(self) -> None:
        try:
            self.source.stop()
            if self.recorder is not None:
                self.recorder.close()
        finally:
            self.root.destroy()


def run(source: FrameSource, n: int = 32, recorder=None) -> None:
    root = tk.Tk()
    MonitorApp(root, source, n=n, recorder=recorder)
    root.mainloop()
