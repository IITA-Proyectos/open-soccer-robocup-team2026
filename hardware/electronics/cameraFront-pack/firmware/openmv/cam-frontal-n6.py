# cam-frontal-n6.py — Cámara FRONTAL del robot Soccer 2026 — OpenMV N6 (STM32N6)
#
# Port del template H7 a la OpenMV N6 (módulo `sensor` deprecado → `csi`).
# Bugs P0 corregidos: sentinel no-blob → Y_coded=0; clamp anti-crash [0,255].
# Detección find_blobs + LAB (pelota naranja 201 / arco amarillo 202 / arco azul 203).
#
# ╔══════════════════════════════════════════════════════════════════════════╗
# ║ ★ BRING-UP vs COMPETENCIA — leer ★                                         ║
# ║  BRING_UP=True  → autos (WB/gain/exposición) ON: SE VE imagen para calibrar.║
# ║                   Usar AHORA, hasta tener los thresholds LAB.              ║
# ║  BRING_UP=False → autos OFF + exposición fija: estable para competir (la    ║
# ║                   luz no rompe los LAB). Pasar a False DESPUÉS de calibrar. ║
# ╠══════════════════════════════════════════════════════════════════════════╣
# ║ [UART]  UART_PORT: confirmar cuál UART de la N6 va al Serial3 del Teensy   ║
# ║         (frontal = conector U8). Probar 1/2/3.                             ║
# ║ [LAB]   Recalibrar los 3 thresholds en la N6 (sensor PAG7936 ≠ H7) con el   ║
# ║         Threshold Editor del IDE, en la luz real. SIN ESTO NO DETECTA.     ║
# ║ [HOMOG] H_MATRIX es placeholder; recalibrar para ESTA cámara.             ║
# ║ [FLIP]  HMIRROR/VFLIP = montaje 180° (conector arriba). Verificar preview. ║
# ║ [LED]   SIN LEDs de diagnóstico a propósito: la API de LED de la N6 difiere ║
# ║         del H7 y hacía crashear el loop. No hacen falta para detectar/enviar.║
# ╚══════════════════════════════════════════════════════════════════════════╝

import csi
import time
import math

# --- UART: en N6 el estándar es machine.UART; pyb como fallback ---
try:
    from machine import UART
except ImportError:
    from pyb import UART

# ============================================================================
# ★ CONFIG — BRING-UP / CALIBRACIÓN ★
# ============================================================================
BRING_UP  = True        # True = autos ON (ver imagen para calibrar). False = competencia.

CAM_ID    = 0           # 0 = FRONTAL (informativo; el TOP distingue por puerto UART)
UART_PORT = 3           # ⚠️ CONFIRMAR en N6 — ¿cuál UART va al Serial3 del Teensy?
UART_BAUD = 19200       # debe coincidir con cameras_runtime.cpp del TOP (no cambiar)

EXPOSURE_US = 37000     # ⚠️ solo se usa si BRING_UP=False. RE-MEDIR en la N6.

# Montaje físico 180° (conector arriba) → HMIRROR+VFLIP=True compensa (verificar preview).
HMIRROR = True
VFLIP   = True

# ⚠️ RECALIBRAR H_MATRIX (4 puntos conocidos en el suelo). Placeholder de desarrollo.
H_MATRIX = [
    [ 4.49341044e-02, -9.48228474e-01,  7.78932109e+02],
    [-2.39913185e+00, -5.65934886e-02,  3.91128921e+02],
    [-1.81344856e-03,  1.15408531e-01,  1.00000000e+00],
]
CAM_HEIGHT_CM  = 18.7                    # ⚠️ MEDIR altura de la cámara sobre el suelo
BALL_RADIUS_CM = 13.5 / (2 * math.pi)    # radio pelota = circunferencia / 2π

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

# --- Inicialización del sensor (API `csi` de la N6) ---
csi0 = csi.CSI()
csi0.reset()
csi0.pixformat(csi.RGB565)
csi0.framesize(csi.QVGA)
if BRING_UP:
    # Autos ON: para VER imagen y calibrar. (Competencia → poner BRING_UP=False.)
    csi0.auto_whitebal(True)
    csi0.auto_gain(True)
    csi0.auto_exposure(True)
else:
    # Autos OFF + exposición fija: la luz no rompe los thresholds LAB.
    csi0.auto_whitebal(False)
    csi0.auto_gain(False)
    csi0.auto_exposure(False, exposure_us=EXPOSURE_US)
csi0.hmirror(HMIRROR)
csi0.vflip(VFLIP)
csi0.snapshot(time=500)                  # estabilización (reemplaza skip_frames)

uart = UART(UART_PORT, UART_BAUD)

clock = time.clock()

# ============================================================================
# TRANSFORMACIÓN pixel → coord física (con clamp anti-crash)
# ============================================================================
def transformar(u, v):
    H = H_MATRIX
    denom = H[2][0] * u + H[2][1] * v + H[2][2]
    if abs(denom) < 1e-6:
        return SENTINEL_X, SENTINEL_Y_CODED      # caso degenerado → no-detectado

    x = (H[0][0] * u + H[0][1] * v + H[0][2]) / denom
    y = (H[1][0] * u + H[1][1] * v + H[1][2]) / denom

    # Corrección de perspectiva (la pelota tiene radio, no es punto en el suelo):
    X = x * (CAM_HEIGHT_CM - BALL_RADIUS_CM) / CAM_HEIGHT_CM
    Y = y * (CAM_HEIGHT_CM - BALL_RADIUS_CM) / CAM_HEIGHT_CM

    X = max(-127, min(200, X))
    Y = max(-100, min(100, Y))

    Y_coded = int(Y) + 100                       # ∈ [0, 200]
    X_int   = int(X)
    if X_int == 0 and Y_coded == 0:              # no colisionar con el sentinel
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
# LOOP PRINCIPAL — detección + 9 bytes por UART
# ============================================================================
while True:
    clock.tick()
    img = csi0.snapshot()

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

    # Todos uint8 por el clamp → no crashea bytearray:
    packet = bytearray([201, Xp, Ypc, 202, Xam, Yamc, 203, Xaz, Yazc])
    uart.write(packet)
    # SIN print() en producción (consume tiempo y baja los fps).
