# ═══════════════════════════════════════════════════════════════════════════
#  ⚠️  TEMPLATE v1 SUPERADO — ya existe el script v2 real; flasheá ése  ⚠️
#
#  Este "template objetivo" emite el packet v1 (9 bytes, X sin offset, SIN
#  CRC/END) y describe el sentinel viejo (X=0, Y_coded=0). Quedó SUPERADO: el
#  contrato cámara→TOP saltó a v2 el 2026-06-03 (commit d230de5) y el script
#  REAL de la cámara trasera ya está implementado en esta carpeta:
#      cam-trasera-n6.py   ← FLASHEAR ÉSTE (v2: 11 B · X+100 · sentinel 255 · CRC8+END)
#  Contrato canónico: docs/firmware/CONTRATO-DATOS-CAMARAS.md
#
#  NO flashear este template. Si contradice el repo vivo, gana el repo vivo.
# ═══════════════════════════════════════════════════════════════════════════

# cam_trasera.py — Template objetivo de la cámara TRASERA del robot Soccer 2026
#
# Estado: TEMPLATE — los valores marcados [CALIBRAR] son placeholders y deben
# medirse en el robot real antes de flashear. Ver
# `cameraBack-pack/04-calibracion-lab-y-homografia.md` para el procedimiento.
#
# IMPORTANTE: la cámara trasera NO rota sus coordenadas internamente. La
# rotación 180° la hace el TOP en cameras_fusion.cpp líneas 25-29. La cámara
# reporta lo que ve en SU PROPIO frame, como si fuera la única cámara.
#
# Cambios respecto a `current-generic.py` (bugs P0 corregidos):
#   1. Sentinel: cuando no hay blob, retorna (X=0, Y_coded=0) — no (0, 100).
#   2. Clamp anti-crash: todos los valores en [0, 255] antes de bytearray.
#   3. set_auto_whitebal(False) + set_auto_gain(False) + set_auto_exposure(False).
#   4. pixels_threshold de pelota subido de 7 a 20.
#   5. Sin print() en loop.

from pyb import UART
import sensor, image, time, math, pyb

# ============================================================================
# CONFIGURACIÓN ESPECÍFICA DE LA CÁMARA TRASERA
# ============================================================================
CAM_ID = 1          # 1 = TRASERA (info; el TOP distingue por puerto UART, no por este)
UART_PORT = 3       # UART3 del OpenMV (el único disponible en H7)
UART_BAUD = 19200

# Exposición fija — [CALIBRAR en la cancha de Incheon, puede diferir del frontal]:
EXPOSURE_US = 37000     # placeholder; medir con auto_exposure(True) y get_exposure_us()

# Corrección geométrica — [VERIFICAR según montaje físico — PROBABLEMENTE distinto al frontal]:
HMIRROR = True
VFLIP   = True
# Si la cámara trasera está montada como espejo de la frontal (cable hacia el
# otro lado), HMIRROR puede tener que ser False. Verificar con preview del IDE.

# Homografía — [CALIBRAR con 4 puntos conocidos DETRÁS del robot]:
H_MATRIX = [
    [ 4.49341044e-02, -9.48228474e-01,  7.78932109e+02],
    [-2.39913185e+00, -5.65934886e-02,  3.91128921e+02],
    [-1.81344856e-03,  1.15408531e-01,  1.00000000e+00]
]
# ⚠️ ESTA H ES LA DEL SCRIPT GENÉRICO ACTUAL — NO sirve para la cámara trasera.
# La cámara trasera necesita SU PROPIA H, calibrada con 4 puntos en el suelo
# DETRÁS del robot. Ver cameraBack-pack/04-calibracion-lab-y-homografia.md §3.

# Constantes físicas — [MEDIR para este robot, puede diferir del frontal]:
CAM_HEIGHT_CM = 18.7                    # altura de la cámara trasera sobre el suelo
BALL_RADIUS_CM = 13.5 / (2 * math.pi)   # radio pelota

