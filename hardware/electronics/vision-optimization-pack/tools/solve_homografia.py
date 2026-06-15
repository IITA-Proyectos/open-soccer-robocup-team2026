#!/usr/bin/env python3
# solve_homografia.py — Calcula la H_MATRIX 3x3 (pixel -> coord fisica cm) a partir
# de correspondencias punto-a-punto. CORRE EN LA PC (no en la camara), con numpy.
#
# Por que en la PC y no en la N6
# ------------------------------
# El algebra lineal (SVD / minimos cuadrados) es fragil en MicroPython y el
# ulab de la N6 no esta verificado en esta placa (misma cautela que csi/machine.UART).
# La camara SOLO reporta los pixeles de los puntos (calib-homografia-n6.py); el
# calculo pesado lo hace numpy en la notebook. Simple y CONFIABLE.
#
# Metodo
# ------
# DLT (Direct Linear Transform) con normalizacion de Hartley + SVD. Acepta N>=4
# correspondencias NO colineales; con N>4 hace minimos cuadrados (mas robusto: un
# punto malo no arruina la H). Reporta el error de reproyeccion (medio y maximo) y
# AVISA si la calibracion quedo mala ANTES de pegarla en la camara.
#
# Uso
# ---
#   1. Lay la lona/grilla, corre calib-homografia-n6.py en la camara -> te imprime
#      la lista CORRESPONDENCIAS (pixel u,v  ->  fisico X,Y en cm).
#   2. Pasala por archivo:   python solve_homografia.py --csv puntos.csv
#      (o pega el bloque abajo en CORRESPONDENCIAS y corre sin --csv).
#   3. Imprime el bloque H_MATRIX para pegar en camaras-openmv/main.py (PRODUCCION).
#   --selftest                 verifica el algoritmo (sin datos reales).
#   --csv FILE                 carga las correspondencias de un CSV (u,v,X,Y por fila).
#   --validate u,v [u,v ...]   despues de resolver, imprime el (X,Y) cm que la H
#                              predice para esos pixeles (confirmar la H ANTES de flashear).
#
# Formato CSV: una fila 'u,v,X,Y' por punto (coma o espacios). '#' = comentario.
#
# Convencion de coordenadas (igual que main.py / CONTRATO-DATOS-CAMARAS.md):
#   +Y = adelante del robot,  +X = derecha,  origen = proyeccion de la camara/robot.
#   La H mapea pixel (u,v) -> (X,Y) en cm SOBRE EL PLANO DEL SUELO (la correccion
#   por el radio de la pelota la hace la camara con (h-r)/h, no esta H).

import sys

try:
    import numpy as np
except ImportError:
    sys.exit("ERROR: falta numpy. Instalalo: pip install numpy")

# ============================================================================
# PEGÁ ACÁ las correspondencias que imprime calib-homografia-n6.py (o usa --csv).
# Formato: (u_pixel, v_pixel, X_cm, Y_cm)
# ============================================================================
CORRESPONDENCIAS = [
    # (u,   v,    X,    Y)
    # (50,  60,  -30,  100),
    # (160, 55,    0,  100),
    # (270, 60,   30,  100),
]


def _normalize(pts):
    """Normalizacion de Hartley: centra en 0 y escala para que la dist media sea sqrt(2).
    Mejora MUCHO el condicionamiento del SVD. Devuelve (pts_norm, T) con pts_norm = T @ pts_h."""
    pts = np.asarray(pts, dtype=float)
    centroid = pts.mean(axis=0)
    d = pts - centroid
    mean_dist = np.sqrt((d ** 2).sum(axis=1)).mean()
    if mean_dist < 1e-12:
        mean_dist = 1.0
    s = np.sqrt(2.0) / mean_dist
    T = np.array([[s, 0, -s * centroid[0]],
                  [0, s, -s * centroid[1]],
                  [0, 0, 1.0]])
    pts_h = np.hstack([pts, np.ones((len(pts), 1))])
    pts_n = (T @ pts_h.T).T
    return pts_n[:, :2], T


def apply_h(H, u, v):
    """Aplica H a un pixel (u,v) -> (X,Y) fisico en cm. None si el denominador ~0."""
    denom = H[2, 0] * u + H[2, 1] * v + H[2, 2]
    if abs(denom) < 1e-12:
        return None
    x = (H[0, 0] * u + H[0, 1] * v + H[0, 2]) / denom
    y = (H[1, 0] * u + H[1, 1] * v + H[1, 2]) / denom
    return (x, y)


