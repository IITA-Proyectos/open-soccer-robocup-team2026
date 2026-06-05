#!/usr/bin/env python3
"""visualize_down_sensors.py — GUI tiempo real de los 32 sensores de luz de la placa DOWN.

Visualizador grafico (Tkinter, sin dependencias externas mas alla de pyserial)
que muestra los 32 sensores ALS-PT19 del anillo en su POSICION FISICA real
(la misma SENSOR_POS[32] del firmware, sincronizada con el doc canonico
hardware/electronics/down-board-pack/01-pinout-y-posiciones.md §5b).

Sirve para:
  1. VALIDAR lecturas: pasar el robot sobre cancha, ver que sensores responden
     y como. Detectar sensores muertos o con ruido.
  2. CALIBRAR: el modo "Normalizado por sensor" auto-aprende min/max por
     sensor mientras vos pasas el robot. Despues podes usar esos rangos para
     definir umbrales reales en config_down.h.
  3. DIAGNOSTICAR: ver en vivo como se mueve el centroide al cruzar lineas
     blancas. Confirmar que la geometria del PCB esta bien interpretada.

Pre-requisitos:
  - Placa DOWN encendida con bateria.
  - USB conectado a la PC.
  - Firmware `diag_down` flasheado (env diag_down en platformio.ini).
  - Serial Monitor de VSCode / Arduino IDE CERRADO (el puerto es exclusivo).
  - python con pyserial (`pip install pyserial`; tkinter viene con python).

Uso:
  python visualize_down_sensors.py
  python visualize_down_sensors.py --port COM10 --baud 115200

Opcional (mejor experiencia): modificar el firmware diag_down para imprimir
cada 100 ms en lugar de 300 ms — ver src/diag/main_diag_down.cpp (el firmware
VIVO) linea ~121 (`if (g_since_print >= 300)`). Cambiarlo
a 100 da 10 Hz de refresh visual en vez de 3.3 Hz. Cambio trivial pero
requiere reflashear con el boton de la Teensy.
"""

import argparse
import io
import json
import math
import re
import sys
import threading
import time
import tkinter as tk
from tkinter import ttk, messagebox

# Forzar UTF-8 en stdout (PowerShell Windows usa cp1252 por default).
try:
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
except Exception:
    pass

# ----------------------------------------------------------------------------
# CONSTANTES Y DATOS
# ----------------------------------------------------------------------------

# SENSOR_POS[i] = (x_mm, y_mm) — posicion fisica del sensor logico i (0..31).
# Origen = centro del PCB; +X = derecha del robot; +Y = adelante.
# Indice 0-based: SENSOR_POS[0] = S1 del PCB. El diag_down imprime S0..S31
# (0-based) que mapea 1:1 con estos indices.
# Fuente: hardware/electronics/down-board-pack/01-pinout-y-posiciones.md §5b.
# Validado parcialmente 2026-05-24 (TASK-027 eje X positivo, eje Y pendiente).
SENSOR_POS = [
    (-36.28, +82.04), (-30.57, +75.06), (-20.28, +74.17), (-10.12, +74.17),  # S1-S4
    (+10.20, +74.17), (+20.36, +74.17), (+30.52, +75.18), (+36.49, +82.04),  # S5-S8
    (-86.32, +12.19), (-86.32,  -0.51), (-84.92, -13.21), (-81.24, -26.04),  # S9-S12
    (-67.65, -50.04), (-60.03, -60.20), (-48.98, -69.09), (-36.28, -76.71),  # S13-S16
    (+36.36, -76.71), (+49.31, -69.09), (+60.87, -60.20), (+69.63, -50.04),  # S17-S20
    (+82.21, -25.91), (+85.64, -13.21), (+87.29,  -0.51), (+86.40, +12.06),  # S21-S24
    (-22.82, +50.93), (-12.66, +50.93), (+12.74, +51.05), (+22.90, +50.93),  # S25-S28
    (+34.55,  -9.51), (+34.55, -19.67), (-33.25,  -9.51), (-33.25, -19.80),  # S29-S32
]
NUM_SENSORS = 32

# Regex para parsear lineas "LUZ S0:670  S1:680*  ... S31:769"
LUZ_RE = re.compile(r'S(\d+):(\d+)([\*\s])')

# Umbrales default (referencia firmware config_down.h):
#   Carpet verde ~100-300, Linea blanca ~600-900.
DEFAULT_THRESHOLD_LOW  = 300   # bajo: ve carpet
DEFAULT_THRESHOLD_HIGH = 600   # alto: ve linea blanca

