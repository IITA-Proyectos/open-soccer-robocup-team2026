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
from .boot_status import BootStatusTracker
from .calibration import CalibrationAssistant, DEFAULT_MIN_MARGIN
from .protocol import Frame
from .sensor_health import Health, SensorHealthTracker
from .sources import FrameSource

RING_PX = 460
SENSOR_R = 11

# Umbral "naranja" de la grilla semáforo: por debajo de DEFAULT_MIN_MARGIN el
# sensor es débil; por debajo de esto directamente NO separa verde/blanco.
WEAK_MARGIN_ORANGE = 25

# Colores del semáforo de calibración (fondo, texto).
_SEM_GREEN = ("#1f7a33", "#eaffea")
_SEM_ORANGE = ("#b36b00", "#fff3d6")
_SEM_RED = ("#a02020", "#ffe0e0")


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
        self.boot = BootStatusTracker()   # qué calib cargó la placa al boot
        self.last: Optional[Frame] = None
        self.frame_count = 0
        self.last_seq = -1
        self.dropped = 0
        self._is_sim = getattr(source, "is_sim", False)

        root.title("IITA Soccer — Monitor de la base (DOWN) v1 — "
                   + source.describe())
        self._build_layout()
        self._compute_ring_transform()
        self._draw_static_ring()
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

        # Columna extra a la derecha: calibración guiada + semáforo de márgenes.
        calib_col = ttk.Frame(main)
        calib_col.grid(row=0, column=2, sticky="n", padx=(12, 0))
        self._build_calib_panel(calib_col)

        # Barra de estado.
        self.status = ttk.Label(self.root, text="iniciando…", anchor="w",
                                relief="sunken", padding=4)
        self.status.pack(fill="x", side="bottom")

    def _build_calib_panel(self, parent: ttk.Frame) -> None:
        """Panel "CALIBRAR (en orden)": 3 pasos guiados, veredicto grande y
        grilla semáforo con el margen verde/blanco de cada sensor."""
        ttk.Label(parent, text="CALIBRAR (en orden)",
                  font=("Segoe UI", 11, "bold")).pack(anchor="w")
        steps = [
            ("1· Capturar VERDE (robot sobre el verde)", "CAL CARPET"),
            ("2· Capturar BLANCO (sensores sobre la línea/hoja)", "CAL WHITE"),
            ("3· GUARDAR en el robot", "CAL SAVE"),
        ]
        for label, cmd in steps:
            ttk.Button(parent, text=label,
                       command=lambda c=cmd: self._send(c)).pack(
                fill="x", pady=2)

        # Veredicto grande + compuerta data_valid.
        self.verdict_lbl = tk.Label(parent, text="esperando datos…",
                                    font=("Segoe UI", 11, "bold"),
                                    bg="#222", fg="#ccc", wraplength=300,
                                    justify="left", padx=6, pady=6)
        self.verdict_lbl.pack(fill="x", pady=(10, 2))
        self.valid_lbl = ttk.Label(parent, text="data_valid=?")
        self.valid_lbl.pack(anchor="w")

        # Grilla semáforo 8×4: margen |blanco−verde| por sensor.
        ttk.Label(parent,
                  text=f"Margen por sensor (verde ≥{DEFAULT_MIN_MARGIN} · "
                       f"naranja ≥{WEAK_MARGIN_ORANGE} · rojo <{WEAK_MARGIN_ORANGE})",
                  foreground="#888").pack(anchor="w", pady=(8, 2))
        grid = ttk.Frame(parent)
        grid.pack()
        self._margin_cells = []
        for i in range(self.n):
            cell = tk.Label(grid, text=f"S{i:02d}\n—", width=5,
                            font=("Consolas", 8), bg="#202020", fg="#888",
                            relief="ridge", bd=1)
            cell.grid(row=i // 8, column=i % 8, padx=1, pady=1)
            self._margin_cells.append(cell)

        # Botones avanzados (secundarios, para quien sabe lo que hace).
        ttk.Label(parent, text="Avanzado:", foreground="#888").pack(
            anchor="w", pady=(10, 2))
        adv = ttk.Frame(parent)
        adv.pack(anchor="w")
        for i, (label, cmd) in enumerate([
                ("Auto ON", "CAL AUTO ON"),
                ("Auto OFF", "CAL AUTO OFF"),
                ("CAL LOAD", "CAL LOAD"),
                ("Reset OTOS", "OTOS RESET")]):
            ttk.Button(adv, text=label, width=10,
                       command=lambda c=cmd: self._send(c)).grid(
                row=0, column=i, padx=2)

    def _send(self, cmd: str) -> None:
        if self._is_sim:
            self._set_status("SIMULADOR: comando no enviado al robot")
            return
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
        self.health.update(f.ring.raw)
        self.calib.update(f.ring.raw)
        if self.recorder is not None:
            self.recorder.write(f)
        self.last = f

    def _drain_errors(self) -> bool:
        msgs = []
        while True:
            try:
                msgs.append(self.source.errors.get_nowait())
            except Exception:  # queue.Empty u otro
                break
        if msgs:
            self._set_status("⚠ " + " | ".join(msgs[-2:]))
            return True
        return False

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
        self._render_calib_panel(f)
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

    def _render_calib_panel(self, f: Frame) -> None:
        """Semáforo de márgenes + veredicto de la calibración VIGENTE."""
        margins = f.ring.margins
        weak = []
        for i, cell in enumerate(self._margin_cells):
            m = margins[i] if i < len(margins) else 0
            if m >= DEFAULT_MIN_MARGIN:
                bg, fg = _SEM_GREEN
            elif m >= WEAK_MARGIN_ORANGE:
                bg, fg = _SEM_ORANGE
            else:
                bg, fg = _SEM_RED
            if m < DEFAULT_MIN_MARGIN:
                weak.append(i)
            cell.config(text=f"S{i:02d}\n{m}", bg=bg, fg=fg)

        if not weak:
            bg, fg = _SEM_GREEN
            text = (f"CALIB OK — {self.n}/{self.n} sensores "
                    f"separan verde/blanco")
        elif len(weak) <= 4:
            bg, fg = _SEM_ORANGE
            ids = ", ".join(f"S{i:02d}" for i in weak)
            text = f"DÉBILES: {ids} — el robot juega con los demás"
        else:
            bg, fg = _SEM_RED
            text = ("CALIB INSUFICIENTE — recalibrar "
                    "(¿batería baja? ¿hoja no cubre todo?)")
        self.verdict_lbl.config(text=text, bg=bg, fg=fg)
        self.valid_lbl.config(
            text=f"data_valid={'SÍ' if f.line.valid else 'NO'}")

    def _render_health_panel(self, f: Frame, statuses) -> None:
        probs = [s for s in statuses if s.is_problem]
        margins = f.ring.margins
        weak = [(i, m) for i, m in enumerate(margins) if m < DEFAULT_MIN_MARGIN]
        waiting = [s for s in statuses if s.is_waiting]
        lines = []
        if probs:
            ids = ", ".join(f"S{s.index}({s.health.value})" for s in probs)
            lines.append(f"PROBLEMA (muerto/pegado): {ids}")
        elif waiting and not self.health.is_moving():
            lines.append(f"robot QUIETO: movelo sobre la línea para chequear los "
                         f"sensores ({len(waiting)} en espera)")
        else:
            lines.append("sensores: todos responden ✓")
        if self.calib.active:
            sus = self.calib.suspects()
            lines.append(f"auto-calib ACTIVA · {self.calib._samples} muestras"
                         f" · sospechosos: {len(sus)}")
        if weak:
            ids = ", ".join(f"S{i}={m}" for i, m in weak[:6])
            lines.append(f"margen bajo (<{DEFAULT_MIN_MARGIN}): {ids}")
        self._set_text(self.health_txt, "\n".join(lines) + "\n")

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


def run(source: FrameSource, n: int = 32, recorder=None) -> None:
    root = tk.Tk()
    MonitorApp(root, source, n=n, recorder=recorder)
    root.mainloop()
