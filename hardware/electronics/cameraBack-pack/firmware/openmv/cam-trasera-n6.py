# cam-trasera-n6.py — Cámara TRASERA del robot Soccer 2026 — OpenMV N6 (STM32N6)
#
# Port del template H7 (`target-cam-trasera-template.py`) a la OpenMV **N6**.
# Lógica de detección y protocolo IDÉNTICOS; solo cambió la capa de cámara
# (`sensor` deprecado → `csi`). Bugs P0 ya corregidos (sentinel no-blob → Y_coded=0,
# clamp anti-crash [0,255], autos OFF).
#
# IMPORTANTE: la cámara trasera NO rota sus coordenadas acá. La rotación 180° la
# hace el TOP (`cameras_fusion.cpp:25-30`). Esta cámara reporta lo que ve en SU
# PROPIO frame, como si fuera la única.
#
# ╔══════════════════════════════════════════════════════════════════════════╗
# ║ ★ ANTES DE CONFIAR EN ESTE SCRIPT — CONFIRMAR / CALIBRAR EN BANCO (N6) ★   ║
# ╠══════════════════════════════════════════════════════════════════════════╣
# ║ [UART]  UART_PORT: la N6 tiene 3 UART. Confirmar cuál mapea a los pines    ║
# ║         que van al Serial7 del Teensy (trasera = conector U9). Probar 1/2/3.║
# ║ [API]   Migrado a `csi`. Si algún `csi.*` da error, ver la API exacta en   ║
# ║         docs.openmv.io/library/omv.csi.html (varía por versión de firmware).║
# ║ [LAB]   Los 3 thresholds son del sensor VIEJO (H7). La N6 tiene OTRO sensor║
# ║         (PAG7936 global shutter) → RECALIBRAR. SIN ESTO NO DETECTA.         ║
# ║ [EXPO]  EXPOSURE_US placeholder; re-medir (global shutter).                 ║
# ║ [HOMOG] H_MATRIX: la trasera necesita SU PROPIA H (4 puntos DETRÁS del      ║
# ║         robot). NO reusar la del frontal.                                   ║
# ║ [FLIP]  HMIRROR/VFLIP: probablemente distintos al frontal (montaje espejo). ║
# ╚══════════════════════════════════════════════════════════════════════════╝

import csi, image, time, math

try:
    from machine import UART
except ImportError:
    from pyb import UART

# ============================================================================
# ★ BLOQUE A CONFIRMAR / CALIBRAR EN BANCO (N6) ★
# ============================================================================
CAM_ID    = 1           # 1 = TRASERA (informativo; el TOP distingue por puerto UART)
UART_PORT = 3           # ⚠️ CONFIRMAR en N6 — ¿cuál de los 3 UART va al Serial7 del Teensy?
UART_BAUD = 19200       # debe coincidir con cameras_runtime.cpp del TOP (no cambiar)

EXPOSURE_US = 37000     # ⚠️ RE-MEDIR en la N6 (global shutter)

# ⚠️ Montaje físico rotado 180° (conector arriba) → HMIRROR+VFLIP=True COMPENSA
# ese montaje (giro de 180° de la imagen). Confirmar con el preview que quede
# DERECHA (puede diferir del frontal si quedó espejada por el montaje).
# OJO: la rotación por "mirar hacia ATRÁS" NO se hace acá — la hace el TOP en
# cameras_fusion.cpp (cam_id=1). Acá SOLO se corrige la orientación de la imagen.
HMIRROR = True
VFLIP   = True

# ⚠️ La trasera necesita SU PROPIA H_MATRIX (4 puntos en el suelo DETRÁS del robot).
# Esta es la del script genérico viejo — NO sirve para la trasera. Recalibrar.
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
# fin del bloque a calibrar
# ============================================================================

# --- Inicialización del sensor (API `csi` de la N6) ---
csi0 = csi.CSI()
csi0.reset()
csi0.pixformat(csi.RGB565)
csi0.framesize(csi.QVGA)
# ⚠️ CRÍTICO — todos los autos en OFF:
csi0.auto_whitebal(False)
csi0.auto_gain(False)
csi0.auto_exposure(False, exposure_us=EXPOSURE_US)
csi0.hmirror(HMIRROR)
csi0.vflip(VFLIP)
csi0.snapshot(time=500)                  # estabilización

uart = UART(UART_PORT, UART_BAUD)

# --- LEDs de diagnóstico (OPCIONALES: si la API difiere en N6, se ignoran) ---
try:
    import pyb
    led_rojo, led_verde, led_azul = pyb.LED(1), pyb.LED(2), pyb.LED(3)
except Exception:
    class _NoLED:
        def on(self): pass
        def off(self): pass
    led_rojo = led_verde = led_azul = _NoLED()

def _set(led, on):
    try:
        led.on() if on else led.off()
    except Exception:
        pass

try:
    for _ in range(2):                    # parpadeo de inicio (trasera = 2 azules, distinto del frontal)
        _set(led_azul, True);  time.sleep_ms(300)
        _set(led_azul, False); time.sleep_ms(300)
except Exception:
    pass

clock = time.clock()

# ============================================================================
# TRANSFORMACIÓN pixel → coord física (con clamp anti-crash) — IDÉNTICA al H7
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

    _set(led_rojo,  bool(naranja_blobs))
    _set(led_verde, bool(amarillo_blobs))
    _set(led_azul,  bool(azul_blobs))

    Xp,  Ypc  = procesar_blob(naranja_blobs)
    Xam, Yamc = procesar_blob(amarillo_blobs)
    Xaz, Yazc = procesar_blob(azul_blobs)

    packet = bytearray([201, Xp, Ypc, 202, Xam, Yamc, 203, Xaz, Yazc])
    uart.write(packet)
    # SIN print() en producción.
