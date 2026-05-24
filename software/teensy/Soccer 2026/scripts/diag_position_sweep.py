#!/usr/bin/env python3
"""diag_position_sweep.py — Validación física de las posiciones (x, y) del SENSOR_POS[].

Pasás una hoja blanca sobre fondo negro lentamente a lo largo de un eje (Y o X)
mientras el firmware `diag_down` está corriendo. El script registra el momento
en que cada uno de los 32 sensores transiciona de negro→blanco (cuando la hoja
lo cubre), y compara el orden temporal observado con el orden esperado según
las coordenadas físicas extraídas del PCB.

Si los 2 barridos (Y y X) coinciden con las coordenadas → SENSOR_POS[] está bien.
Si hay inversiones sistemáticas → posible rotación/reflexión del montaje físico.

Uso:
    python diag_position_sweep.py --port COM10 --axis Y      # barrido vertical
    python diag_position_sweep.py --port COM10 --axis X      # barrido horizontal
    python diag_position_sweep.py --verdict                  # análisis completo

⚠️ El Serial Monitor (Arduino IDE / VSCode) debe estar CERRADO — el puerto es
exclusivo. Si está tomado, falla con "Access denied".
"""
import argparse
import io
import json
import os
import re
import sys
import time

# Forzar UTF-8 en stdout/stderr — Windows PowerShell usa cp1252 por default y
# falla con caracteres como -> >= etc. cuando se imprimen via Python.
try:
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')
except Exception:
    pass

CAPTURE_DIR = ".captures"
SWEEP_PREFIX = "sweep_"
LUZ_RE = re.compile(r'S(\d+):(\d+)([\*\s])')

# SENSOR_POS[i] = (x_mm, y_mm) — posición física del sensor lógico i (0..31)
# desde el centro del PCB DOWN. Convención: +X = derecha del robot, +Y = adelante.
# Fuente canónica: hardware/electronics/down-board-pack/01-pinout-y-posiciones.md §5b
# Validado empíricamente 2026-05-24 (firmware lee los 32 sensores con valores
# independientes). Cada índice del firmware (idx en g_raw[idx]) corresponde a
# S(idx+1) en la numeración del PCB; el scrambling del CD4051 ya lo aplica el
# firmware en sample_all_sensors_hardware().
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


