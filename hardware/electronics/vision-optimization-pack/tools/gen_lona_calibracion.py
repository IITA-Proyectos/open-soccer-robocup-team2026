#!/usr/bin/env python3
# gen_lona_calibracion.py — Genera el PDF de la LONA de calibración de homografía.
#
# Produce un PDF A1 (594x841 mm) a ESCALA 1:1 con una grilla de puntos negros sobre
# blanco, a distancias EXACTAS conocidas, para calibrar la H_MATRIX de las cámaras
# OpenMV N6 (ver docs/firmware/CALIBRACION-HOMOGRAFIA-XY-N6.md).
#
# Uso:  python gen_lona_calibracion.py
# Salida: lona-calibracion-homografia-A1.pdf (en esta misma carpeta)
#
# IMPRIMIR: A1, al 100% / "tamaño real" (NO "ajustar a la página"). Verificar con
# la regla de control de escala impresa abajo (debe medir 200 mm exactos).
#
# La grilla acá DEBE coincidir con la config de calib-homografia-n6.py:
#   COLS_X_CM   -> COL_X_CM        (X absoluto = X impreso, si alineás la col X=0
#                                   con el eje delantero del robot)
#   ROWS_Y_CM   -> y RELATIVO al borde cercano (sumar NEAR_EDGE_Y_CM al usarlo)

import os

from reportlab.lib.units import mm
from reportlab.pdfgen import canvas

# ============================================================================
# ★ PARÁMETROS DE LA GRILLA (cm) — cambialos y regenerá; sincronizá con el .py de cámara
# ============================================================================
COLS_X_CM = [-24, -12, 0, 12, 24]        # X (cm), +X = derecha del robot. 12 cm de paso.
ROWS_Y_CM = [0, 15, 30, 45, 60]          # y (cm) RELATIVO al borde cercano. 15 cm de paso.
DOT_DIAM_MM = 24                         # diámetro de cada punto negro
LABEL_PUNTOS = True                      # escribir (x,y) al lado de cada punto

PAGE_W_MM = 594                          # A1 ancho
PAGE_H_MM = 841                          # A1 alto
NEAR_ROW_FROM_BOTTOM_MM = 130            # dónde cae la fila y=0 (deja lugar p/ regla+título)

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "lona-calibracion-homografia-A1.pdf")


