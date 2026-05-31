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
# ║ [UART]  UART_PORT: confirmar cuál UART de la N6 va al Serial7 del Teensy   ║
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
UART_PORT = 3           # ⚠️ CONFIRMAR en N6 — ¿cuál UART va al Serial7 del Teensy?
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

SENTINEL_X = 0
SENTINEL_Y_CODED = 0    # → Y = 0-100 = -100 en el parser TOP → is_visible = False
# ============================================================================

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
        return SENTINEL_X, SENTINEL_Y_CODED

    x = (H[0][0] * u + H[0][1] * v + H[0][2]) / denom
    y = (H[1][0] * u + H[1][1] * v + H[1][2]) / denom

    X = x * (CAM_HEIGHT_CM - BALL_RADIUS_CM) / CAM_HEIGHT_CM
    Y = y * (CAM_HEIGHT_CM - BALL_RADIUS_CM) / CAM_HEIGHT_CM

    X = max(-127, min(200, X))
    Y = max(-100, min(100, Y))

    Y_coded = int(Y) + 100
    X_int   = int(X)
    if X_int == 0 and Y_coded == 0:
        X_int = 1
    X_int   = max(0, min(255, X_int))            # ⚠️ clamp uint8 final → anti-crash
    Y_coded = max(0, min(255, Y_coded))
    return X_int, Y_coded


def procesar_blob(blobs):
    if not blobs:
        return SENTINEL_X, SENTINEL_Y_CODED
    blob = max(blobs, key=lambda b: b.pixels())
    return transformar(blob.cx(), blob.cy())


# ============================================================================
# LOOP PRINCIPAL — la trasera NO rota nada (lo hace el TOP). 9 bytes por UART.
# ============================================================================
while True:
    clock.tick()
    img = sensor.snapshot()

    naranja_blobs  = img.find_blobs([NARANJA_THRESHOLD],
                                    pixels_threshold=NARANJA_PIXELS_MIN,
                                    area_threshold=NARANJA_PIXELS_MIN, merge=True)
    amarillo_blobs = img.find_blobs([AMARILLO_THRESHOLD],
                                    pixels_threshold=AMARILLO_PIXELS_MIN,
                                    area_threshold=AMARILLO_PIXELS_MIN, merge=True)
    azul_blobs     = img.find_blobs([AZUL_THRESHOLD],
                                    pixels_threshold=AZUL_PIXELS_MIN,
                                    area_threshold=AZUL_PIXELS_MIN, merge=True)

    Xp,  Ypc  = procesar_blob(naranja_blobs)
    Xam, Yamc = procesar_blob(amarillo_blobs)
    Xaz, Yazc = procesar_blob(azul_blobs)

    packet = bytearray([201, Xp, Ypc, 202, Xam, Yamc, 203, Xaz, Yazc])
    uart.write(packet)
    if BRING_UP:
        print(list(packet))   # bring-up: ver en consola que salen paquetes válidos
    # En competencia (BRING_UP=False) NO imprime — el print baja los fps.
