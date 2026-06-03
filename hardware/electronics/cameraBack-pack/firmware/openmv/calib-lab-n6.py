# calib-lab-n6.py — KIT DE CALIBRACIÓN LAB para las cámaras OpenMV N6 (Soccer 2026)
#
# Qué hace
# --------
# Herramienta de BANCO para sacar los 3 thresholds LAB (pelota naranja / arco
# amarillo / arco azul) RÁPIDO y bajo la luz real (la del taller HOY, la del
# venue en Incheon). NO transmite por UART, NO compite: sólo te ayuda a leer el
# color real del objeto y a verificar que el threshold lo agarra limpio.
#
# Es un script APARTE a propósito: no toca `cam-frontal-n6.py` / `cam-trasera-n6.py`
# (el path de competencia que ya anda). Cuando terminás de calibrar, copiás los
# 3 tuples que imprime este script a la cámara de producción.
#
# Cómo se usa (resumen — procedimiento completo en docs/firmware/CALIBRACION-VISION-N6.md)
# --------------------------------------------------------------------------------------
#   1. OpenMV IDE → abrir este .py → ▶ Run (corre de RAM, NO lo guardes como main.py).
#   2. Mirá el framebuffer del IDE: hay un cuadro (la "sonda") en el centro.
#   3. Poné el OBJETO a calibrar (la pelota / el arco) llenando ese cuadro.
#   4. Elegí qué color estás tuneando con TARGET (abajo). La consola imprime:
#        • el LAB real del objeto dentro de la sonda (min/mean/max por canal),
#        • un TUPLE sugerido listo para pegar (con margen),
#        • cuántos píxeles agarra el threshold ACTUAL (para ver si está bien).
#   5. Pegá el tuple sugerido en el threshold de ese color (abajo), volvé a Run,
#      y ajustá hasta que el cuadro verde rodee SOLO el objeto y nada del fondo.
#   6. Repetí para los 3 colores. Pasá los 3 tuples finales a la cámara de producción.
#
# ⚠️ VERIFICAR EN BANCO (no pude probarlo en la N6 desde acá):
#   • `img.get_statistics(roi=...)` y `img.draw_*` son API estándar de OpenMV y
#     deberían andar con el módulo `sensor` (igual que find_blobs). Si algo
#     crashea, avisá y lo adapto — la misma trampa que csi/machine.UART en esta N6.
#   • Rangos LAB de OpenMV: L ∈ [0,100], A ∈ [-128,127], B ∈ [-128,127].

import sensor
import time

# ============================================================================
# ★ QUÉ ESTÁS CALIBRANDO ★  — cambiá esto y volvé a Run
# ============================================================================
TARGET = "naranja"        # "naranja" (pelota) | "amarillo" (arco) | "azul" (arco)

MARGEN = 6                # margen que se suma/resta al min/max medido para el tuple sugerido
PROBE_FRAC = 0.18         # tamaño de la sonda central como fracción del frame (0.18 ≈ 18%)
PRINT_EACH = 12           # imprimir cada N frames (para no inundar la consola)

# Thresholds ACTUALES (arrancá con los de producción; pegá acá lo que vayas afinando):
THRESHOLDS = {
    "naranja":  (21, 67,  18, 79, -32, 127),    # pelota  → header 201
    "amarillo": (17, 70, -27, 14,  38, 111),    # arco    → header 202
    "azul":     ( 4, 36, -13, 57, -64,  -4),    # arco    → header 203
}
PIXELS_MIN = {"naranja": 20, "amarillo": 600, "azul": 300}

# Flip del montaje 180° (igual que producción). Verificá que la imagen quede derecha.
HMIRROR = True
VFLIP   = True

# ============================================================================
# Init del sensor — IGUAL que el script que funciona (módulo `sensor`, autos ON
# para VER la imagen mientras calibrás). NO fijar exposición acá: querés ver.
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

# Sonda central (ROI donde ponés el objeto para leer su color).
W = sensor.width()
H = sensor.height()
pw = int(W * PROBE_FRAC)
ph = int(H * PROBE_FRAC)
PROBE = (W // 2 - pw // 2, H // 2 - ph // 2, pw, ph)

def clamp(v, lo, hi):
    return max(lo, min(hi, v))

def sugerir_tuple(st):
    # Construye un threshold (Lmin,Lmax,Amin,Amax,Bmin,Bmax) desde la sonda + margen.
    lmin = clamp(int(st.l_min()) - MARGEN, 0, 100)
    lmax = clamp(int(st.l_max()) + MARGEN, 0, 100)
    amin = clamp(int(st.a_min()) - MARGEN, -128, 127)
    amax = clamp(int(st.a_max()) + MARGEN, -128, 127)
    bmin = clamp(int(st.b_min()) - MARGEN, -128, 127)
    bmax = clamp(int(st.b_max()) + MARGEN, -128, 127)
    return (lmin, lmax, amin, amax, bmin, bmax)

frame = 0
print("=== CALIB LAB N6 ===  TARGET={}  (poné el objeto en el cuadro central)".format(TARGET))

while True:
    clock.tick()
    img = sensor.snapshot()

    thr = THRESHOLDS[TARGET]
    pmin = PIXELS_MIN[TARGET]

    # 1) Sonda central: dibujar el cuadro + medir el LAB real adentro.
    img.draw_rectangle(PROBE, color=(255, 255, 255))
    st = img.get_statistics(roi=PROBE)

    # 2) Blobs con el threshold ACTUAL: dibujar lo que agarra (feedback inmediato).
    blobs = img.find_blobs([thr], roi=(0, 0, W, H),
                           pixels_threshold=pmin, area_threshold=pmin, merge=True)
    biggest_px = 0
    for b in blobs:
        img.draw_rectangle(b.rect(), color=(0, 255, 0))
        img.draw_cross(b.cx(), b.cy(), color=(0, 255, 0))
        if b.pixels() > biggest_px:
            biggest_px = b.pixels()

    # 3) Consola, cada PRINT_EACH frames.
    frame += 1
    if frame % PRINT_EACH == 0:
        print("--- {}  fps={:.0f} ---".format(TARGET, clock.fps()))
        print(" sonda LAB  L[{:d}..{:d}] A[{:d}..{:d}] B[{:d}..{:d}]  (mean L{:.0f} A{:.0f} B{:.0f})".format(
            int(st.l_min()), int(st.l_max()),
            int(st.a_min()), int(st.a_max()),
            int(st.b_min()), int(st.b_max()),
            st.l_mean(), st.a_mean(), st.b_mean()))
        print(" TUPLE sugerido (margen {}): {}".format(MARGEN, sugerir_tuple(st)))
        print(" threshold actual agarra: {} blobs, mayor = {} px (min pedido {})".format(
            len(blobs), biggest_px, pmin))