# Layout canvas
CANVAS_W = 700
CANVAS_H = 700
# Escala mm -> px. SENSOR_POS llega a ~±90mm; ocupar ~80% del canvas:
#   max_mm * scale = (CANVAS_W/2) * 0.8 = 280 → scale ≈ 280/90 ≈ 3.1 px/mm
SCALE_PX_PER_MM = 3.0
SENSOR_RADIUS_PX = 22


# ----------------------------------------------------------------------------
# COLOR HELPERS
# ----------------------------------------------------------------------------

def lerp(a, b, t):
    return a + (b - a) * t


def rgb_to_hex(r, g, b):
    return f"#{int(r):02x}{int(g):02x}{int(b):02x}"


def color_for_value(val, low, high):
    """Devuelve color hex segun el valor y los umbrales:
       val <= low      -> verde oscuro (carpet)
       low < val < high -> amarillo (transicion)
       val >= high     -> rojo brillante (linea blanca)
    Interpola suavemente entre los 3 puntos."""
    if val <= low:
        # Verde oscuro: ratio 0 -> verde mas oscuro al disminuir
        t = max(0.0, min(1.0, val / max(low, 1)))
        r = lerp(20, 60, t)
        g = lerp(80, 180, t)
        b = lerp(20, 60, t)
        return rgb_to_hex(r, g, b)
    elif val >= high:
        t = max(0.0, min(1.0, (val - high) / max(1023 - high, 1)))
        r = lerp(220, 255, t)
        g = lerp(60, 30, t)
        b = lerp(30, 30, t)
        return rgb_to_hex(r, g, b)
    else:
        # Transicion verde -> amarillo -> rojo
        t = (val - low) / max(high - low, 1)
        # 0=verde, 0.5=amarillo, 1=rojo
        if t < 0.5:
            tt = t * 2
            r = lerp(60, 230, tt)
            g = lerp(180, 200, tt)
            b = lerp(60, 60, tt)
        else:
            tt = (t - 0.5) * 2
            r = lerp(230, 220, tt)
            g = lerp(200, 80, tt)
            b = lerp(60, 40, tt)
        return rgb_to_hex(r, g, b)


# ----------------------------------------------------------------------------
# SERIAL READER (thread)
# ----------------------------------------------------------------------------

class SerialReader(threading.Thread):
    """Lee del puerto serial en background y actualiza shared_state."""
    def __init__(self, port, baud, shared_state):
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.state = shared_state
        self._stop = threading.Event()
        self.ser = None

    def stop(self):
        self._stop.set()

    def run(self):
        try:
            import serial
        except ImportError:
            self.state.set_error(
                "pyserial no instalado. Corré: pip install pyserial")
            return

        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.3)
        except Exception as e:
            self.state.set_error(f"No pude abrir {self.port}: {e}")
            return

        self.state.set_connected(True)

        while not self._stop.is_set():
            try:
                line = self.ser.readline().decode('utf-8', errors='replace').strip()
            except Exception:
                continue
            if not line.startswith('LUZ '):
                continue
            # Parsear y volcar al estado compartido
            updates = {}
            whites = {}
            for m in LUZ_RE.finditer(line):
                idx = int(m.group(1))
                val = int(m.group(2))
                white = (m.group(3) == '*')
                if 0 <= idx < NUM_SENSORS:
                    updates[idx] = val
                    whites[idx] = white
            if updates:
                self.state.update_values(updates, whites)

        try:
            if self.ser:
                self.ser.close()
        except Exception:
            pass
        self.state.set_connected(False)


# ----------------------------------------------------------------------------
# SHARED STATE (thread-safe minimo)
# ----------------------------------------------------------------------------

class SharedState:
    """Estado compartido entre el thread serial y la GUI. Lock simple."""
    def __init__(self):
        self.lock = threading.Lock()
        self.values  = [0] * NUM_SENSORS    # ultimo raw por sensor
        self.whites  = [False] * NUM_SENSORS
        self.min_val = [1023] * NUM_SENSORS  # historico
        self.max_val = [0] * NUM_SENSORS
        self.last_update_ts = 0.0
        self.frame_count = 0
        self.connected = False
        self.error = None

    def update_values(self, updates, whites):
        with self.lock:
            for idx, val in updates.items():
                self.values[idx] = val
                if val < self.min_val[idx]: self.min_val[idx] = val
                if val > self.max_val[idx]: self.max_val[idx] = val
            for idx, w in whites.items():
                self.whites[idx] = w
            self.last_update_ts = time.time()
            self.frame_count += 1

    def reset_min_max(self):
        with self.lock:
            self.min_val = [1023] * NUM_SENSORS
            self.max_val = [0] * NUM_SENSORS

    def snapshot(self):
        with self.lock:
            return (list(self.values), list(self.whites),
                    list(self.min_val), list(self.max_val),
                    self.frame_count, self.last_update_ts,
                    self.connected, self.error)

    def set_connected(self, v):
        with self.lock:
            self.connected = v

    def set_error(self, msg):
        with self.lock:
            self.error = msg


