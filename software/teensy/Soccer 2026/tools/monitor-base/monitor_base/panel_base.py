"""panel_base.py — Panel de la placa BASE (DOWN): anillo de línea + CALIBRACIÓN.

Refactor de gui.MonitorApp a Panel embebible: el mismo anillo de 32 sensores en
su geometría real, la flecha de línea, los paneles LineStatusV2 + OTOS y TODO el
panel de calibración (botones CAL *, sliders de sensibilidad global/por-sensor,
toggles SENSOR i ON|OFF, barras de cercanía al umbral, inspector). Pero SIN
ventana/loop/fuente/banner/status-bar propios: eso lo provee el shell vía el
PanelContext. Los comandos van por self.ctx.send; los eventos por self.ctx.log.

El estado de los trackers puros (SensorHealthTracker, CalibrationAssistant) vive
en la instancia, alimentado en cada render(f) con f.ring.raw.
"""
from __future__ import annotations

import math
import tkinter as tk
from tkinter import ttk
from typing import Optional

from . import geometry
from .calibration import CalibrationAssistant, DEFAULT_MIN_MARGIN
from .panel import Panel
from .protocol import Frame
from .sensor_health import SensorHealthTracker

RING_PX = 400
SENSOR_R = 11

# Grilla de barras "cercanía al umbral" (8 columnas × 4 filas = 32 sensores).
_BARS_COLS = 8
_BARS_ROWS = 4
_BARS_CW = 50   # ancho de celda px
_BARS_CH = 34   # alto de celda px
_BARS_W = _BARS_COLS * _BARS_CW   # ancho total de la grilla (= ancho de la columna calib)

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