def points_are_degenerate(pts):
    """True si los puntos 2D estan (casi) COINCIDENTES o COLINEALES -> DLT mal condicionado.
    Mira los valores singulares de los puntos centrados: si el 2do es ~0 respecto del 1ro,
    todos caen sobre una recta (rango 1) y la H seria invalida."""
    pts = np.asarray(pts, dtype=float)
    if len(pts) < 4:
        return True
    centered = pts - pts.mean(axis=0)
    sv = np.linalg.svd(centered, compute_uv=False)
    if sv[0] < 1e-9:                 # todos coincidentes
        return True
    return (sv[1] / sv[0]) < 1e-6    # rango 1 = colineales


def validate_correspondencias(corr):
    """Chequea las correspondencias ANTES de resolver. Lanza ValueError con mensaje
    CLARO (no un crash crudo) ante: <4 puntos, pixeles colineales, fisico colineal.
    Evita que el equipo se lleve una H invalida al banco creyendo que es buena."""
    if len(corr) < 4:
        raise ValueError(
            "Hacen falta >=4 correspondencias NO colineales. Tenes %d.\n"
            "  Recomendado: grilla 3x3 = 9 puntos bien distribuidos." % len(corr))
    src = [(c[0], c[1]) for c in corr]
    dst = [(c[2], c[3]) for c in corr]
    if points_are_degenerate(src):
        raise ValueError(
            "Los PIXELES estan (casi) colineales o coincidentes -> la H seria invalida.\n"
            "  Usa puntos en varias filas Y columnas (no todos sobre una linea).")
    if points_are_degenerate(dst):
        raise ValueError(
            "Los puntos FISICOS (X,Y cm) estan (casi) colineales -> la H seria invalida.\n"
            "  Verifica que la grilla cubra varias distancias Y lados de la lona.")


def solve_homography(correspondencias):
    """correspondencias: lista de (u, v, X, Y). Devuelve H 3x3 (pixel -> fisico cm).
    Lanza ValueError con mensaje claro si las correspondencias o la H son degeneradas."""
    validate_correspondencias(correspondencias)
    src = np.array([[c[0], c[1]] for c in correspondencias], dtype=float)  # pixeles
    dst = np.array([[c[2], c[3]] for c in correspondencias], dtype=float)  # fisico cm

    src_n, Ts = _normalize(src)
    dst_n, Td = _normalize(dst)

    A = []
    for (u, v), (X, Y) in zip(src_n, dst_n):
        A.append([-u, -v, -1, 0, 0, 0, u * X, v * X, X])
        A.append([0, 0, 0, -u, -v, -1, u * Y, v * Y, Y])
    A = np.asarray(A)
    _, _, Vt = np.linalg.svd(A)
    Hn = Vt[-1].reshape(3, 3)

    # Des-normalizar: H = Td^-1 @ Hn @ Ts
    H = np.linalg.inv(Td) @ Hn @ Ts
    if abs(H[2, 2]) <= 1e-12:
        raise ValueError(
            "H degenerada (H[2,2]~0): la calibracion no es valida. Suele pasar con\n"
            "  puntos colineales o mal medidos. NO la uses; re-corre la captura.")
    return H / H[2, 2]


def reprojection_error(H, correspondencias):
    """Devuelve (error_medio, error_max) en cm: aplica H a los pixeles vs el fisico real."""
    errs = []
    for (u, v, X, Y) in correspondencias:
        xy = apply_h(H, u, v)
        if xy is None:
            errs.append(float("inf"))
            continue
        errs.append(((xy[0] - X) ** 2 + (xy[1] - Y) ** 2) ** 0.5)
    errs = np.asarray(errs)
    return float(errs.mean()), float(errs.max())


def load_csv(path):
    """Lee correspondencias de un CSV: una fila 'u,v,X,Y' por punto (coma o espacios).
    Ignora lineas vacias y las que empiezan con '#'."""
    corr = []
    with open(path, "r") as f:
        for ln, raw in enumerate(f, 1):
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            parts = line.replace(",", " ").split()
            if len(parts) != 4:
                raise ValueError(
                    "CSV linea %d: esperaba 'u,v,X,Y' (4 numeros), vi: %r" % (ln, raw.strip()))
            try:
                corr.append(tuple(float(p) for p in parts))
            except ValueError:
                raise ValueError("CSV linea %d: numeros invalidos: %r" % (ln, raw.strip()))
    return corr