# ----------------------------------------------------------------------------
# GUI APP
# ----------------------------------------------------------------------------

class VisualizerApp:
    def __init__(self, root, port, baud):
        self.root = root
        self.port = port
        self.baud = baud
        self.state = SharedState()
        self.reader = None

        # Modo de color: 'fixed' | 'normalized' | 'raw_grayscale'
        self.color_mode = tk.StringVar(value='fixed')
        self.thresh_low  = tk.IntVar(value=DEFAULT_THRESHOLD_LOW)
        self.thresh_high = tk.IntVar(value=DEFAULT_THRESHOLD_HIGH)

        # Para FPS
        self.last_fps_check = time.time()
        self.last_frame_count = 0
        self.fps_display = 0.0

        self.build_ui()
        self.start_serial()
        self.schedule_refresh()

    def build_ui(self):
        self.root.title(f"DOWN Sensor Visualizer — {self.port}")
        self.root.geometry(f"{CANVAS_W + 280}x{CANVAS_H + 20}")
        self.root.configure(bg='#222222')

        # ===== Frame principal =====
        frame = tk.Frame(self.root, bg='#222222')
        frame.pack(fill=tk.BOTH, expand=True)

        # ===== Canvas (izquierda) =====
        self.canvas = tk.Canvas(frame, width=CANVAS_W, height=CANVAS_H,
                                bg='#1a1a1a', highlightthickness=0)
        self.canvas.pack(side=tk.LEFT, padx=8, pady=8)

        # ===== Panel lateral (derecha) =====
        side = tk.Frame(frame, bg='#222222', width=260)
        side.pack(side=tk.RIGHT, fill=tk.Y, padx=8, pady=8)
        side.pack_propagate(False)

        # Estado conexion
        self.lbl_status = tk.Label(side, text="● Conectando...",
                                   bg='#222222', fg='#ffaa00',
                                   font=('Consolas', 11, 'bold'))
        self.lbl_status.pack(anchor='w', pady=(0, 4))

        self.lbl_fps = tk.Label(side, text="FPS: -", bg='#222222', fg='#aaaaaa',
                                font=('Consolas', 9))
        self.lbl_fps.pack(anchor='w', pady=(0, 8))

        ttk.Separator(side).pack(fill='x', pady=4)

        # Modo color
        tk.Label(side, text="MODO COLOR", bg='#222222', fg='#ffffff',
                 font=('Consolas', 9, 'bold')).pack(anchor='w', pady=(4, 2))
        ttk.Radiobutton(side, text="Umbral fijo (carpet/linea)",
                        variable=self.color_mode, value='fixed').pack(anchor='w')
        ttk.Radiobutton(side, text="Normalizado por sensor",
                        variable=self.color_mode, value='normalized').pack(anchor='w')
        ttk.Radiobutton(side, text="Grayscale raw",
                        variable=self.color_mode, value='raw_grayscale').pack(anchor='w')

        ttk.Separator(side).pack(fill='x', pady=8)

        # Umbrales
        tk.Label(side, text="UMBRALES (modo fijo)", bg='#222222', fg='#ffffff',
                 font=('Consolas', 9, 'bold')).pack(anchor='w', pady=(4, 2))
        f1 = tk.Frame(side, bg='#222222'); f1.pack(fill='x', pady=2)
        tk.Label(f1, text="Carpet ≤", bg='#222222', fg='#aaaaaa',
                 font=('Consolas', 9), width=10, anchor='w').pack(side=tk.LEFT)
        tk.Spinbox(f1, from_=0, to=1023, increment=10, width=6,
                   textvariable=self.thresh_low).pack(side=tk.LEFT)

        f2 = tk.Frame(side, bg='#222222'); f2.pack(fill='x', pady=2)
        tk.Label(f2, text="Linea ≥", bg='#222222', fg='#aaaaaa',
                 font=('Consolas', 9), width=10, anchor='w').pack(side=tk.LEFT)
        tk.Spinbox(f2, from_=0, to=1023, increment=10, width=6,
                   textvariable=self.thresh_high).pack(side=tk.LEFT)

        ttk.Separator(side).pack(fill='x', pady=8)

        # Stats globales
        tk.Label(side, text="STATS GLOBALES", bg='#222222', fg='#ffffff',
                 font=('Consolas', 9, 'bold')).pack(anchor='w', pady=(4, 2))
        self.lbl_stats = tk.Label(side, text="—", bg='#222222', fg='#aaffaa',
                                  font=('Consolas', 9), justify='left')
        self.lbl_stats.pack(anchor='w')

        ttk.Separator(side).pack(fill='x', pady=8)

        # Botones
        tk.Button(side, text="Reset min/max", command=self.reset_range,
                  bg='#444444', fg='#ffffff').pack(fill='x', pady=2)
        tk.Button(side, text="Snapshot a JSON", command=self.save_snapshot,
                  bg='#444444', fg='#ffffff').pack(fill='x', pady=2)
        tk.Button(side, text="Salir", command=self.on_close,
                  bg='#663333', fg='#ffffff').pack(fill='x', pady=2)

        # Leyenda
        ttk.Separator(side).pack(fill='x', pady=8)
        tk.Label(side, text="REFERENCIA",
                 bg='#222222', fg='#ffffff',
                 font=('Consolas', 9, 'bold')).pack(anchor='w', pady=(4, 2))
        tk.Label(side, text="● Verde: carpet (oscuro)",
                 bg='#222222', fg='#80c080',
                 font=('Consolas', 9)).pack(anchor='w')
        tk.Label(side, text="● Amarillo: transicion",
                 bg='#222222', fg='#ddcc66',
                 font=('Consolas', 9)).pack(anchor='w')
        tk.Label(side, text="● Rojo: linea blanca",
                 bg='#222222', fg='#ff6644',
                 font=('Consolas', 9)).pack(anchor='w')

        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def start_serial(self):
        self.reader = SerialReader(self.port, self.baud, self.state)
        self.reader.start()

    def reset_range(self):
        self.state.reset_min_max()

    def save_snapshot(self):
        vals, whites, mins, maxs, fc, ts, _, _ = self.state.snapshot()
        path = f"snapshot_down_sensors_{int(time.time())}.json"
        data = {
            "timestamp": ts,
            "frame_count": fc,
            "port": self.port,
            "sensors": [
                {
                    "idx": i, "pcb_label": f"S{i+1}",
                    "x_mm": SENSOR_POS[i][0], "y_mm": SENSOR_POS[i][1],
                    "raw": vals[i], "sees_white": whites[i],
                    "min_hist": mins[i], "max_hist": maxs[i],
                }
                for i in range(NUM_SENSORS)
            ],
        }
        try:
            with open(path, 'w') as f:
                json.dump(data, f, indent=2)
            messagebox.showinfo("Snapshot guardado", f"Guardado en:\n{path}")
        except Exception as e:
            messagebox.showerror("Error", f"No pude guardar: {e}")

    def on_close(self):
        if self.reader:
            self.reader.stop()
        try:
            self.root.destroy()
        except Exception:
            pass

    def schedule_refresh(self):
        self.refresh()
        self.root.after(100, self.schedule_refresh)  # ~10 Hz GUI refresh

    def refresh(self):
        vals, whites, mins, maxs, fc, ts, conn, err = self.state.snapshot()

        # === Status ===
        if err:
            self.lbl_status.config(text=f"● ERROR: {err[:35]}", fg='#ff5555')
        elif conn:
            self.lbl_status.config(text=f"● Conectado {self.port}", fg='#55ff55')
        else:
            self.lbl_status.config(text=f"● Desconectado", fg='#888888')

        # FPS (cuanto refresca el firmware)
        now = time.time()
        dt = now - self.last_fps_check
        if dt >= 1.0:
            frames = fc - self.last_frame_count
            self.fps_display = frames / dt
            self.last_fps_check = now
            self.last_frame_count = fc
        self.lbl_fps.config(text=f"FPS firmware: {self.fps_display:.1f} Hz")

        # Stats globales
        if fc > 0:
            vmin = min(vals); vmax = max(vals)
            vavg = sum(vals) / NUM_SENSORS
            n_white = sum(1 for w in whites if w)
            n_active = sum(1 for v in vals if v > self.thresh_high.get())
            stats_txt = (f"min:   {vmin:4d}\nmax:   {vmax:4d}\n"
                         f"avg:   {vavg:6.1f}\n"
                         f"see-white firmware: {n_white}\n"
                         f"> umbral GUI:       {n_active}")
            self.lbl_stats.config(text=stats_txt)

        # === Canvas ===
        self.canvas.delete("all")

        cx = CANVAS_W // 2
        cy = CANVAS_H // 2

        # Etiquetas direccionales
        self.canvas.create_text(cx, 18, text="FRENTE  (+Y)",
                                fill='#888888', font=('Consolas', 10, 'bold'))
        self.canvas.create_text(cx, CANVAS_H - 18, text="ATRAS  (-Y)",
                                fill='#666666', font=('Consolas', 10))
        self.canvas.create_text(28, cy, text="IZQ", angle=90,
                                fill='#666666', font=('Consolas', 10))
        self.canvas.create_text(CANVAS_W - 28, cy, text="DER", angle=90,
                                fill='#666666', font=('Consolas', 10))

        # Aros guia (radios 40, 60, 80 mm)
        for r_mm in (40, 60, 80):
            r_px = int(r_mm * SCALE_PX_PER_MM)
            self.canvas.create_oval(cx - r_px, cy - r_px, cx + r_px, cy + r_px,
                                    outline='#333333', dash=(2, 4))
            self.canvas.create_text(cx + r_px + 6, cy - 4,
                                    text=f"{r_mm}mm", anchor='w',
                                    fill='#444444', font=('Consolas', 8))

        # Ejes
        self.canvas.create_line(cx, 40, cx, CANVAS_H - 40,
                                fill='#333333', dash=(2, 2))
        self.canvas.create_line(40, cy, CANVAS_W - 40, cy,
                                fill='#333333', dash=(2, 2))

        # Sensores
        low = self.thresh_low.get()
        high = self.thresh_high.get()
        mode = self.color_mode.get()

        for i in range(NUM_SENSORS):
            x_mm, y_mm = SENSOR_POS[i]
            # +Y robot = arriba del canvas (invertido respecto a pixel coords)
            x_px = cx + x_mm * SCALE_PX_PER_MM
            y_px = cy - y_mm * SCALE_PX_PER_MM
            val = vals[i]

            # Determinar color segun modo
            if mode == 'fixed':
                color = color_for_value(val, low, high)
            elif mode == 'normalized':
                vmin = mins[i]; vmax = maxs[i]
                if vmax - vmin < 5:
                    color = '#444444'  # sensor sin variacion
                else:
                    norm = (val - vmin) / (vmax - vmin)
                    color = color_for_value(int(norm * 1023), 300, 700)
            else:  # raw_grayscale
                t = max(0, min(255, int(val * 255 / 1023)))
                color = rgb_to_hex(t, t, t)

            # Circulo sensor
            r = SENSOR_RADIUS_PX
            self.canvas.create_oval(x_px - r, y_px - r, x_px + r, y_px + r,
                                    fill=color, outline='#666666', width=1)

            # Marca '*' del firmware (ve blanco)
            if whites[i]:
                self.canvas.create_oval(x_px - r - 3, y_px - r - 3,
                                        x_px + r + 3, y_px + r + 3,
                                        outline='#ffffff', width=2)

            # Valor numerico (raw)
            txt_color = '#000000' if mode == 'raw_grayscale' and val > 500 else '#000000'
            # Texto blanco para fondos oscuros
            if mode == 'raw_grayscale' and val < 128:
                txt_color = '#ffffff'
            self.canvas.create_text(x_px, y_px, text=str(val),
                                    fill=txt_color, font=('Consolas', 10, 'bold'))

            # Etiqueta sensor (S1..S32) debajo
            self.canvas.create_text(x_px, y_px + r + 10,
                                    text=f"S{i+1}", fill='#888888',
                                    font=('Consolas', 8))


# ----------------------------------------------------------------------------
# MAIN
# ----------------------------------------------------------------------------

def main():
    p = argparse.ArgumentParser(description="GUI visualizador 32 sensores DOWN")
    p.add_argument('--port', default='COM10', help="Puerto serial (default COM10)")
    p.add_argument('--baud', type=int, default=115200, help="Baud rate (default 115200)")
    args = p.parse_args()

    root = tk.Tk()
    app = VisualizerApp(root, args.port, args.baud)
    try:
        root.mainloop()
    except KeyboardInterrupt:
        app.on_close()


if __name__ == '__main__':
    main()
