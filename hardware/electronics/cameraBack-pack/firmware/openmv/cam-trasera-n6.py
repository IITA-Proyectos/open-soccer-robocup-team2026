# cam-trasera-n6.py — Cámara TRASERA del robot Soccer 2026 — OpenMV N6 (STM32N6)
#
# Basado en el `current-generic.py` que SÍ funciona en la N6 (fw 4.8.1): usa el
# módulo **`sensor`** y **`pyb.UART`** (probado: `csi` daba preview NEGRO y
# `machine.UART` crasheaba en write en esta placa). Migrar a csi/machine = post-Incheon.
#
# Mejoras vs el generic: bugs P0 corregidos (sentinel no-blob → Y_coded=0;
# clamp anti-crash [0,255]), sin LEDs (la API de LED de la N6 crasheaba el loop),
# y flag BRING_UP para alternar autos / exposición-fija.
#
# IMPORTANTE: la cámara trasera NO rota sus coordenadas acá. La rotación 180° por
# "mirar hacia atrás" la hace el TOP (`cameras_fusion.cpp:25-30`, cam_id=1). Esta
# cámara reporta lo que ve en SU PROPIO frame, como si fuera la única.
#
# ╔══════════════════════════════════════════════════════════════════════════╗
# ║ ★ BRING-UP vs COMPETENCIA — leer ★                                         ║
# ║  BRING_UP=True  → autos (WB/gain) ON: SE VE imagen para calibrar (AHORA).   ║
# ║  BRING_UP=False → autos OFF + exposición fija (competencia, tras calibrar). ║
# ╠══════════════════════════════════════════════════════════════════════════╣
# ║ [UART]  UART_PORT: confirmar cuál UART de la N6 va al Serial5 del Teensy   ║
# ║         (trasera = conector U9). Probar 1/2/3.                             ║
# ║ [LAB]   Recalibrar los 3 thresholds en la N6 (PAG7936 ≠ H7). SIN ESTO no detecta.║
# ║ [HOMOG] H_MATRIX: la trasera necesita SU PROPIA H (4 puntos DETRÁS del robot)║
# ║ [FLIP]  HMIRROR/VFLIP: montaje 180°; puede diferir del frontal (verificar). ║
# ╚══════════════════════════════════════════════════════════════════════════╝

import sensor
import time
import math

# --- UART: pyb.UART (lo que usa el generic que funciona en la N6) ---
from pyb import UART

# ============================================================================
# ★ CONFIG — BRING-UP / CALIBRACIÓN ★
# ============================================================================
BRING_UP  = True        # True = autos ON (ver imagen para calibrar). False = competencia.

CAM_ID    = 1           # 1 = TRASERA (informativo; el TOP distingue por puerto UART)
UART_PORT = 3           # N6 manda por UART3 → entra al Serial5 del Teensy (pin 21) ✅
UART_BAUD = 19200       # debe coincidir con cameras_runtime.cpp del TOP (no cambiar)

EXPOSURE_US = 37000     # ⚠️ solo se usa si BRING_UP=False. RE-MEDIR en la N6.

# Montaje físico 180° (conector arriba) → HMIRROR+VFLIP compensa. Verificar preview.
# (Puede diferir del frontal si quedó espejada por el montaje.)
HMIRROR = True
VFLIP   = True

# ⚠️ La trasera necesita SU PROPIA H_MATRIX (4 puntos en el suelo DETRÁS del robot).
# Esta es la del generic — NO sirve para la trasera. Recalibrar.
H_MATRIX = [
    [ 4.49341044e-02, -9.48228474e-01,  7.78932109e+02],
    [-2.39913185e+00, -5.65934886e-02,  3.91128921e+02],
    [-1.81344856e-03,  1.15408531e-01,  1.00000000e+00],
]
CAM_HEIGHT_CM  = 18.7                    # ⚠️ MEDIR (puede diferir del frontal)
BALL_RADIUS_CM = 13.5 / (2 * math.pi)

# ⚠️ RECALIBRAR los 3 thresholds LAB en la N6 (sensor distinto al H7):
NARANJA_THRESHOLD  = (21, 67, 18, 79, -32, 127)    # pelota naranja  → header 201
AMARILLO_THRESHOLD = (17, 70, -27, 14, 38, 111)    # arco amarillo   → header 202
AZUL_THRESHOLD     = (4, 36, -13, 57, -64, -4)     # arco azul       → header 203

NARANJA_PIXELS_MIN  = 20
AMARILLO_PIXELS_MIN = 600
AZUL_PIXELS_MIN     = 300