class BasePanel(Panel):
    title = "Base"
    key = "base"
    icon = "◉"
    board = "down"
    sends_commands = True

    def build(self, parent: tk.Frame) -> None:
        self.n = 32
        self.health = SensorHealthTracker(n=self.n)
        self.calib = CalibrationAssistant(n=self.n)
        self.last: Optional[Frame] = None
        self.frame_count = 0
        self.last_seq = -1
        self.dropped = 0
        self._selected: Optional[int] = None   # sensor seleccionado (inspector)
        self._global_synced = False            # ¿ya sincronicé el slider global con la placa?
        # Último valor de sensibilidad ENVIADO (dedup: mandar solo al cambiar el
        # entero mientras se arrastra, no en cada sub-pixel ni re-enviar al seleccionar).
        self._last_global_sent = 0
        self._last_persensor_sent = 0

        main = ttk.Frame(parent, padding=6)
        main.pack(fill="both", expand=True)

        # Izquierda: canvas del anillo.
        left = ttk.Frame(main)
        left.grid(row=0, column=0, sticky="n")
        self.canvas = tk.Canvas(left, width=RING_PX, height=RING_PX,
                                bg="#101418", highlightthickness=0)
        self.canvas.pack()
        self.canvas.bind("<Button-1>", self._on_canvas_click)               # click → inspector
        self.canvas.bind("<Double-Button-1>", self._on_canvas_double_click) # doble → on/off
        ttk.Label(left, text="Anillo de 32 sensores — +Y = frente · CLICK = inspeccionar · DOBLE CLICK = habilitar/deshabilitar",
                  foreground="#888", wraplength=RING_PX, justify="left").pack(pady=(4, 0))

        # Derecha: paneles de texto.
        right = ttk.Frame(main)
        right.grid(row=0, column=1, sticky="n", padx=(8, 0))

        ttk.Label(right, text="LÍNEA → CENTRAL (LineStatusV2)").pack(anchor="w")
        self.line_txt = tk.Text(right, width=38, height=12, font=("Consolas", 10),
                                bg="#0c0f12", fg="#d8e0e8", relief="flat")
        self.line_txt.pack(pady=(0, 8))

        ttk.Label(right, text="ODOMETRÍA (OTOS)").pack(anchor="w")
        self.otos_txt = tk.Text(right, width=38, height=9, font=("Consolas", 10),
                                bg="#0c0f12", fg="#d8e0e8", relief="flat")
        self.otos_txt.pack(pady=(0, 8))

        ttk.Label(right, text="SALUD DE SENSORES / CALIBRACIÓN").pack(anchor="w")
        self.health_txt = tk.Text(right, width=38, height=7, font=("Consolas", 10),
                                  bg="#0c0f12", fg="#f0c0c0", relief="flat")
        self.health_txt.pack()

        # Columna extra a la derecha: calibración guiada + semáforo de márgenes.
        calib_col = ttk.Frame(main)
        calib_col.grid(row=0, column=2, sticky="n", padx=(8, 0))
        self._build_calib_panel(calib_col)

        self._compute_ring_transform()
        self._draw_static_ring()

    def _build_calib_panel(self, parent: ttk.Frame) -> None:
        """Panel "CALIBRAR": 3 pasos guiados + veredicto + sensibilidad global +
        barras de cercanía al umbral por sensor + inspector del sensor clickeado."""
        ttk.Label(parent, text="CALIBRAR (en orden)",
                  font=("Segoe UI", 11, "bold")).pack(anchor="w")
        for label, cmd in [
                ("1· Capturar VERDE (sobre el verde)", "CAL CARPET"),
                ("2· Capturar BLANCO (sobre la línea)", "CAL WHITE"),
                ("3· GUARDAR (calib + sensibilidad)", "CAL SAVE")]:
            ttk.Button(parent, text=label,
                       command=lambda c=cmd: self._send(c)).pack(fill="x", pady=2)

        self.verdict_lbl = tk.Label(parent, text="esperando datos…",
                                    font=("Segoe UI", 11, "bold"),
                                    bg="#222", fg="#ccc", wraplength=_BARS_W,
                                    justify="left", padx=6, pady=6)
        self.verdict_lbl.pack(fill="x", pady=(10, 2))
        self.valid_lbl = ttk.Label(parent, text="data_valid=?")
        self.valid_lbl.pack(anchor="w")

        # ── Sensibilidad GLOBAL al blanco ──
        ttk.Label(parent, text="Sensibilidad global al blanco "
                  "(+ = MENOS sensible / más estricto · 0 = normal)",
                  foreground="#888", wraplength=_BARS_W, justify="left").pack(anchor="w", pady=(10, 0))
        gframe = ttk.Frame(parent)
        gframe.pack(fill="x")
        self.global_sens_var = tk.IntVar(value=0)
        self.global_sens_lbl = ttk.Label(gframe, text="+0%", width=6)
        self.global_sens_scale = ttk.Scale(
            gframe, from_=-100, to=100, orient="horizontal",
            variable=self.global_sens_var, command=self._on_global_sens_change)
        self.global_sens_scale.pack(side="left", fill="x", expand=True)
        self.global_sens_lbl.pack(side="left")

        # ── Barras de cercanía al umbral por sensor ──
        ttk.Label(parent, text="Cercanía al umbral por sensor (barra a la DERECHA = ve "
                  "blanco · CLICK = inspeccionar · DOBLE = habilitar/deshabilitar)",
                  foreground="#888", wraplength=_BARS_W, justify="left").pack(anchor="w", pady=(8, 2))
        self._bars_canvas = tk.Canvas(
            parent, width=_BARS_COLS * _BARS_CW, height=_BARS_ROWS * _BARS_CH,
            bg="#101418", highlightthickness=0)
        self._bars_canvas.pack()
        self._bars_canvas.bind("<Button-1>", self._on_bars_click)
        self._bars_canvas.bind("<Double-Button-1>", self._on_bars_double_click)

        # ── Inspector del sensor seleccionado ──
        self._build_inspector_panel(parent)

        # Botones avanzados (secundarios).
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

    def _build_inspector_panel(self, parent: ttk.Frame) -> None:
        box = ttk.LabelFrame(parent, text="Inspector de sensor")
        box.pack(fill="x", pady=(10, 2))
        self.inspector_lbl = tk.Label(
            box, text="(click un sensor en el anillo o en las barras)",
            font=("Consolas", 9), bg="#0c0f12", fg="#d8e0e8",
            justify="left", anchor="w", padx=6, pady=4)
        self.inspector_lbl.pack(fill="x")
        row = ttk.Frame(box)
        row.pack(fill="x", pady=2)
        self.toggle_btn = ttk.Button(row, text="Habilitar/Deshabilitar",
                                     command=self._toggle_selected, state="disabled")
        self.toggle_btn.pack(side="left")
        sframe = ttk.Frame(box)
        sframe.pack(fill="x", pady=2)
        ttk.Label(sframe, text="sens propia:").pack(side="left")
        self.sensor_sens_var = tk.IntVar(value=0)
        self.sensor_sens_lbl = ttk.Label(sframe, text="+0%", width=6)
        self.sensor_sens_scale = ttk.Scale(
            sframe, from_=-100, to=100, orient="horizontal",
            variable=self.sensor_sens_var, command=self._on_persensor_sens_change,
            state="disabled")
        self.sensor_sens_scale.pack(side="left", fill="x", expand=True)
        self.sensor_sens_lbl.pack(side="left")

    # ── Comandos a la placa (vía el shell) ────────────────────────────────────
    def _send(self, cmd: str) -> None:
        self.ctx.send(cmd)
        if cmd == "CAL AUTO ON":
            self.calib.start()
        elif cmd == "CAL AUTO OFF":
            self.calib.stop()
        self.ctx.log("cmd", f"→ {cmd}")

    # ── Sintonía fina: handlers ──────────────────────────────────────────────
    # Se envía AL MOVER el slider (no al soltar): se manda cuando el valor ENTERO
    # cambia (dedup), así se ve en vivo sin inundar el serial con cada sub-pixel.
    @staticmethod
    def _scale_int(value) -> int:
        # El command de ttk.Scale pasa el valor como string (ej. "-18.0").
        try:
            return int(round(float(value)))
        except (TypeError, ValueError):
            return 0

    def _on_global_sens_change(self, value=None) -> None:
        v = self._scale_int(value)
        self.global_sens_lbl.config(text=f"{v:+d}%")
        if v != self._last_global_sent:
            self._last_global_sent = v
            self._send(f"SENS GLOBAL {v}")

    def _on_persensor_sens_change(self, value=None) -> None:
        v = self._scale_int(value)
        self.sensor_sens_lbl.config(text=f"{v:+d}%")
        if self._selected is None:
            return
        if v != self._last_persensor_sent:
            self._last_persensor_sent = v
            self._send(f"SENS SET {self._selected} {v}")

    def _ring_sensor_at(self, px, py):
        """Índice del sensor más cercano al click en el anillo, o None."""
        best, bestd = None, (SENSOR_R + 4) ** 2
        for i in range(self.n):
            sx, sy = self._to_px(*geometry.SENSOR_POS[i])
            d = (px - sx) ** 2 + (py - sy) ** 2
            if d <= bestd:
                best, bestd = i, d
        return best

    def _bars_sensor_at(self, px, py):
        """Índice del sensor de la celda de barras clickeada, o None."""
        col = int(px // _BARS_CW)
        row = int(py // _BARS_CH)
        if 0 <= col < _BARS_COLS and 0 <= row < _BARS_ROWS:
            i = row * _BARS_COLS + col
            if i < self.n:
                return i
        return None

    def _on_canvas_click(self, evt) -> None:
        i = self._ring_sensor_at(evt.x, evt.y)
        if i is not None:
            self._select_sensor(i)

    def _on_bars_click(self, evt) -> None:
        i = self._bars_sensor_at(evt.x, evt.y)
        if i is not None:
            self._select_sensor(i)

    def _on_canvas_double_click(self, evt) -> None:
        i = self._ring_sensor_at(evt.x, evt.y)
        if i is not None:
            self._select_sensor(i)
            self._toggle_selected()   # doble-click = habilitar/deshabilitar directo

    def _on_bars_double_click(self, evt) -> None:
        i = self._bars_sensor_at(evt.x, evt.y)
        if i is not None:
            self._select_sensor(i)
            self._toggle_selected()

    def _select_sensor(self, i: int) -> None:
        self._selected = i
        self.toggle_btn.config(state="normal")
        self.sensor_sens_scale.config(state="normal")
        if self.last is not None and i < len(self.last.ring.persensor_sens):
            ps = int(self.last.ring.persensor_sens[i])
            self._last_persensor_sent = ps   # seleccionar NO debe re-enviar el valor actual
            self.sensor_sens_var.set(ps)     # dispara command → v==last → no envía
            self.sensor_sens_lbl.config(text=f"{ps:+d}%")
        if self.last is not None:
            self._render_inspector(self.last)
            self._render_calib_panel(self.last)   # refresca el resaltado

    def _toggle_selected(self) -> None:
        if self._selected is None or self.last is None:
            return
        i = self._selected
        cur = self.last.ring.enabled[i] if i < len(self.last.ring.enabled) else True
        self._send(f"SENSOR {i} {'OFF' if cur else 'ON'}")

    def _render_inspector(self, f: Frame) -> None:
        i = self._selected
        r = f.ring
        # Defensivo igual que _render/_render_calib_panel: un frame con n > largo
        # real de algún array (firmware fuera de contrato / frame armado a mano) no
        # debe tirar IndexError dentro del callback de Tk.
        if i is None or i >= r.n or i >= min(
                len(r.raw), len(r.carpet), len(r.white_cal), len(r.threshold),
                len(r.margins), len(r.white), len(r.enabled), len(r.persensor_sens)):
            return
        raw = f.ring.raw[i]
        th = f.ring.threshold[i]
        en = f.ring.enabled[i]
        sees = raw >= th   # ve blanco = raw >= umbral efectivo (refleja sensibilidad)
        ps = f.ring.persensor_sens[i]
        txt = (f"SENSOR {i:02d}    "
               f"{'HABILITADO' if en else '*** DESHABILITADO ***'}\n"
               f"raw={raw}   carpet={f.ring.carpet[i]}   blanco={f.ring.white_cal[i]}\n"
               f"umbral efectivo={th}   margen={f.ring.margins[i]}\n"
               f"distancia al umbral={raw - th:+d}  "
               f"({'VE BLANCO' if sees else 've piso'})\n"
               f"sensibilidad propia={ps:+d}%")
        self.inspector_lbl.config(text=txt, fg="#9fe0a0" if en else "#e08a8a")
        self.toggle_btn.config(text=("Deshabilitar" if en else "Habilitar")
                               + f" S{i:02d}")

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

    # ── Render ─────────────────────────────────────────────────────────────
    def render(self, f: Frame) -> None:
        self._consume(f)
        statuses = self.health.status()
        self.canvas.delete("sensorx")   # X de deshabilitados del frame anterior
        for i in range(self.n):
            raw = f.ring.raw[i] if i < len(f.ring.raw) else 0
            # "Ve blanco" = raw >= umbral EFECTIVO (con sensibilidad), no el white_bits
            # del line_ring (que usa el punto medio y NO refleja las perillas). Así el
            # slider de sensibilidad realmente barre el anillo blanco↔no-blanco.
            th = f.ring.threshold[i] if i < len(f.ring.threshold) else 0
            en = f.ring.enabled[i] if i < len(f.ring.enabled) else True
            st = statuses[i]
            if not en:
                # DESHABILITADO: gris apagado + X. NUNCA amarillo, aunque raw sea alto,
                # para no confundir al mover el robot (no participa de la detección).
                self.canvas.itemconfig(self._dots[i], fill="#333333",
                                       outline="#777777", width=2)
                px, py = self._to_px(*geometry.SENSOR_POS[i])
                self.canvas.create_line(px - SENSOR_R, py - SENSOR_R,
                                        px + SENSOR_R, py + SENSOR_R,
                                        fill="#ff5050", width=2, tags="sensorx")
                self.canvas.create_line(px + SENSOR_R, py - SENSOR_R,
                                        px - SENSOR_R, py + SENSOR_R,
                                        fill="#ff5050", width=2, tags="sensorx")
                continue
            white = raw >= th
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

    def _consume(self, f: Frame) -> None:
        self.frame_count += 1
        if self.last_seq >= 0:
            gap = f.seq - self.last_seq - 1
            if gap > 0:
                self.dropped += gap
                self.ctx.log("warn", f"se perdieron {gap} frame(s) (seq {self.last_seq}→{f.seq})")
        self.last_seq = f.seq
        self.health.update(f.ring.raw)
        self.calib.update(f.ring.raw)
        self.last = f

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
        """Barras de cercanía al umbral por sensor + veredicto de calibración.

        Cada celda: borde por margen (verde/naranja/rojo), barra horizontal desde
        el centro (=umbral) hacia la derecha si raw≥umbral (ve blanco, amarillo) o
        hacia la izquierda si ve piso (azul); largo ∝ distancia al umbral. Tachado
        rojo si el sensor está deshabilitado. Resalta el sensor seleccionado."""
        margins = f.ring.margins
        thresholds = f.ring.threshold
        c = self._bars_canvas
        c.delete("all")
        weak = []
        for i in range(self.n):
            m = margins[i] if i < len(margins) else 0
            if m >= DEFAULT_MIN_MARGIN:
                edge = "#1f7a33"
            elif m >= WEAK_MARGIN_ORANGE:
                edge = "#b36b00"
            else:
                edge = "#a02020"
            if m < DEFAULT_MIN_MARGIN:
                weak.append(i)

            col = i % _BARS_COLS
            row = i // _BARS_COLS
            x0, y0 = col * _BARS_CW, row * _BARS_CH
            cx = x0 + _BARS_CW / 2
            cy = y0 + _BARS_CH - 11
            sel = (i == self._selected)
            c.create_rectangle(x0 + 1, y0 + 1, x0 + _BARS_CW - 1, y0 + _BARS_CH - 1,
                               fill=("#27333d" if sel else "#161b20"),
                               outline=edge, width=(3 if sel else 2))
            # Línea de umbral (centro) + barra de distancia.
            c.create_line(cx, y0 + 13, cx, y0 + _BARS_CH - 4, fill="#667", dash=(2, 2))
            raw = f.ring.raw[i] if i < len(f.ring.raw) else 0
            th = thresholds[i] if i < len(thresholds) else 0
            half = max(1, m // 2)
            frac = max(-1.0, min(1.0, (raw - th) / half))
            barw = frac * (_BARS_CW / 2 - 5)
            sees = raw >= th   # ve blanco = raw >= umbral EFECTIVO (refleja la sensibilidad)
            if abs(barw) >= 1:
                c.create_rectangle(cx, cy - 3, cx + barw, cy + 3,
                                   fill=("#ffe27a" if sees else "#5a8ac0"),
                                   outline="")
            en = f.ring.enabled[i] if i < len(f.ring.enabled) else True
            if not en:  # X roja grande = deshabilitado (obvio de un vistazo)
                c.create_line(x0 + 4, y0 + 4, x0 + _BARS_CW - 4, y0 + _BARS_CH - 4,
                              fill="#ff5050", width=3)
                c.create_line(x0 + _BARS_CW - 4, y0 + 4, x0 + 4, y0 + _BARS_CH - 4,
                              fill="#ff5050", width=3)
            # Número de sensor: sombra negra + texto claro encima → legible sobre
            # cualquier color de barra/celda (mejor contraste).
            c.create_text(x0 + 5, y0 + 9, text=f"S{i:02d}", anchor="w",
                          fill="#000000", font=("Consolas", 8, "bold"))
            c.create_text(x0 + 4, y0 + 8, text=f"S{i:02d}", anchor="w",
                          fill=("#ffffff" if en else "#ff6a6a"),
                          font=("Consolas", 8, "bold"))

        if not weak:
            bg, fg = _SEM_GREEN
            text = f"CALIB OK — {self.n}/{self.n} sensores separan verde/blanco"
        elif len(weak) <= 4:
            bg, fg = _SEM_ORANGE
            text = "DÉBILES: " + ", ".join(f"S{i:02d}" for i in weak) + " — juega con los demás"
        else:
            bg, fg = _SEM_RED
            text = "CALIB INSUFICIENTE — recalibrar (¿batería baja? ¿hoja no cubre todo?)"
        self.verdict_lbl.config(text=text, bg=bg, fg=fg)
        self.valid_lbl.config(text=f"data_valid={'SÍ' if f.line.valid else 'NO'}")

        # Sincronizar el slider global con la placa UNA vez (no pisar al usuario).
        if not self._global_synced:
            self._global_synced = True
            gs = int(getattr(f.ring, "global_sens", 0))
            self._last_global_sent = gs   # sincronizar con la placa NO debe re-enviar
            self.global_sens_var.set(gs)
            self.global_sens_lbl.config(text=f"{gs:+d}%")

        # Refrescar inspector del sensor seleccionado con el frame nuevo.
        if self._selected is not None:
            self._render_inspector(f)

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
