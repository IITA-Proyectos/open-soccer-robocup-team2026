# cam-frontal-n6.py — Cámara FRONTAL del robot Soccer 2026 — OpenMV N6 (STM32N6)
#
# Port del template H7 (`target-cam-frontal-template.py`) a la OpenMV **N6**.
# La lógica de detección y el protocolo UART son IDÉNTICOS al template; lo único
# que cambió es la capa de cámara: módulo `sensor` (deprecado desde fw 4.5) → `csi`.
# Bugs P0 ya corregidos (igual que el template): sentinel no-blob → Y_coded=0,
# clamp anti-crash [0,255], autos (WB/gain/exposición) en OFF.
#
# ╔══════════════════════════════════════════════════════════════════════════╗
# ║ ★ ANTES DE CONFIAR EN ESTE SCRIPT — CONFIRMAR / CALIBRAR EN BANCO (N6) ★   ║
# ╠══════════════════════════════════════════════════════════════════════════╣
# ║ [UART]  UART_PORT: la N6 tiene 3 UART. Confirmar cuál mapea a los pines    ║
# ║         que van al Serial3 del Teensy (frontal = conector U8). Probar 1/2/3.║
# ║ [API]   Migrado a `csi`. Si algún `csi.*` da error, ver la versión exacta  ║
# ║         de la API en docs.openmv.io/library/omv.csi.html (los nombres de   ║
# ║         constantes/métodos pueden variar por versión de firmware).         ║
# ║ [LAB]   Los 3 thresholds (naranja/amarillo/azul) son del sensor VIEJO (H7).║
# ║         La N6 tiene OTRO sensor (PAG7936 global shutter) → RECALIBRAR con   ║
# ║         el threshold editor del IDE, en la luz real. SIN ESTO NO DETECTA.   ║
# ║ [EXPO]  EXPOSURE_US es placeholder; re-medir (poner auto_exposure(True),    ║
# ║         leer el valor, fijarlo). Global shutter → conviene exposición baja. ║
# ║ [HOMOG] H_MATRIX es placeholder de desarrollo; recalibrar para ESTA cámara.║
# ║ [FLIP]  HMIRROR/VFLIP según el montaje real (verificar con preview del IDE).║
# ╚══════════════════════════════════════════════════════════════════════════╝

import csi, image, time, math

# --- UART: en N6 el estándar es machine.UART; pyb como fallback ---
try:
    from machine import UART
except ImportError:
    from pyb import UART

# ============================================================================
# ★ BLOQUE A CONFIRMAR / CALIBRAR EN BANCO (N6) ★
# ============================================================================
CAM_ID    = 0           # 0 = FRONTAL (informativo; el TOP distingue por puerto UART)
UART_PORT = 3           # ⚠️ CONFIRMAR en N6 — ¿cuál de los 3 UART va al Serial3 del Teensy?
UART_BAUD = 19200       # debe coincidir con cameras_runtime.cpp del TOP (no cambiar)

EXPOSURE_US = 37000     # ⚠️ RE-MEDIR en la N6 (global shutter)

HMIRROR = True          # ⚠️ VERIFICAR con preview del IDE
VFLIP   = True          # ⚠️ VERIFICAR con preview del IDE

# ⚠️ RECALIBRAR H_MATRIX para la cámara frontal (4 puntos conocidos en el suelo).
# Esta es la H del script genérico viejo — placeholder de desarrollo.
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
# fin del bloque a calibrar
# ============================================================================

# --- Inicialización del sensor (API `csi` de la N6) ---
csi0 = csi.CSI()
csi0.reset()
csi0.pixformat(csi.RGB565)
csi0.framesize(csi.QVGA)
# ⚠️ CRÍTICO — todos los autos en OFF (si no, la luz invalida los thresholds LAB):
csi0.auto_whitebal(False)
csi0.auto_gain(False)
csi0.auto_exposure(False, exposure_us=EXPOSURE_US)
csi0.hmirror(HMIRROR)
csi0.vflip(VFLIP)
csi0.snapshot(time=500)                  # estabilización (reemplaza skip_frames)

uart = UART(UART_PORT, UART_BAUD)

# --- LEDs de diagnóstico (OPCIONALES: si la API de LED difiere en N6, se ignoran) ---
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
    for _ in range(3):                    # parpadeo de inicio (frontal = 3 verdes)
        _set(led_verde, True);  time.sleep_ms(200)
        _set(led_verde, False); time.sleep_ms(200)
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
# LOOP PRINCIPAL — detección + 9 bytes por UART (IDÉNTICO al H7)
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

    # Todos uint8 por el clamp → no crashea bytearray:
    packet = bytearray([201, Xp, Ypc, 202, Xam, Yamc, 203, Xaz, Yazc])
    uart.write(packet)
    # SIN print() en producción (consume ~3 ms y baja los fps).
