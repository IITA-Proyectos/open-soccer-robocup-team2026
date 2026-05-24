"""diag_otos_move_test.py — Test cuantitativo de tracking OTOS.

Captura 15 s de pose OTOS por serial mientras el operador desliza el robot
una distancia conocida (ej: 30 cm sobre cancha/cartón/superficie texturada).
Reporta trayectoria + desplazamiento neto + comparación contra referencia
del 2026-05-24 (con lámina vs sin lámina + cartón).

Uso (con el firmware diag_down corriendo en COM10):
    python diag_otos_move_test.py

Mientras corre, deslizar el robot 30 cm sin levantar ni rotar.

Criterio de éxito (TASK-029): distancia reportada >= 275 mm sobre
movimiento real de 300 mm (error < 8%, < 25 mm).

Creado 2026-05-24 al validar TASK-029. Ver
journal/2026-05-24-down-board-passing-tests-cierre.md sección "Test final".
"""
import serial, time, re, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

print('>>> CAPTURA INICIADA. Espera 2 segs y desliza el robot 30cm despacito <<<', flush=True)

OTOS_RE = re.compile(r'OTOS: x=(-?[0-9.]+) y=(-?[0-9.]+) hdg=(-?[0-9.]+)')

try:
    s = serial.Serial('COM10', 115200, timeout=0.3)
    start = time.time()
    samples = []
    while time.time() - start < 15:
        line = s.readline().decode('utf-8', errors='replace').strip()
        m = OTOS_RE.match(line)
        if m:
            t = time.time() - start
            samples.append((t, float(m.group(1)), float(m.group(2)), float(m.group(3))))
    s.close()

    print(f'\n[{len(samples)} muestras OTOS en 15s]')
    if samples:
        step = max(1, len(samples) // 12)
        print('  t(s)     x(mm)      y(mm)    hdg(deg)   dist(mm)')
        x0, y0 = samples[0][1], samples[0][2]
        for i in range(0, len(samples), step):
            t, x, y, h = samples[i]
            d = ((x-x0)**2 + (y-y0)**2) ** 0.5
            print(f'  {t:5.2f}  {x:+8.1f}   {y:+8.1f}   {h:+7.1f}    {d:7.1f}')
        t, x, y, h = samples[-1]
        d = ((x-x0)**2 + (y-y0)**2) ** 0.5
        print(f'  {t:5.2f}  {x:+8.1f}   {y:+8.1f}   {h:+7.1f}    {d:7.1f}  <- final')

        dx, dy = x - x0, y - y0
        dist = (dx*dx + dy*dy) ** 0.5
        max_dist = max(((row[1]-x0)**2 + (row[2]-y0)**2)**0.5 for row in samples)
        dhdg = h - samples[0][3]

        print()
        print(f'  ==> Desplazamiento neto: dx={dx:+.1f}mm  dy={dy:+.1f}mm  distancia={dist:.1f}mm')
        print(f'  ==> Distancia maxima alcanzada (peak): {max_dist:.1f}mm')
        print(f'  ==> Cambio heading: {dhdg:+.1f}deg (esperado ~0 si no rotaste)')
        print()
        print(f'  Comparacion vs test anterior con lamina puesta:')
        print(f'    Antes (lamina):  distancia neta 28.6mm sobre movimiento real 300mm')
        print(f'    Ahora (sin lamina): distancia neta {dist:.1f}mm')
except Exception as e:
    print(f'[ERROR] {e}')
