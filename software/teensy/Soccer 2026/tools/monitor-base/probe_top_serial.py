"""probe_top_serial.py — sonda HEADLESS de la TOP por serie (sin la GUI del monitor).

Para validar/diagnosticar en banco leyendo la telemetría USB del TOP desde la terminal.
Reusa el parser oficial (monitor_base.protocol_top). Se creó en la sesión de banco del
2026-06-22 (validación del bus de ToF a 400 kHz, robot2) — ver
journal/2026-06-22-banco-tof-400khz-validacion-serie-robot2.md.

SOLO LECTURA segura: solo manda 'STREAM ON' + 'PING' (modo MÁQUINA, encienden el stream
JSONL y se auto-apaga a los 3 s) o un ENTER vacío (modo HUMANO). NUNCA manda comandos de
config (IMU ZERO, ZONEMASK, CFG SAVE, etc.).

Uso (desde tools/monitor-base/):
    python probe_top_serial.py tof      [--port COM22] [--secs 10]   # 4 ToF por sensor + zonas + heading
    python probe_top_serial.py heading  [--port COM22] [--secs 20]   # traza de heading (para girar el robot)
    python probe_top_serial.py loop     [--port COM22] [--secs 15]   # pasadas/s del loop (A/B 400 vs 100 kHz)
    python probe_top_serial.py cfg      [--port COM22] [--secs 6]    # dump de config (orientación por ToF, etc.)

Si no se pasa --port, autodetecta el Teensy (VID PJRC 0x16C0).
"""
from __future__ import annotations

import argparse
import re
import sys
import time

import serial
from monitor_base.protocol_top import parse_line_top
from monitor_base.sources import autodetect_port


def _open(port: str | None) -> "serial.Serial":
    dev = port or autodetect_port()
    if not dev:
        sys.exit("no encontré el Teensy (pasá --port COMx)")
    print(f"abriendo {dev} @115200 ...")
    s = serial.Serial(dev, 115200, timeout=0.4)
    s.reset_input_buffer()
    return s


def _stream(s, secs: float):
    """Itera TopFrames del stream MÁQUINA por `secs` segundos (manda STREAM ON + PING)."""
    s.write(b"STREAM ON\n"); s.flush()
    t0 = time.time(); last_ping = time.time()
    try:
        while time.time() - t0 < secs:
            if time.time() - last_ping > 1.0:
                s.write(b"PING\n"); s.flush(); last_ping = time.time()
            raw = s.readline()
            if not raw:
                continue
            line = raw.decode("ascii", "replace").strip()
            if not line.startswith("{"):
                continue
            try:
                yield time.time() - t0, parse_line_top(line)
            except Exception:
                continue
    finally:
        try:
            s.write(b"STREAM OFF\n"); s.flush()
        except Exception:
            pass


def cmd_tof(s, secs):
    n = 0; last_print = 0.0
    for t, fr in _stream(s, secs):
        n += 1
        if t - last_print < 0.5:
            continue
        last_print = t
        tof, im = fr.tof, fr.imu
        d = "  ".join(f"T{i}={'--' if v is None else v}" for i, v in enumerate(tof.distances_mm))
        zc = ""
        if tof.zones:
            zc = " zonas_ok=[" + ",".join(str(sum(1 for z in zs if z is not None)) for zs in tof.zones) + "]/16"
        print(f"[{t:4.1f}s] {d}  min={tof.min_mm}{zc} | hdg={im.heading_deg:.1f} valid={im.heading_valid} "
              f"cent_ok={im.sentinel_ok} cent={im.sentinel_deg:.1f}")
    print(f"--- {n} frames ---")


def cmd_heading(s, secs):
    n = 0; last_print = 0.0; h0 = None; hmin, hmax = 1e9, -1e9
    print("girá el robot ahora; mido la traza de heading...")
    for t, fr in _stream(s, secs):
        n += 1
        h = fr.imu.heading_deg
        if h0 is None:
            h0 = h
        hmin = min(hmin, h); hmax = max(hmax, h)
        if t - last_print < 0.3:
            continue
        last_print = t
        print(f"[{t:4.1f}s] hdg={h:7.1f} (delta_ini={h-h0:+6.1f}) valid={fr.imu.heading_valid} cent={fr.imu.sentinel_deg:.1f}")
    if h0 is not None:
        print(f"--- {n} frames | inicial={h0:.1f} min={hmin:.1f} max={hmax:.1f} rango={hmax-hmin:.1f} ---")


def cmd_loop(s, secs):
    """Pasadas/s del loop leyendo el contador 'loop=' del panel humano (SIN STREAM,
    para no agregar overhead de serialización al loop)."""
    pat = re.compile(r"loop=(\d+)")
    t0 = time.time(); first = last = None; ft = lt = None; k = 0
    while time.time() - t0 < secs:
        raw = s.readline()
        if not raw:
            continue
        m = pat.search(raw.decode("ascii", "replace"))
        if not m:
            continue
        v = int(m.group(1)); now = time.time()
        if first is None:
            first = v; ft = now
        last = v; lt = now; k += 1
    if first is not None and lt and lt > ft and k >= 2:
        print(f"{last-first:,} pasadas en {lt-ft:.2f}s = {(last-first)/(lt-ft):,.0f} pasadas/s ({k} muestras)")
    else:
        print(f"no se capturó el panel loop= ({k} muestras) — ¿build sin -DTOP_USB_MONITOR?")


def cmd_cfg(s, secs):
    """Despierta el modo HUMANO (ENTER) e imprime las líneas de texto (CFG, ToF, IMU...)."""
    t0 = time.time(); last = 0.0; seen = set()
    while time.time() - t0 < secs:
        if time.time() - last > 0.8:
            s.write(b"\n"); s.flush(); last = time.time()
        raw = s.readline()
        if not raw:
            continue
        line = raw.decode("ascii", "replace").rstrip()
        if not line or line.startswith("{"):
            continue
        key = line[:40]
        if key in seen and "loop=" not in line:
            continue
        seen.add(key)
        print(line)


def main():
    ap = argparse.ArgumentParser(description="Sonda headless de la TOP por serie (solo lectura).")
    ap.add_argument("mode", choices=["tof", "heading", "loop", "cfg"])
    ap.add_argument("--port", default=None, help="COMx (autodetecta si se omite)")
    ap.add_argument("--secs", type=float, default=None, help="duración (default por modo)")
    a = ap.parse_args()
    defaults = {"tof": 10.0, "heading": 20.0, "loop": 15.0, "cfg": 6.0}
    secs = a.secs if a.secs is not None else defaults[a.mode]
    s = _open(a.port)
    try:
        {"tof": cmd_tof, "heading": cmd_heading, "loop": cmd_loop, "cfg": cmd_cfg}[a.mode](s, secs)
    finally:
        s.close()


if __name__ == "__main__":
    main()
