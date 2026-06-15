#!/usr/bin/env python3
# test_solve_homografia.py — suite host (sin camara, sin banco) del solver de homografia.
# Corre en la PC: python test_solve_homografia.py  (necesita numpy).
#
# Fija el comportamiento del UNICO calculo de calibracion 100% PC y queda como red de
# regresion (2027): recuperacion de una H conocida, error de reproyeccion, y los guards
# nuevos (colinealidad, <4 puntos, H[2,2]~0, parseo CSV). NO necesita hardware.

import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np            # noqa: E402
import solve_homografia as sh  # noqa: E402

_PASS = 0
_FAIL = 0


def check(cond, name):
    global _PASS, _FAIL
    if cond:
        _PASS += 1
        print("  PASS", name)
    else:
        _FAIL += 1
        print("  FAIL", name)


def expect_raises(fn, name):
    try:
        fn()
        check(False, name + " (NO lanzo ValueError)")
    except ValueError:
        check(True, name)


def test_recovers_known_h():
    H_true = np.array([
        [4.49341044e-02, -9.48228474e-01, 7.78932109e+02],
        [-2.39913185e+00, -5.65934886e-02, 3.91128921e+02],
        [-1.81344856e-03, 1.15408531e-01, 1.00000000e+00],
    ])
    corr = []
    for v in (55, 125, 205):
        for u in (40, 160, 280):
            xy = sh.apply_h(H_true, u, v)
            corr.append((u, v, xy[0], xy[1]))
    H = sh.solve_homography(corr)
    mean_e, max_e = sh.reprojection_error(H, corr)
    check(max_e < 1e-6, "recupera H conocida (reproyeccion ~0)")
    check(np.abs(H - H_true).max() < 1e-3, "H_est ~= H_true")


def test_few_points_raises():
    expect_raises(lambda: sh.solve_homography([(0, 0, 0, 0), (1, 1, 1, 1), (2, 2, 2, 2)]),
                  "<4 puntos lanza mensaje claro")


def test_collinear_pixels_raises():
    # pixeles sobre la recta u=v (colineales); fisico variado.
    corr = [(10, 10, 0, 0), (20, 20, 5, 1), (30, 30, 10, 2), (40, 40, 15, 3), (50, 50, 20, 4)]
    expect_raises(lambda: sh.solve_homography(corr), "pixeles colineales lanza")


def test_collinear_physical_raises():
    # fisico sobre la recta X=0 (colineal); pixeles bien distribuidos.
    corr = [(40, 55, 0, 0), (160, 55, 0, 10), (280, 55, 0, 20), (160, 125, 0, 30), (40, 205, 0, 40)]
    expect_raises(lambda: sh.solve_homography(corr), "fisico colineal lanza")


def test_degenerate_helper():
    check(sh.points_are_degenerate([(0, 0), (1, 1), (2, 2), (3, 3)]), "colineal => degenerado")
    check(not sh.points_are_degenerate([(0, 0), (1, 0), (0, 1), (1, 1)]), "cuadrado => no degenerado")
    check(sh.points_are_degenerate([(5, 5), (5, 5), (5, 5)]), "<4/coincidentes => degenerado")


def test_apply_h():
    H = np.eye(3)
    check(sh.apply_h(H, 5, 7) == (5.0, 7.0), "apply_h identidad")
    Hdeg = np.array([[1, 0, 0], [0, 1, 0], [0, 0, 0.0]])
    check(sh.apply_h(Hdeg, 5, 7) is None, "apply_h denom~0 => None")


def test_load_csv():
    content = "# comentario\n40,55,-30,100\n160 125 0 50   # inline\n\n280,205,30,25\n10,10,1,1\n"
    fd, path = tempfile.mkstemp(suffix=".csv")
    os.close(fd)
    with open(path, "w") as f:
        f.write(content)
    try:
        corr = sh.load_csv(path)
        check(len(corr) == 4, "csv: 4 filas (ignora '#' y vacias)")
        check(corr[0] == (40.0, 55.0, -30.0, 100.0), "csv: primera fila parseada (coma)")
        check(corr[1] == (160.0, 125.0, 0.0, 50.0), "csv: fila con espacios + comentario inline")
    finally:
        os.remove(path)


def test_load_csv_bad_row():
    fd, path = tempfile.mkstemp(suffix=".csv")
    os.close(fd)
    with open(path, "w") as f:
        f.write("40,55,-30\n")   # solo 3 numeros
    try:
        expect_raises(lambda: sh.load_csv(path), "csv: fila con !=4 numeros lanza")
    finally:
        os.remove(path)


def main():
    tests = [
        test_recovers_known_h, test_few_points_raises, test_collinear_pixels_raises,
        test_collinear_physical_raises, test_degenerate_helper, test_apply_h,
        test_load_csv, test_load_csv_bad_row,
    ]
    for t in tests:
        print(t.__name__)
        t()
    print("\n%d PASS, %d FAIL" % (_PASS, _FAIL))
    sys.exit(1 if _FAIL else 0)


if __name__ == "__main__":
    main()