# --- Filtro de FORMA (solo pelota naranja) — menos false positives del fondo ---
# La pelota es redonda; los falsos del fondo suelen ser alargados/dispersos.
# Estos NO son valores de hardware: son umbrales de forma TUNEABLES en banco.
# Defaults conservadores. Los arcos (amarillo/azul) NO usan este filtro (solo área).
# Si BALL_SHAPE_FILTER=False → comportamiento EXACTO al de hoy (poder apagarlo es requisito).
BALL_SHAPE_FILTER = True
BALL_MIN_DENSITY = 0.45      # density = pixels / area del bounding box; < esto = rechazar
BALL_ASPECT_MIN = 0.5       # w()/h() mínimo (redondo ≈ 1); fuera de rango = rechazar
BALL_ASPECT_MAX = 2.0       # w()/h() máximo
# REDONDEZ/circularidad — fit del círculo de OpenMV (b.roundness(), 0..1). La
# pelota es esférica ⇒ roundness alto; reflejos/manchas naranjas del fondo son
# irregulares ⇒ roundness bajo. Conservador y TUNEABLE en banco. ⚠️ Si la fw
# 4.8.1 de la N6 NO expone roundness(), el try/except de is_ball_like lo ignora
# (FAIL-OPEN, igual que aspect/density): mejor ver la pelota que crashear/filtrar.
BALL_MIN_ROUNDNESS = 0.55    # b.roundness() < esto = rechazar (no esférico)

# ROI OPCIONAL de pelota — DESACTIVADO por default (frame completo).
# Un ROI que recorte la parte superior puede excluir horizonte/público y bajar
# false positives, PERO depende del montaje físico de la cámara. ⚠️ VERIFICAR en
# banco antes de activar; NO poner números que recorten el FOV real. Dejar en None.
BALL_ROI = None

# ── Contrato v2 (contract-schema 2) — packet de 11 bytes ──────────────────────
# (a) EJE-X SIMÉTRICO: X se codifica IGUAL que Y → X_coded = X + 100 (X∈[-100,100]
#     → 0..200). La pelota a la IZQUIERDA (X<0) ya es representable (antes colapsaba
#     a 0 = "al frente"). Ver research/2026-06-03-eje-x-codificacion-asimetrica.
# (b) SENTINEL INEQUÍVOCO "no detectado" = (X_coded=255, Y_coded=255). Como las
#     coords reales se clampean a [0,200], 255 es INALCANZABLE desde una detección
#     → cero colisión con un objeto real.
# (c) INTEGRIDAD: al final del packet van CRC8 (XOR de los 9 bytes de datos) + END.
# ⚠️ TOCA EL WIRE: re-flashear cámara + TOP juntos + validar en banco.
# NOTA: la trasera NO rota coords acá; la rotación 180° la hace el TOP (cam_id=1).
SENTINEL_CODED = 255    # X_coded==255 y Y_coded==255 ⇒ objeto no detectado (parser TOP)
COORD_OFFSET = 100      # coded = valor + 100, en AMBOS ejes
HEADER1, HEADER2, HEADER3 = 201, 202, 203
END_BYTE = 254          # fin de trama
# ============================================================================


def crc8(data):
    # XOR de los 9 bytes de datos (3 headers + 6 coords). Debe coincidir EXACTO
    # con cam_crc8() del TOP (cameras.h). Detecta bit-flips en el enlace UART.
    c = 0
    for b in data:
        c ^= b
    return c & 0xFF

# --- Inicialización del sensor (módulo `sensor`, IGUAL que el generic que funciona) ---
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)
if BRING_UP:
    sensor.set_auto_whitebal(True)
    sensor.set_auto_gain(True)
else:
    sensor.set_auto_whitebal(False)
    sensor.set_auto_gain(False)
    sensor.set_auto_exposure(False, exposure_us=EXPOSURE_US)
sensor.set_hmirror(HMIRROR)
sensor.set_vflip(VFLIP)
sensor.skip_frames(time=2000)            # dejar converger los autos (igual que el generic)

uart = UART(UART_PORT, UART_BAUD)

clock = time.clock()

# ============================================================================
# TRANSFORMACIÓN pixel → coord física (con clamp anti-crash)
# ============================================================================
def transformar(u, v):
    H = H_MATRIX
    denom = H[2][0] * u + H[2][1] * v + H[2][2]
    if abs(denom) < 1e-6:
        return SENTINEL_CODED, SENTINEL_CODED     # caso degenerado → no-detectado

    x = (H[0][0] * u + H[0][1] * v + H[0][2]) / denom
    y = (H[1][0] * u + H[1][1] * v + H[1][2]) / denom

    X = x * (CAM_HEIGHT_CM - BALL_RADIUS_CM) / CAM_HEIGHT_CM
    Y = y * (CAM_HEIGHT_CM - BALL_RADIUS_CM) / CAM_HEIGHT_CM

    # Clamp al rango físico simétrico [-100,100] en AMBOS ejes (X igual que Y).
    X = max(-100, min(100, X))
    Y = max(-100, min(100, Y))

    X_coded = round(X) + COORD_OFFSET            # ∈ [0, 200] (símetrico a Y)
    Y_coded = round(Y) + COORD_OFFSET            # ∈ [0, 200]
    # VIS-02 (eval 2026-06-04): round() (al entero más cercano) en vez de int()
    # (trunca hacia cero) → simetría +X/-X correcta. El clamp uint8 de abajo cubre el rango.
    # Clamp uint8 final → anti-crash en bytearray. Nunca llega al SENTINEL (255).
    X_coded = max(0, min(200, X_coded))
    Y_coded = max(0, min(200, Y_coded))
    return X_coded, Y_coded