def capture_sweep(port, axis, duration):
    """Captura un barrido completo. La dirección física no importa — el verdict
    prueba ambas y elige la mejor."""
    import serial
    os.makedirs(CAPTURE_DIR, exist_ok=True)

    print()
    print("=" * 70)
    print(f"  BARRIDO EJE {axis}")
    print("=" * 70)
    print()
    print("Preparación:")
    print("  1. Pone FONDO NEGRO (cartulina/papel oscuro) debajo del robot.")
    print("  2. Tene a mano una HOJA BLANCA (preferentemente rígida tipo carton)")
    print("     más grande que el robot, con un borde RECTO.")
    if axis == 'Y':
        print()
        print("  3. Sostené la hoja FUERA del robot, ya sea por el FRENTE o por")
        print("     ATRAS del robot (no importa cuál, el script auto-detecta).")
        print("  4. Cuando arranque la captura, deslizá la hoja LENTAMENTE")
        print("     hacia el lado opuesto, CUBRIENDO el robot, en ~6-8 segundos.")
        print("     El borde de la hoja debe ir PARALELO al eje X (perpendicular")
        print("     a la dirección del movimiento).")
    else:  # X
        print()
        print("  3. Sostené la hoja FUERA del robot, ya sea por la IZQUIERDA o")
        print("     por la DERECHA (no importa cuál, el script auto-detecta).")
        print("  4. Cuando arranque la captura, deslizá la hoja LENTAMENTE")
        print("     hacia el lado opuesto, CUBRIENDO el robot, en ~6-8 segundos.")
        print("     El borde de la hoja debe ir PARALELO al eje Y (perpendicular")
        print("     a la dirección del movimiento).")
    print()
    input("Pulsá Enter cuando estés listo para empezar el barrido...")

    try:
        s = serial.Serial(port, 115200, timeout=0.5)
    except Exception as e:
        print(f"\n[ERROR] No pude abrir {port}: {e}")
        print("  Tip: cerrá el Serial Monitor de Arduino IDE / VSCode.")
        sys.exit(2)

    # Limpiar buffer de lecturas viejas antes de empezar.
    s.reset_input_buffer()
    time.sleep(0.1)
    s.reset_input_buffer()

    print(f"\n>>> EMPEZÁ A DESLIZAR LA HOJA AHORA. Capturando {duration:.0f}s... <<<\n")

    start = time.time()
    samples = {}  # samples[sensor_idx] = list of (t_rel_sec, raw_value)
    n_lines = 0
    while time.time() - start < duration:
        try:
            line = s.readline().decode('utf-8', errors='replace').strip()
        except Exception:
            continue
        if not line.startswith('LUZ '):
            continue
        t_rel = time.time() - start
        n_lines += 1
        for m in LUZ_RE.finditer(line):
            idx = int(m.group(1))
            val = int(m.group(2))
            samples.setdefault(idx, []).append((t_rel, val))
    s.close()

    if not samples:
        print("[ERROR] No se leyó ninguna línea LUZ en el período.")
        print("  Posibles causas: diag_down no flasheado, Serial ocupado, placa apagada.")
        sys.exit(1)

    avg_frames = sum(len(v) for v in samples.values()) // max(len(samples), 1)
    print(f"\nCaptura terminada. {n_lines} frames LUZ leídos.")
    print(f"  Sensores detectados: {len(samples)} (esperados: 32)")
    print(f"  Promedio frames por sensor: {avg_frames}")
    if avg_frames < 15:
        print("  ⚠️ Pocos frames — pasaste muy rápido la hoja o el diag tiene refresco lento.")

    path = os.path.join(CAPTURE_DIR, f"{SWEEP_PREFIX}{axis}.json")
    json_samples = {str(k): v for k, v in samples.items()}
    with open(path, 'w') as f:
        json.dump({'axis': axis, 'duration': duration, 'samples': json_samples}, f, indent=2)
    print(f"  Guardado en {path}")
    print(f"\nSiguiente paso: corré el otro barrido (--axis {'X' if axis=='Y' else 'Y'})")
    print(f"               o el veredicto (--verdict) si ya hiciste los 2.")


def detect_transitions(samples):
    """Para cada sensor, detecta el timestamp del primer cruce ascendente
    (negro→blanco, cuando la hoja blanca llega al sensor)."""
    transitions = {}
    stats = {}
    for idx, vals in samples.items():
        nums = [v for _, v in vals]
        if not nums:
            continue
        vmin, vmax = min(nums), max(nums)
        rng = vmax - vmin
        if rng < 40:  # No hubo transición clara
            stats[idx] = {'min': vmin, 'max': vmax, 'range': rng,
                          'transition_t': None, 'note': 'sin transición (rango pequeño)'}
            continue
        threshold = vmin + rng * 0.5  # cruce a la mitad del rango
        # Buscar primer cruce ASCENDENTE: prev < threshold y current > threshold
        t_trans = None
        prev_val = None
        for t, val in vals:
            if prev_val is not None and prev_val < threshold and val >= threshold:
                t_trans = t
                break
            prev_val = val
        # Fallback: primer frame por encima del umbral
        if t_trans is None:
            for t, val in vals:
                if val > threshold:
                    t_trans = t
                    break
        stats[idx] = {'min': vmin, 'max': vmax, 'range': rng,
                      'threshold': int(threshold), 'transition_t': t_trans, 'note': 'OK'}
        if t_trans is not None:
            transitions[idx] = t_trans
    return transitions, stats


