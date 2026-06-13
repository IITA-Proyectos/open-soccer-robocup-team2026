# Vigía de banco — espera que el puerto COM REAPAREZCA (USB flojo del banco) y
# después lee el panel serial de la CENTRAL buscando snap_fresh=Y + el estado de
# la FSM. Solo LEE; no manda nada al robot. Reporta el estado aunque la TOP no
# levante (= sin batería). Nació el 2026-06-12, cuando el conector USB de la
# CENTRAL cortó >5 veces y bloqueó la verificación del marcador post-flasheo.
#
# Uso:  python tools/banco-usb-watch/watch_panel.py
#   exit 0 = snapshot fresco (imprime el panel: state=..., match=..., hdg=...)
#   exit 1 = el puerto nunca reaparecio (USB sigue muerto -> revisar conector)
#   exit 2 = puerto OK pero sin snapshot (TOP no levanto / sin bateria)
#
# Tras un -t upload, sirve para confirmar el marcador del binario (p.ej.
# state=GK_SIMPLE_WAIT de la v6 del arquero strafe) SIN creerle al SUCCESS de pio.
import serial
import serial.tools.list_ports
import sys
import time

DEADLINE_PORT_S = 540   # esperar el puerto hasta 9 min
DEADLINE_SNAP_S = 120   # con puerto, esperar snapshot hasta 2 min
MARKER = "snap_fresh=Y"  # cambiar si se quiere cazar otro marcador del panel

t0 = time.time()
port = None
while time.time() - t0 < DEADLINE_PORT_S:
    ports = [p.device for p in serial.tools.list_ports.comports()]
    if ports:
        port = ports[0]
        break
    time.sleep(2)
if port is None:
    print("TIMEOUT: el puerto COM nunca reaparecio (USB sigue muerto).")
    sys.exit(1)
print(f"PUERTO {port} reaparecio a los {int(time.time() - t0)} s. Leyendo panel...")

buf = b""
last_lines = []
t1 = time.time()
p = None
try:
    while time.time() - t1 < DEADLINE_SNAP_S:
        if p is None:
            try:
                p = serial.Serial(port, 115200, timeout=0.5)
            except serial.SerialException:
                time.sleep(2)
                continue
        try:
            chunk = p.read(512)
        except serial.SerialException:
            p.close()
            p = None
            continue
        buf += chunk
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            text = line.decode("utf-8", errors="replace").strip()
            if text:
                last_lines = (last_lines + [text])[-4:]
            if MARKER in text:
                print("SNAPSHOT FRESCO — panel:")
                print("\n".join(last_lines))
                sys.exit(0)
    print(f"Puerto OK pero TIMEOUT sin {MARKER} (TOP no levanto?). Ultimas lineas:")
    print("\n".join(last_lines) if last_lines else "(nada legible)")
    sys.exit(2)
finally:
    if p is not None:
        p.close()