def main():
    c = canvas.Canvas(OUT, pagesize=(PAGE_W_MM * mm, PAGE_H_MM * mm))
    cx = PAGE_W_MM / 2.0                  # centro horizontal (mm) = eje X=0

    def PX(x_cm):  # cm físico -> punto X de la página
        return (cx + x_cm * 10.0) * mm

    def PY(y_cm):  # cm físico (rel borde cercano) -> punto Y de la página
        return (NEAR_ROW_FROM_BOTTOM_MM + y_cm * 10.0) * mm

    # --- Puntos de la grilla ---
    r = (DOT_DIAM_MM / 2.0) * mm
    c.setFillColorRGB(0, 0, 0)
    c.setStrokeColorRGB(0, 0, 0)
    for y_cm in ROWS_Y_CM:
        for x_cm in COLS_X_CM:
            c.circle(PX(x_cm), PY(y_cm), r, stroke=0, fill=1)
            if LABEL_PUNTOS:
                c.setFont("Helvetica", 9)
                c.drawString(PX(x_cm) + r + 2 * mm, PY(y_cm) - 3 * mm,
                             "(%d, %d)" % (x_cm, y_cm))

    # --- Eje X=0 (centerline) punteado ---
    c.setDash(4, 6)
    c.setLineWidth(0.5)
    c.line(PX(0), PY(ROWS_Y_CM[0]) - 20 * mm, PX(0), PY(ROWS_Y_CM[-1]) + 25 * mm)
    c.setDash()

    # --- Flecha +Y / ADELANTE (arriba) ---
    top_y = PY(ROWS_Y_CM[-1]) + 25 * mm
    c.setLineWidth(2)
    c.line(PX(0), top_y, PX(0), top_y + 22 * mm)
    c.line(PX(0), top_y + 22 * mm, PX(0) - 5 * mm, top_y + 14 * mm)
    c.line(PX(0), top_y + 22 * mm, PX(0) + 5 * mm, top_y + 14 * mm)
    c.setFont("Helvetica-Bold", 12)
    c.drawCentredString(PX(0), top_y + 25 * mm, "ADELANTE  +Y  (alejandose del robot)")
    c.setFont("Helvetica", 10)
    c.drawCentredString(PX(0), top_y + 12 * mm, "+X = derecha del robot  ->")

    # --- Borde cercano (y=0) marcado ---
    c.setLineWidth(1)
    c.line(PX(COLS_X_CM[0]) - 10 * mm, PY(0), PX(COLS_X_CM[-1]) + 10 * mm, PY(0))
    c.setFont("Helvetica-Bold", 10)
    c.drawString(PX(COLS_X_CM[-1]) + 12 * mm, PY(0) - 1.5 * mm, "borde cercano (y=0)")

    # --- Regla de control de escala (200 mm exactos) ---
    ruler_y = 70 * mm
    ruler_x0 = 60 * mm
    ruler_x1 = ruler_x0 + 200 * mm
    c.setLineWidth(1.5)
    c.line(ruler_x0, ruler_y, ruler_x1, ruler_y)
    for xx in (ruler_x0, ruler_x1):
        c.line(xx, ruler_y - 4 * mm, xx, ruler_y + 4 * mm)
    # ticks cada 10 mm
    c.setLineWidth(0.5)
    x = ruler_x0
    while x <= ruler_x1 + 0.01:
        c.line(x, ruler_y, x, ruler_y + 2.5 * mm)
        x += 10 * mm
    c.setFont("Helvetica-Bold", 11)
    c.drawString(ruler_x0, ruler_y + 7 * mm,
                 "CONTROL DE ESCALA: esta linea debe medir 200 mm EXACTOS tras imprimir.")
    c.setFont("Helvetica", 10)
    c.drawString(ruler_x0, ruler_y - 9 * mm,
                 "Si no mide 200 mm -> reimprimi al 100% / 'tamano real' (NO 'ajustar a la pagina').")

    # --- Título / instrucciones (abajo) ---
    c.setFont("Helvetica-Bold", 13)
    c.drawString(60 * mm, 40 * mm,
                 "Lona de calibracion de HOMOGRAFIA  -  Camaras OpenMV N6  -  IITA Soccer 2026")
    c.setFont("Helvetica", 9.5)
    c.drawString(60 * mm, 33 * mm,
                 "Grilla %dx%d.  Columnas X(cm)=%s.  Filas y(cm) desde borde cercano=%s." %
                 (len(COLS_X_CM), len(ROWS_Y_CM),
                  "/".join(str(v) for v in COLS_X_CM),
                  "/".join(str(v) for v in ROWS_Y_CM)))
    c.drawString(60 * mm, 27 * mm,
                 "Colocar: alinear la columna X=0 con el eje delantero del robot; medir robot -> borde cercano = NEAR_EDGE_Y_CM.")
    c.drawString(60 * mm, 21 * mm,
                 "Procedimiento: docs/firmware/CALIBRACION-HOMOGRAFIA-XY-N6.md   |   Imprimir A1 al 100%.")

    c.showPage()
    c.save()
    print("PDF generado:", OUT)
    print("Puntos:", len(COLS_X_CM) * len(ROWS_Y_CM),
          "(%dx%d)" % (len(COLS_X_CM), len(ROWS_Y_CM)))
    print("Pagina: %dx%d mm (A1).  Imprimir al 100%%." % (PAGE_W_MM, PAGE_H_MM))


if __name__ == "__main__":
    main()