def compute_score_for_direction(transitions, axis, direction):
    """Calcula score para una dirección específica del barrido.
    direction='positive' = hoja entra desde el lado negativo del eje (X- o Y-),
                          entonces sensor con coord MENOR se tapa PRIMERO (t menor).
    direction='negative' = hoja entra desde el lado positivo (X+ o Y+),
                          entonces sensor con coord MAYOR se tapa PRIMERO.
    """
    coord_idx = 1 if axis == 'Y' else 0
    pairs_correct = 0
    pairs_wrong = 0
    inversions = []
    sensor_ids = sorted(transitions.keys())
    for i in range(len(sensor_ids)):
        for j in range(i + 1, len(sensor_ids)):
            a, b = sensor_ids[i], sensor_ids[j]
            if a >= len(SENSOR_POS) or b >= len(SENSOR_POS):
                continue
            coord_a = SENSOR_POS[a][coord_idx]
            coord_b = SENSOR_POS[b][coord_idx]
            if abs(coord_a - coord_b) < 5:  # mismo coord ±5mm, no contar
                continue
            t_a, t_b = transitions[a], transitions[b]
            if abs(t_a - t_b) < 0.15:  # mismo tiempo (within resolución), no contar
                continue
            # Esperado según dirección:
            if direction == 'positive':
                expected = coord_a < coord_b  # a tiene coord menor → se tapa antes
            else:  # negative
                expected = coord_a > coord_b  # a tiene coord mayor → se tapa antes
            actual = t_a < t_b
            if expected == actual:
                pairs_correct += 1
            else:
                pairs_wrong += 1
                inversions.append((a, b, coord_a, coord_b, t_a, t_b))
    total = pairs_correct + pairs_wrong
    score = 100.0 * pairs_correct / total if total else 0.0
    return {
        'score': score,
        'correct': pairs_correct,
        'wrong': pairs_wrong,
        'total': total,
        'inversions': inversions,
        'direction': direction,
    }


def compute_best_score(transitions, axis):
    """Prueba ambas direcciones y devuelve el mejor score."""
    pos = compute_score_for_direction(transitions, axis, 'positive')
    neg = compute_score_for_direction(transitions, axis, 'negative')
    best = pos if pos['score'] >= neg['score'] else neg
    return best, pos, neg


