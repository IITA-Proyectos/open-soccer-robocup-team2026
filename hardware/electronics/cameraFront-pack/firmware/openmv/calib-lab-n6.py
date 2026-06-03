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
# NOVEDAD (recalibración <5 min, doc 04 #9): este script CICLA los 3 colores
# SOLO. En cada frame mide los 3 (naranja/amarillo/azul) y la consola imprime
# los 3 bloques juntos cada PRINT_EACH frames. NO hace falta re-Run cambiando
# TARGET a mano: poné el objeto en la sonda y leé el bloque del color que estés
# calibrando. TARGET sólo decide qué color se RESALTA con blobs en la imagen
# (para no saturar el framebuffer dibujando los 3 a la vez).
#
# Cómo se usa (resumen — procedimiento completo en docs/firmware/CALIBRACION-VISION-N6.md)
# --------------------------------------------------------------------------------------
#   1. OpenMV IDE → abrir este .py → ▶ Run (corre de RAM, NO lo guardes como main.py).
#   2. Mirá el framebuffer del IDE: hay un cuadro (la "sonda") en el centro.
#      Arriba se escribe el TARGET actual y cuántos blobs/px agarra (feedback visual).
#   3. Poné el OBJETO a calibrar (la pelota / el arco) llenando ese cuadro.
#   4. La consola imprime, para LOS 3 colores en cada bloque:
#        • el LAB real del objeto dentro de la sonda (min/mean/max por canal),
#        • un TUPLE sugerido listo para pegar (con margen),
#        • cuántos píxeles agarra el threshold ACTUAL (para ver si está bien).
#      Leé el bloque del color que tengas en la sonda.
#   5. Pegá el tuple sugerido en el threshold de ese color (abajo). Para VER en la
#      imagen si quedó limpio, poné ese color en TARGET y volvé a Run: el cuadro
#      verde debe rodear SOLO el objeto y nada del fondo.
#   6. Repetí para los 3 colores. Pasá los 3 tuples finales a la cámara de producción.
#
# ⚠️ VERIFICAR EN BANCO (no pude probarlo en la N6 real desde acá):
#   • `img.get_statistics(roi=...)`, `img.draw_string(...)` y los `img.draw_*` son
#     API estándar de OpenMV y deberían andar con el módulo `sensor` (igual que
#     find_blobs). POR LAS DUDAS van envueltos en try/except: si alguno NO existe
#     en esta N6 (la misma trampa que csi/machine.UART/pyb.LED), el script NO
#     crashea — avisa por consola que ese feature no está disponible y sigue.
#   • Rangos LAB de OpenMV: L ∈ [0,100], A ∈ [-128,127], B ∈ [-128,127].

import sensor
import time

# ============================================================================
# ★ QUÉ COLOR SE RESALTA EN LA IMAGEN ★  — la consola igual reporta los 3.
# ============================================================================
TARGET = "naranja"        # "naranja" (pelota) | "amarillo" (arco) | "azul" (arco)

COLORES = ("naranja", "amarillo", "azul")   # orden de ciclado en la consola

# --- Márgenes para el tuple sugerido ---
# MARGEN = margen global por defecto (compat hacia atrás).
# Si querés afinar fino: L (brillo) suele necesitar MÁS margen que A/B (color),
# así que podés separar MARGEN_L y MARGEN_AB. Si los dejás en None, se usa MARGEN.
MARGEN = 6                # margen global (fallback)
MARGEN_L = 10             # margen para el canal L (brillo) — None → usa MARGEN
MARGEN_AB = 6             # margen para canales A y B (color) — None → usa MARGEN

PROBE_FRAC = 0.18         # tamaño de la sonda central como fracción del frame (0.18 ≈ 18%)
PRINT_EACH = 12           # imprimir cada N frames (para no inundar la consola)

# Thresholds ACTUALES (arrancá con los de producción; pegá acá lo que vayas afinando):
THRESHOLDS = {
    "naranja":  (21, 67,  18, 79, -32, 127),    # pelota  → header 201
    "amarillo": (17, 70, -27, 14,  38, 111),    # arco    → header 202
    "azul":     ( 4, 36, -13, 57, -64,  -4),    # arco    → header 203
}
PIXELS_MIN = {"naranja": 20, "amarillo": 600, "azul": 300}

# Color del recuadro/cross de blobs por color (R,G,B) para distinguir en pantalla.
DRAW_COLOR = {"naranja": (255, 128, 0), "amarillo": (255, 255, 0), "azul": (0, 128, 255)}

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

# Flags de features que en banco resulten NO soportados por esta N6.
# Si un try/except los apaga, no volvemos a intentarlos ni a spamear la consola.
FEATURE_OK = {"get_statistics": True, "draw_string": True, "draw_shapes": True}


