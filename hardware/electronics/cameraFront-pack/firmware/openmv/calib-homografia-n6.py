# calib-homografia-n6.py — Captura de puntos para calibrar la HOMOGRAFÍA (XY) — OpenMV N6
#
# Qué hace
# --------
# Herramienta de BANCO para sacar la H_MATRIX (pixel → coord física en cm) RÁPIDO
# y CONFIABLE, usando una LONA/HOJA con una grilla de puntos NEGROS sobre fondo
# blanco. Detecta los puntos con find_blobs (lo único verificado en esta N6), los
# ORDENA en grilla, los DIBUJA NUMERADOS en el framebuffer, e IMPRIME las
# correspondencias (u,v píxel → X,Y cm) listas para pegar en el solver de PC
# (vision-optimization-pack/tools/solve_homografia.py).
#
# NO transmite por UART. NO compite. Es un script aparte (no toca cam-*-n6.py).
#
# Por qué grilla/lona y NO "pelota en puntos sueltos"
# ---------------------------------------------------
#  • 1 sola foto captura MUCHOS puntos → H por mínimos cuadrados (robusta).
#  • La cámara lee los píxeles (no los anotás a ojo → cero error humano).
#  • Repetible: en Incheon desenrollás la MISMA lona y recalibrás las 4 cámaras
#    en minutos.
# La "pelota en puntos" queda como VALIDACIÓN final (ver el doc), no como método.
#
# Procedimiento resumido (completo en docs/firmware/CALIBRACION-HOMOGRAFIA-XY-N6.md)
# --------------------------------------------------------------------------------
#  1. Montá la cámara en su posición DEFINITIVA. Poné la lona PLANA en el piso,
#     en el campo de visión, alineada (origen y eje +Y marcados → adelante).
#  2. Abrí este .py → ▶ Run (corre de RAM; NO lo guardes como main.py).
#  3. Mirá el framebuffer: cada punto detectado sale con un recuadro y un NÚMERO.
#  4. Ajustá GRID_COLS/GRID_ROWS, ROW_Y_CM y COL_X_CM (abajo) para que coincidan
#     con la lona REAL y con lo que VES (qué fila/columna es cuál). Re-Run.
#  5. Cuando los números cuadran con la grilla física, copiá el bloque
#     "CORRESPONDENCIAS = [...]" que imprime la consola.
#  6. En la PC: pegalo en solve_homografia.py y corrélo → te da la H_MATRIX.
#
# ⚠️ VERIFICAR EN BANCO (no se pudo probar en la N6 real desde acá):
#  • find_blobs con threshold oscuro (LAB L bajo) y draw_string/draw_rectangle son
#    API estándar de OpenMV; deberían andar con el módulo `sensor`. Si algo crashea,
#    avisá y lo adapto (misma trampa que csi/machine.UART/pyb.LED en esta placa).

import sensor
import time

# ============================================================================
# ★ CONFIG DE LA GRILLA FÍSICA (medila de la lona real, en cm) ★
# ============================================================================
# Estos defaults coinciden con la lona que genera gen_lona_calibracion.py
# (grilla 5x5 → lona-calibracion-homografia-A1.pdf). Si cambiás la lona, sincronizá acá.
#
# Coord X (cm) de cada COLUMNA = el X impreso en la lona. +X = derecha del robot.
# Alineá la columna X=0 con el eje delantero del robot al colocar la lona.
COL_X_CM = [-24, -12, 0, 12, 24]
# Filas de la lona, del borde CERCANO (y=0) al LEJANO, en cm (igual que el PDF):
Y_REL_CM = [0, 15, 30, 45, 60]
# ⚠️ MEDIR: distancia del robot al borde cercano (fila y=0) de la lona, en cm:
NEAR_EDGE_Y_CM = 20
# Por el VFLIP, la fila más LEJANA suele verse ARRIBA en la imagen. Verificá con los
# números que dibuja el script; si la fila de ARRIBA NO es la lejana, poné False.
IMAGE_TOP_IS_FAR = True

GRID_COLS = len(COL_X_CM)
GRID_ROWS = len(Y_REL_CM)
_abs_y = [NEAR_EDGE_Y_CM + y for y in Y_REL_CM]          # cercana→lejana (Y absoluto, cm)
ROW_Y_CM = list(reversed(_abs_y)) if IMAGE_TOP_IS_FAR else list(_abs_y)  # top→bottom imagen