def analyze_axis(axis):
    path = os.path.join(CAPTURE_DIR, f"{SWEEP_PREFIX}{axis}.json")
    if not os.path.isfile(path):
        print(f"\n[Barrido {axis}] No encontré la captura ({path}). Corré primero:")
        print(f"   python diag_position_sweep.py --port COM10 --axis {axis}")
        return False
    with open(path) as f:
        data = json.load(f)
    samples = {int(k): v for k, v in data['samples'].items()}
    transitions, stats = detect_transitions(samples)

    print(f"\n=== Barrido {axis} ({len(transitions)} sensores con transición clara) ===")

    if not transitions:
        print("  Ningún sensor mostró transición clara.")
        return False

    best, pos, neg = compute_best_score(transitions, axis)
    dir_label = 'X-/Y- → X+/Y+ (positiva)' if best['direction'] == 'positive' else 'X+/Y+ → X-/Y- (negativa)'
    print(f"\n  Dirección detectada: {dir_label}")
    print(f"  Score: {best['score']:.0f}%  ({best['correct']}/{best['total']} pares correctos)")
    print(f"  (Score dirección contraria: {neg['score']:.0f}%  →  {pos['score']:.0f}%)")

    # Orden temporal: primer en taparse → último
    order_obs = sorted(transitions.items(), key=lambda x: x[1])
    coord_idx = 1 if axis == 'Y' else 0
    coord_name = 'Y' if axis == 'Y' else 'X'

    print(f"\n  Primeros 5 en taparse:")
    for i, t in order_obs[:5]:
        if i < len(SENSOR_POS):
            print(f"    S{i:2d}  t={t:.2f}s  {coord_name}={SENSOR_POS[i][coord_idx]:+6.1f}mm")
    print(f"  Últimos 5 en taparse:")
    for i, t in order_obs[-5:]:
        if i < len(SENSOR_POS):
            print(f"    S{i:2d}  t={t:.2f}s  {coord_name}={SENSOR_POS[i][coord_idx]:+6.1f}mm")

    # Veredicto numérico
    if best['score'] >= 90:
        print(f"\n  ✅ Posiciones OK para eje {axis} (score ≥ 90%)")
    elif best['score'] >= 70:
        print(f"\n  🟡 Posiciones MAYORMENTE OK para eje {axis} (score 70-90%)")
    else:
        print(f"\n  ❌ Posiciones SOSPECHOSAS para eje {axis} (score < 70%)")

    # Reportar inversiones más relevantes (orden por diferencia de coordenada)
    if best['inversions']:
        sig_inv = sorted(best['inversions'],
                         key=lambda x: -abs(x[2] - x[3]))[:6]
        print(f"\n  Inversiones más relevantes (esperaba uno antes pero llegó después):")
        for a, b, ca, cb, ta, tb in sig_inv:
            print(f"    S{a:2d}({coord_name}={ca:+6.1f})  vs  S{b:2d}({coord_name}={cb:+6.1f}): "
                  f"t={ta:.2f}s y t={tb:.2f}s (Δcoord={ca-cb:+.0f}mm, Δt={ta-tb:+.2f}s)")

    # Sensores sin cambio
    no_change = [i for i, s in stats.items() if s.get('note') != 'OK']
    if no_change:
        print(f"\n  ⚠️ {len(no_change)} sensores sin transición clara: {no_change}")
        print(f"     (no fueron tapados, o el barrido fue demasiado rápido para detectar)")

    return True


def verdict():
    print()
    print("=" * 70)
    print(" VEREDICTO POSICIONAMIENTO FÍSICO DE SENSORES (SENSOR_POS[])")
    print("=" * 70)
    print("Compara el orden temporal de transición negro→blanco con el orden")
    print("esperado según las coordenadas (x, y) del doc canónico")
    print("`hardware/electronics/down-board-pack/01-pinout-y-posiciones.md` §5b.")
    print()
    print("Score = % de pares de sensores cuya secuencia coincide con lo esperado.")
    print("OK ≥ 90%  |  Mayormente OK 70-90%  |  Sospechoso < 70%")
    print()

    ok_y = analyze_axis('Y')
    ok_x = analyze_axis('X')

    print("\n" + "=" * 70)
    if ok_y and ok_x:
        print(" Veredicto: ambos ejes analizados. Si los 2 dieron ≥ 90%, el")
        print(" SENSOR_POS[] del firmware coincide con la realidad física.")
        print(" Si alguno dio < 70%, verificá orientación física del PCB en")
        print(" el chasis (rotado? espejado? offset?) y compará con §5b.")
    else:
        print(" Veredicto incompleto. Faltan capturas de uno o ambos ejes.")
    print("=" * 70)


if __name__ == '__main__':
    p = argparse.ArgumentParser(
        description='Validación de SENSOR_POS[] de la placa DOWN por barridos.')
    p.add_argument('--port', help='Puerto serial (ej. COM10)')
    p.add_argument('--axis', choices=['X', 'Y'], help='Eje del barrido')
    p.add_argument('--duration', type=float, default=10.0,
                   help='Segundos de captura (default 10, recomendado 8-12)')
    p.add_argument('--verdict', action='store_true',
                   help='Analizá las 2 capturas (Y y X) y dame el veredicto')
    args = p.parse_args()

    if args.verdict:
        verdict()
    elif args.port and args.axis:
        capture_sweep(args.port, args.axis, args.duration)
    else:
        p.print_help()
        sys.exit(1)