def _warn_once(feature, exc):
    # Avisa UNA sola vez por feature que no está disponible en esta placa.
    if FEATURE_OK.get(feature, False):
        print("⚠️ feature NO disponible en esta N6: {} ({}). "
              "Verificar en banco; el resto del kit sigue.".format(feature, exc))
    FEATURE_OK[feature] = False


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def _m_l():
    # Margen efectivo para L (brillo). Cae a MARGEN si MARGEN_L es None.
    return MARGEN if MARGEN_L is None else MARGEN_L


def _m_ab():
    # Margen efectivo para A/B (color). Cae a MARGEN si MARGEN_AB es None.
    return MARGEN if MARGEN_AB is None else MARGEN_AB


def sugerir_tuple(st):
    # Construye un threshold (Lmin,Lmax,Amin,Amax,Bmin,Bmax) desde la sonda + margen.
    # L usa MARGEN_L (más holgado por brillo); A/B usan MARGEN_AB (más fino por color).
    ml = _m_l()
    mab = _m_ab()
    lmin = clamp(int(st.l_min()) - ml, 0, 100)
    lmax = clamp(int(st.l_max()) + ml, 0, 100)
    amin = clamp(int(st.a_min()) - mab, -128, 127)
    amax = clamp(int(st.a_max()) + mab, -128, 127)
    bmin = clamp(int(st.b_min()) - mab, -128, 127)
    bmax = clamp(int(st.b_max()) + mab, -128, 127)
    return (lmin, lmax, amin, amax, bmin, bmax)


def medir_blobs(img, color):
    # Cuenta blobs del threshold ACTUAL de `color` en todo el frame.
    # Devuelve (n_blobs, biggest_px, lista_blobs). Lista vacía si falla find_blobs.
    thr = THRESHOLDS[color]
    pmin = PIXELS_MIN[color]
    try:
        blobs = img.find_blobs([thr], roi=(0, 0, W, H),
                               pixels_threshold=pmin, area_threshold=pmin, merge=True)
    except Exception as e:
        _warn_once("find_blobs", e)
        return (0, 0, [])
    biggest_px = 0
    for b in blobs:
        if b.pixels() > biggest_px:
            biggest_px = b.pixels()
    return (len(blobs), biggest_px, blobs)


frame = 0
print("=== CALIB LAB N6 ===  ciclando {}  (poné el objeto en el cuadro central)".format(
    ", ".join(COLORES)))
print("    TARGET={} es sólo el color RESALTADO en la imagen; la consola reporta los 3.".format(TARGET))

while True:
    clock.tick()
    img = sensor.snapshot()

    # 1) Sonda central: dibujar el cuadro + medir el LAB real adentro.
    try:
        img.draw_rectangle(PROBE, color=(255, 255, 255))
    except Exception as e:
        _warn_once("draw_shapes", e)

    st = None
    try:
        st = img.get_statistics(roi=PROBE)
    except Exception as e:
        _warn_once("get_statistics", e)

    # 2) Blobs del TARGET: dibujar SOLO ese color resaltado (no saturar la imagen).
    tcolor = DRAW_COLOR.get(TARGET, (0, 255, 0))
    n_t, big_t, blobs_t = medir_blobs(img, TARGET)
    if FEATURE_OK.get("draw_shapes", True):
        for b in blobs_t:
            try:
                img.draw_rectangle(b.rect(), color=tcolor)
                img.draw_cross(b.cx(), b.cy(), color=tcolor)
            except Exception as e:
                _warn_once("draw_shapes", e)
                break

    # 2b) Texto sobre la imagen: TARGET actual + #blobs/px que agarra (feedback inmediato).
    try:
        img.draw_string(2, 2,
                        "TARGET:{} blobs:{} px:{}".format(TARGET, n_t, big_t),
                        color=tcolor, scale=2)
    except Exception as e:
        _warn_once("draw_string", e)

    # 3) Consola, cada PRINT_EACH frames: los 3 colores juntos (sin re-Run).
    frame += 1
    if frame % PRINT_EACH == 0:
        print("=== sonda (poné cada objeto y leé su bloque) | fps={:.0f} | TARGET resaltado={} ===".format(
            clock.fps(), TARGET))
        if st is None:
            print("  (sin LAB: get_statistics no disponible en esta N6 — verificar en banco)")
        for color in COLORES:
            n_c, big_c, _ = medir_blobs(img, color)
            pmin = PIXELS_MIN[color]
            if st is not None:
                print(" [{}] sonda LAB  L[{:d}..{:d}] A[{:d}..{:d}] B[{:d}..{:d}]  (mean L{:.0f} A{:.0f} B{:.0f})".format(
                    color,
                    int(st.l_min()), int(st.l_max()),
                    int(st.a_min()), int(st.a_max()),
                    int(st.b_min()), int(st.b_max()),
                    st.l_mean(), st.a_mean(), st.b_mean()))
                print("      TUPLE sugerido (mL{} mAB{}): {}".format(
                    _m_l(), _m_ab(), sugerir_tuple(st)))
            print("      threshold actual agarra: {} blobs, mayor = {} px (min pedido {})".format(
                n_c, big_c, pmin))