# Thresholds LAB — [CALIBRAR; pueden ser iguales al frontal si la iluminación recibida es similar]:
NARANJA_THRESHOLD  = (21, 67, 18, 79, -32, 127)
AMARILLO_THRESHOLD = (17, 70, -27, 14, 38, 111)
AZUL_THRESHOLD     = (4, 36, -13, 57, -64, -4)

# Filtros de área mínima:
NARANJA_PIXELS_MIN  = 20
AMARILLO_PIXELS_MIN = 600
AZUL_PIXELS_MIN     = 300

# Sentinel "no detectado":
SENTINEL_X = 0
SENTINEL_Y_CODED = 0   # → Y = 0 - 100 = -100 en el parser TOP → is_visible = False

# ============================================================================
# INICIALIZACIÓN DEL SENSOR
# ============================================================================
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)

# ⚠️ CRÍTICO — todos los autos en False:
sensor.set_auto_whitebal(False)
sensor.set_auto_gain(False)
sensor.set_auto_exposure(False, exposure_us=EXPOSURE_US)

sensor.set_hmirror(HMIRROR)
sensor.set_vflip(VFLIP)

sensor.skip_frames(time=500)

uart = UART(UART_PORT, UART_BAUD)

# LEDs:
led_rojo  = pyb.LED(1)   # naranja / pelota
led_verde = pyb.LED(2)   # amarillo / arco
led_azul  = pyb.LED(3)   # azul / arco
# Parpadeo de inicio distinto del frontal para identificar visualmente:
for _ in range(2):
    led_azul.on(); time.sleep_ms(300); led_azul.off(); time.sleep_ms(300)

clock = time.clock()

# ============================================================================
# FUNCIONES DE TRANSFORMACIÓN — con clamp anti-crash
# ============================================================================
def transformar(u, v):
    """De pixel (u, v) a coord física (X_uint8, Y_coded_uint8) con clamp."""
    H = H_MATRIX
    denom = H[2][0] * u + H[2][1] * v + H[2][2]
    if abs(denom) < 1e-6:
        return SENTINEL_X, SENTINEL_Y_CODED

    x = (H[0][0] * u + H[0][1] * v + H[0][2]) / denom
    y = (H[1][0] * u + H[1][1] * v + H[1][2]) / denom

    # Corrección de perspectiva:
    X = x * (CAM_HEIGHT_CM - BALL_RADIUS_CM) / CAM_HEIGHT_CM
    Y = y * (CAM_HEIGHT_CM - BALL_RADIUS_CM) / CAM_HEIGHT_CM

    # Clamp al rango físico:
    X = max(-127, min(200, X))
    Y = max(-100, min(100, Y))

    Y_coded = int(Y) + 100   # ∈ [0, 200]
    X_int   = int(X)

    if X_int == 0 and Y_coded == 0:
        X_int = 1   # evitar colisión con sentinel

    # CLAMP FINAL a uint8 — anti-crash en bytearray:
    X_int   = max(0, min(255, X_int))
    Y_coded = max(0, min(255, Y_coded))
    return X_int, Y_coded


def procesar_blob(blobs):
    if not blobs:
        return SENTINEL_X, SENTINEL_Y_CODED
    blob = max(blobs, key=lambda b: b.pixels())
    return transformar(blob.cx(), blob.cy())


# ============================================================================
# LOOP PRINCIPAL — IDÉNTICO al de la cámara frontal
# ============================================================================
# La cámara trasera NO rota nada acá. La rotación 180° la hace el TOP en
# cameras_fusion.cpp. Esta cámara reporta lo que ve en SU PROPIO frame.

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

    led_rojo.value(1 if naranja_blobs else 0)
    led_verde.value(1 if amarillo_blobs else 0)
    led_azul.value(1 if azul_blobs else 0)

    Xp,  Ypc  = procesar_blob(naranja_blobs)
    Xam, Yamc = procesar_blob(amarillo_blobs)
    Xaz, Yazc = procesar_blob(azul_blobs)

    packet = bytearray([201, Xp, Ypc, 202, Xam, Yamc, 203, Xaz, Yazc])
    uart.write(packet)

    # NO print() en producción.
