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
# correspondencias; con N>4 hace minimos cuadrados (mas robusto: un punto malo no
# arruina la H). Reporta el error de reproyeccion (medio y maximo) para que sepas
# si la calibracion quedo bien ANTES de pegarla en la camara.
#
# Uso
# ---
#   1. Lay la lona/grilla, corre calib-homografia-n6.py en la camara -> te imprime
#      la lista CORRESPONDENCIAS (pixel u,v  ->  fisico X,Y en cm).
#   2. Pega esa lista abajo (o pasala por archivo: --csv puntos.csv).
#   3. python solve_homografia.py            -> imprime el bloque H_MATRIX a pegar.
#      python solve_homografia.py --selftest -> verifica el algoritmo (sin datos reales).
#
# Convencion de coordenadas (igual que cam-*-n6.py / CONTRATO-DATOS-CAMARAS.md):
#   +Y = adelante del robot,  +X = derecha,  origen = proyeccion de la camara/robot.
#   La H mapea pixel (u,v) -> (X,Y) en cm SOBRE EL PLANO DEL SUELO (la correccion
#   por el radio de la pelota la hace la camara con (h-r)/h, no esta H).

import sys

try:
    import numpy as np
except ImportError:
    sys.exit("ERROR: falta numpy. Instalalo: pip install numpy")

# ============================================================================
# PEGÁ ACÁ las correspondencias que imprime calib-homografia-n6.py
# Formato: (u_pixel, v_pixel, X_cm, Y_cm)
# (este ejemplo es ilustrativo — reemplazalo por los datos reales del banco)
# ============================================================================
CORRESPONDENCIAS = [
    # (u,   v,    X,    Y)
    # (50,  60,  -30,  100),
    # (160, 55,    0,  100),
    # (270, 60,   30,  100),
    # (40, 130,  -30,   50),
    # (160,125,    0,   50),
    # (280,130,   30,   50),
    # (30, 210,  -30,   25),
    # (160,205,    0,   25),
    # (290,210,   30,   25),
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


def solve_homography(correspondencias):
    """correspondencias: lista de (u, v, X, Y). Devuelve H 3x3 (pixel -> fisico)."""
    if len(correspondencias) < 4:
        raise ValueError("Hacen falta >=4 correspondencias (no colineales). Tenes %d." % len(correspondencias))
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
    if abs(H[2, 2]) > 1e-12:
        H = H / H[2, 2]
    return H


def reprojection_error(H, correspondencias):
    """Devuelve (error_medio, error_max) en cm: aplica H a los pixeles y compara con el fisico real."""
    errs = []
    for (u, v, X, Y) in correspondencias:
        denom = H[2, 0] * u + H[2, 1] * v + H[2, 2]
        if abs(denom) < 1e-12:
            errs.append(float("inf"))
            continue
        x = (H[0, 0] * u + H[0, 1] * v + H[0, 2]) / denom
        y = (H[1, 0] * u + H[1, 1] * v + H[1, 2]) / denom
        errs.append(((x - X) ** 2 + (y - Y) ** 2) ** 0.5)
    errs = np.asarray(errs)
    return float(errs.mean()), float(errs.max())


def print_h_block(H):
    print("\n# --- Pegá esto en cam-*-n6.py (reemplaza H_MATRIX) ---")
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
    # Grilla de pixeles tipica (QVGA 320x240) bien distribuida:
    us = [40, 160, 280]
    vs = [55, 125, 205]
    corr = []
    for v in vs:
        for u in us:
            denom = H_true[2, 0] * u + H_true[2, 1] * v + H_true[2, 2]
            X = (H_true[0, 0] * u + H_true[0, 1] * v + H_true[0, 2]) / denom
            Y = (H_true[1, 0] * u + H_true[1, 1] * v + H_true[1, 2]) / denom
            corr.append((u, v, X, Y))
    H_est = solve_homography(corr)
    mean_e, max_e = reprojection_error(H_est, corr)
    # Comparar H_est vs H_true (ambas normalizadas a h22=1):
    diff = np.abs(H_est - H_true).max()
    print("[selftest] correspondencias sinteticas: %d" % len(corr))
    print("[selftest] reproyeccion media=%.6f cm  max=%.6f cm" % (mean_e, max_e))
    print("[selftest] |H_est - H_true|_max = %.3e" % diff)
    ok = (max_e < 1e-3) and (diff < 1e-3)
    print("[selftest] RESULTADO:", "OK (algoritmo correcto)" if ok else "FALLO")
    return 0 if ok else 1


def main():
    if "--selftest" in sys.argv:
        sys.exit(selftest())
    if not CORRESPONDENCIAS:
        print("No hay correspondencias cargadas.")
        print("Pegá la lista que imprime calib-homografia-n6.py en CORRESPONDENCIAS")
        print("(o corré con --selftest para verificar el algoritmo).")
        sys.exit(2)
    H = solve_homography(CORRESPONDENCIAS)
    mean_e, max_e = reprojection_error(H, CORRESPONDENCIAS)
    print("Correspondencias usadas: %d" % len(CORRESPONDENCIAS))
    print("Error de reproyeccion: medio=%.3f cm  max=%.3f cm" % (mean_e, max_e))
    if max_e > 3.0:
        print("⚠️  error alto (>3 cm). Revisa puntos mal ordenados o mal medidos.")
    print_h_block(H)


if __name__ == "__main__":
    main()