# ============================================================================
# ★ DETECCIÓN DE LOS PUNTOS ★
# ============================================================================
# Flip del montaje — DEBE ser IGUAL al de cam-frontal-n6.py de ESTA cámara, porque
# la H se calibra en el MISMO frame (flipeado) que usa producción.
HMIRROR = True
VFLIP   = True

# Threshold OSCURO en LAB: puntos negros = L bajo (robusto a iluminación). Tuneable.
DARK_THRESHOLD = (0, 40, -128, 127, -128, 127)   # (Lmin,Lmax,Amin,Amax,Bmin,Bmax)
DOT_PIXELS_MIN = 30        # área mínima de un punto (subir si agarra ruido)
PRINT_EACH = 20            # imprimir cada N frames (no inundar la consola)

# ============================================================================
# Init del sensor — módulo `sensor` (lo que funciona en la N6), autos ON para VER.
# ============================================================================
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)        # 320x240
sensor.set_auto_whitebal(True)
sensor.set_auto_gain(True)
sensor.set_hmirror(HMIRROR)
sensor.set_vflip(VFLIP)
sensor.skip_frames(time=2000)

clock = time.clock()
N_EXPECTED = GRID_COLS * GRID_ROWS

print("=== CALIB HOMOGRAFIA N6 ===  grilla %dx%d (%d puntos)" % (GRID_COLS, GRID_ROWS, N_EXPECTED))
print("Poné la lona en el piso. Ajustá COL_X_CM / ROW_Y_CM para que casen con los numeros dibujados.")


def ordenar_en_grilla(blobs):
    # Ordena los blobs en filas (por cy) y columnas (por cx), tal como se ven en la
    # imagen: fila 0 = arriba, columna 0 = izquierda. Las filas se separan en los
    # GRID_ROWS-1 saltos MÁS GRANDES de cy (robusto a que la perspectiva comprima
    # las filas lejanas, donde cortar fijo cada GRID_COLS fallaría).
    bs = sorted(blobs, key=lambda b: b.cy())
    if len(bs) < GRID_ROWS:
        return [sorted(bs, key=lambda b: b.cx())]
    saltos = sorted(range(1, len(bs)), key=lambda i: bs[i].cy() - bs[i - 1].cy(), reverse=True)
    cortes = sorted(saltos[:GRID_ROWS - 1])
    filas = []
    prev = 0
    for corte in cortes + [len(bs)]:
        filas.append(sorted(bs[prev:corte], key=lambda b: b.cx()))   # dentro de fila: izq→der
        prev = corte
    return filas


frame = 0
while True:
    clock.tick()
    img = sensor.snapshot()

    blobs = img.find_blobs([DARK_THRESHOLD],
                           pixels_threshold=DOT_PIXELS_MIN,
                           area_threshold=DOT_PIXELS_MIN, merge=True)

    # Dibujar todos los blobs detectados (sin ordenar todavía) para feedback rápido.
    for b in blobs:
        img.draw_rectangle(b.rect(), color=(0, 255, 0))

    n = len(blobs)
    ok_count = (n == N_EXPECTED)

    if ok_count:
        filas = ordenar_en_grilla(blobs)
        # Numerar cada punto en orden de grilla (índice = fila*COLS + col).
        idx = 0
        for r in range(GRID_ROWS):
            for c in range(GRID_COLS):
                b = filas[r][c]
                img.draw_cross(b.cx(), b.cy(), color=(255, 0, 0))
                img.draw_string(b.cx() + 3, b.cy() - 3, str(idx), color=(255, 0, 0))
                idx += 1

    img.draw_string(2, 2, "blobs=%d/%d %s fps=%d" %
                    (n, N_EXPECTED, "OK" if ok_count else "AJUSTAR", round(clock.fps())),
                    color=(255, 255, 255))

    frame += 1
    if frame % PRINT_EACH == 0:
        if not ok_count:
            print("--- detectados %d puntos (esperaba %d). Ajustá DARK_THRESHOLD / DOT_PIXELS_MIN / la lona ---" % (n, N_EXPECTED))
        else:
            # Imprimir el bloque de correspondencias listo para el solver de PC.
            print("--- CORRESPONDENCIAS (pegar en solve_homografia.py) ---  fps=%d" % round(clock.fps()))
            print("CORRESPONDENCIAS = [")
            for r in range(GRID_ROWS):
                for c in range(GRID_COLS):
                    b = filas[r][c]
                    print("    (%d, %d, %d, %d)," % (b.cx(), b.cy(), COL_X_CM[c], ROW_Y_CM[r]))
            print("]")