def procesar_blob(blobs):
    if not blobs:
        return SENTINEL_CODED, SENTINEL_CODED     # no detectado → 255,255
    blob = max(blobs, key=lambda b: b.pixels())
    return transformar(blob.cx(), blob.cy())


def is_ball_like(b):
    # La pelota es redonda → rechaza blobs alargados/dispersos (fondo, líneas,
    # reflejos). Usa atributos universales de OpenMV (w/h, density); son umbrales
    # de FORMA tuneables en banco, NO valores de hardware.
    # Fail-open: si algún atributo no existe en esta N6 (misma cautela que
    # csi/machine.UART/pyb.LED), NO filtra — mejor ver la pelota que crashear.
    try:
        h = b.h()
        if h <= 0:
            return False
        aspect = b.w() / h
        if aspect < BALL_ASPECT_MIN or aspect > BALL_ASPECT_MAX:
            return False
        if b.density() < BALL_MIN_DENSITY:
            return False
        # Redondez/circularidad (DENTRO del try → fail-open si la fw no la expone):
        # OpenMV `roundness()` = fit del círculo en [0,1]. Rechaza naranjas no
        # esféricos (reflejos, fondo) que envenenan la FSM. Si roundness() no
        # existe en esta N6 (4.8.1), salta al except y NO filtra (= hoy).
        if b.roundness() < BALL_MIN_ROUNDNESS:
            return False
        return True
    except Exception:
        return True


def detectar_naranja(img):
    # Pelota: find_blobs (con ROI opcional) + filtro de forma opcional.
    if BALL_ROI is not None:
        blobs = img.find_blobs([NARANJA_THRESHOLD], roi=BALL_ROI,
                               pixels_threshold=NARANJA_PIXELS_MIN,
                               area_threshold=NARANJA_PIXELS_MIN, merge=True)
    else:
        blobs = img.find_blobs([NARANJA_THRESHOLD],
                               pixels_threshold=NARANJA_PIXELS_MIN,
                               area_threshold=NARANJA_PIXELS_MIN, merge=True)
    if BALL_SHAPE_FILTER:
        blobs = [b for b in blobs if is_ball_like(b)]
    return blobs


# ============================================================================
# LOOP PRINCIPAL — la trasera NO rota nada (lo hace el TOP). 11 bytes (contrato v2) por UART.
# Objetivo de frame rate: ≥25 Hz sostenido en QVGA (doc 04 #5) — confirmar en banco.
# ============================================================================
while True:
    clock.tick()
    img = sensor.snapshot()

    # Pelota: ROI opcional + filtro de forma (anti false-positives del fondo).
    naranja_blobs  = detectar_naranja(img)
    # Arcos: solo por área (son rectangulares — NO se les aplica filtro de forma).
    amarillo_blobs = img.find_blobs([AMARILLO_THRESHOLD],
                                    pixels_threshold=AMARILLO_PIXELS_MIN,
                                    area_threshold=AMARILLO_PIXELS_MIN, merge=True)
    azul_blobs     = img.find_blobs([AZUL_THRESHOLD],
                                    pixels_threshold=AZUL_PIXELS_MIN,
                                    area_threshold=AZUL_PIXELS_MIN, merge=True)

    Xp,  Ypc  = procesar_blob(naranja_blobs)
    Xam, Yamc = procesar_blob(amarillo_blobs)
    Xaz, Yazc = procesar_blob(azul_blobs)

    # Contrato v2: 9 bytes de datos + CRC8 + END. Todos uint8 por el clamp.
    data = [HEADER1, Xp, Ypc, HEADER2, Xam, Yamc, HEADER3, Xaz, Yazc]
    packet = bytearray(data + [crc8(data), END_BYTE])   # 11 bytes
    uart.write(packet)
    if BRING_UP:
        # bring-up: ver paquetes válidos + fps (medir ≥25 Hz). NO en competencia.
        print(list(packet), "fps", round(clock.fps()))
    # En competencia (BRING_UP=False) NO imprime — el print baja los fps.
