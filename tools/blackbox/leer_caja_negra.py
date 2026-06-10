#!/usr/bin/env python3
"""Lector de la CAJA NEGRA de la CENTRAL (envs *_bb).

Se conecta al puerto USB de la CENTRAL, pide el volcado ('d') y guarda el CSV
que llega entre los marcadores BLACKBOX BEGIN/END. También captura volcados
AUTOMÁTICOS (los que la CENTRAL emite sola en cada RUN→STOP) si el script está
corriendo en ese momento.

Uso:
    python leer_caja_negra.py COM15
    python leer_caja_negra.py COM15 --espera   (no manda 'd'; espera el auto-volcado)

Salida: corrida_YYYYmmdd_HHMMSS.csv en el directorio actual.
Requiere: pip install pyserial
"""
import sys
import time
import datetime

try:
    import serial
except ImportError:
    sys.exit("Falta pyserial:  pip install pyserial")


def main() -> None:
    if len(sys.argv) < 2:
        sys.exit("Uso: python leer_caja_negra.py COMx [--espera]")
    puerto = sys.argv[1]
    solo_esperar = "--espera" in sys.argv[2:]

    ser = serial.Serial(puerto, 115200, timeout=1)
    time.sleep(0.3)
    ser.reset_input_buffer()

    if not solo_esperar:
        ser.write(b"d")
        print(f"[{puerto}] pedido de volcado enviado ('d'); esperando datos...")
    else:
        print(f"[{puerto}] esperando un volcado automático (RUN->STOP)...")

    filas: list[str] = []
    capturando = False
    inicio = time.time()
    while True:
        linea = ser.readline().decode("utf-8", errors="replace").strip()
        if not linea:
            # 90 s sin BEGIN = algo no está: ¿env *_bb flasheado? ¿puerto correcto?
            if not capturando and time.time() - inicio > 90:
                sys.exit("Timeout: no llegó 'BLACKBOX BEGIN'. ¿Flasheaste un env *_bb? ¿Puerto correcto?")
            continue
        if "BLACKBOX BEGIN" in linea:
            capturando = True
            filas = []
            print(f"  {linea}")
            continue
        if "BLACKBOX END" in linea:
            break
        if capturando:
            filas.append(linea)

    ser.close()
    nombre = datetime.datetime.now().strftime("corrida_%Y%m%d_%H%M%S.csv")
    with open(nombre, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(filas) + "\n")
    print(f"OK: {len(filas) - 1} muestras -> {nombre}")
    print("Pasale ese archivo a Claude para el análisis.")


if __name__ == "__main__":
    main()