def print_h_block(H):
    # HI-6 (2026-06-15): el destino de PRODUCCION es camaras-openmv/main.py, NO los
    # cam-*-n6.py (DEPRECADOS 2026-06-08). Ver FUENTES-DE-VERDAD.md.
    print("\n# --- Pegá esto como H_MATRIX en camaras-openmv/main.py (script de PRODUCCION) ---")
    print("H_MATRIX = [")
    for row in H:
        print("    [{: .8e}, {: .8e}, {: .8e}],".format(row[0], row[1], row[2]))
    print("]")


def selftest():
    """Genera correspondencias sinteticas desde una H conocida y verifica que la recuperamos."""
    H_true = np.array([
        [4.49341044e-02, -9.48228474e-01, 7.78932109e+02],
        [-2.39913185e+00, -5.65934886e-02, 3.91128921e+02],
        [-1.81344856e-03, 1.15408531e-01, 1.00000000e+00],
    ])
    us = [40, 160, 280]
    vs = [55, 125, 205]
    corr = []
    for v in vs:
        for u in us:
            xy = apply_h(H_true, u, v)
            corr.append((u, v, xy[0], xy[1]))
    H_est = solve_homography(corr)
    mean_e, max_e = reprojection_error(H_est, corr)
    diff = np.abs(H_est - H_true).max()
    print("[selftest] correspondencias sinteticas: %d" % len(corr))
    print("[selftest] reproyeccion media=%.6f cm  max=%.6f cm" % (mean_e, max_e))
    print("[selftest] |H_est - H_true|_max = %.3e" % diff)
    ok = (max_e < 1e-3) and (diff < 1e-3)
    print("[selftest] RESULTADO:", "OK (algoritmo correcto)" if ok else "FALLO")
    return 0 if ok else 1


def _run_validate(H, pixels):
    """Imprime el (X,Y) cm que la H predice para cada pixel 'u,v'. HI-3."""
    print("\n# --- Validacion: pixel -> (X,Y) cm predicho por esta H ---")
    for p in pixels:
        try:
            u, v = (float(x) for x in p.replace(",", " ").split())
        except ValueError:
            print("  (pixel invalido: %r — usa 'u,v')" % p)
            continue
        xy = apply_h(H, u, v)
        if xy is None:
            print("  pixel(%s) -> denominador ~0 (H degenerada)" % p)
        else:
            print("  pixel(%g,%g) -> X=%.1f cm  Y=%.1f cm" % (u, v, xy[0], xy[1]))


def main():
    args = sys.argv[1:]
    if "--selftest" in args:
        sys.exit(selftest())

    # Fuente de correspondencias: --csv FILE o el bloque CORRESPONDENCIAS embebido.
    corr = list(CORRESPONDENCIAS)
    if "--csv" in args:
        i = args.index("--csv")
        if i + 1 >= len(args):
            sys.exit("ERROR: --csv requiere un archivo. Uso: --csv puntos.csv")
        try:
            corr = load_csv(args[i + 1])
        except (OSError, ValueError) as e:
            sys.exit("ERROR leyendo CSV: %s" % e)

    if not corr:
        print("No hay correspondencias cargadas.")
        print("  Opciones: --csv puntos.csv  |  pegar la lista en CORRESPONDENCIAS  |  --selftest")
        sys.exit(2)

    try:
        H = solve_homography(corr)
    except ValueError as e:
        sys.exit("ERROR de calibracion: %s" % e)

    mean_e, max_e = reprojection_error(H, corr)
    print("Correspondencias usadas: %d" % len(corr))
    print("Error de reproyeccion: medio=%.3f cm  max=%.3f cm" % (mean_e, max_e))
    # Umbrales del doc (CALIBRACION-HOMOGRAFIA-XY-N6.md): medio<2cm, max<3cm.
    if mean_e > 2.0 or max_e > 3.0:
        print("[!] ERROR ALTO (medio>2cm o max>3cm): NO uses esta H. Revisa puntos mal "
              "ordenados/medidos o re-corre la captura.")

    if "--validate" in args:
        i = args.index("--validate")
        pixels = args[i + 1:] or ["40,55", "160,125", "280,205"]
        _run_validate(H, pixels)
        return   # en modo validate no imprimimos el bloque H

    print_h_block(H)


if __name__ == "__main__":
    main()
