# ═══════════════════════════════════════════════════════════════════════════
#  ⚠️  TEMPLATE v1 SUPERADO — ya existe el script v2 real; flasheá ése  ⚠️
#
#  Este "template objetivo" emite el packet v1 (9 bytes, X sin offset, SIN
#  CRC/END) y describe el sentinel viejo (X=0, Y_coded=0). Quedó SUPERADO: el
#  contrato cámara→TOP saltó a v2 el 2026-06-03 (commit d230de5) y el script
#  REAL de la cámara frontal ya está implementado en esta carpeta:
#      cam-frontal-n6.py   ← FLASHEAR ÉSTE (v2: 11 B · X+100 · sentinel 255 · CRC8+END)
#  Contrato canónico: docs/firmware/CONTRATO-DATOS-CAMARAS.md
#
#  NO flashear este template. Si contradice el repo vivo, gana el repo vivo.
# ═══════════════════════════════════════════════════════════════════════════

# cam_frontal.py — Template objetivo de la cámara FRONTAL del robot Soccer 2026
#
# Estado: TEMPLATE — los valores marcados [CALIBRAR] son placeholders y deben
# medirse en el robot real antes de flashear. Ver
# `cameraFront-pack/04-calibracion-lab-y-homografia.md` para el procedimiento.
#
# Cambios respecto a `current-generic.py` (bugs P0 corregidos):
#   1. Sentinel: cuando no hay blob, retorna (X=0, Y_coded=0) — no (0, 100).
#   2. Clamp anti-crash: todos los valores en [0, 255] antes de bytearray.
#   3. set_auto_whitebal(False) + set_auto_gain(False) + set_auto_exposure(False).
#   4. pixels_threshold de pelota subido de 7 a 20.
#   5. Sin print() en loop (consume ~3 ms).

from pyb import UART
import sensor, image, time, math, pyb

# ============================================================================
# CONFIGURACIÓN ESPECÍFICA DE LA CÁMARA FRONTAL
# ============================================================================
CAM_ID = 0          # 0 = FRONTAL (info; el TOP distingue por puerto UART, no por este)
UART_PORT = 3       # UART3 del OpenMV (el único disponible en H7)
UART_BAUD = 19200

# Exposición fija — [CALIBRAR en la cancha de Incheon]:
EXPOSURE_US = 37000     # placeholder; medir con auto_exposure(True) y get_exposure_us()

# Corrección geométrica — [VERIFICAR según montaje físico]:
HMIRROR = True
VFLIP   = True

# Homografía — [CALIBRAR con 4 puntos conocidos para la posición frontal]:
H_MATRIX = [
    [ 4.49341044e-02, -9.48228474e-01,  7.78932109e+02],
    [-2.39913185e+00, -5.65934886e-02,  3.91128921e+02],
    [-1.81344856e-03,  1.15408531e-01,  1.00000000e+00]
]
# Esta H es la del script genérico actual — placeholder de desarrollo.
# Recalibrar para la posición de montaje real de la cámara frontal.

# Constantes físicas — [MEDIR para este robot]:
CAM_HEIGHT_CM = 18.7                    # altura de la cámara sobre el suelo
BALL_RADIUS_CM = 13.5 / (2 * math.pi)   # radio pelota = circunferencia / 2π

# Thresholds LAB — [CALIBRAR en la cancha de Incheon con threshold editor del IDE]:
NARANJA_THRESHOLD  = (21, 67, 18, 79, -32, 127)    # pelota naranja
AMARILLO_THRESHOLD = (17, 70, -27, 14, 38, 111)    # arco amarillo
AZUL_THRESHOLD     = (4, 36, -13, 57, -64, -4)     # arco azul

# Filtros de área mínima:
NARANJA_PIXELS_MIN  = 20    # ⚠️ subido de 7 (actual) a 20 para evitar ruido
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

sensor.skip_frames(time=500)            # estabilización

uart = UART(UART_PORT, UART_BAUD)

# LEDs:
led_rojo  = pyb.LED(1)   # naranja / pelota
led_verde = pyb.LED(2)   # amarillo / arco
led_azul  = pyb.LED(3)   # azul / arco
# Parpadeo de inicio:
for _ in range(3):
    led_verde.on(); time.sleep_ms(200); led_verde.off(); time.sleep_ms(200)

clock = time.clock()

# ============================================================================
# FUNCIONES DE TRANSFORMACIÓN — con clamp anti-crash
# ============================================================================
def transformar(u, v):
    """De pixel (u, v) a coord física (X_uint8, Y_coded_uint8) con clamp."""
    H = H_MATRIX
    denom = H[2][0] * u + H[2][1] * v + H[2][2]
    if abs(denom) < 1e-6:
        return SENTINEL_X, SENTINEL_Y_CODED   # caso degenerado → marcar no-detectado

    x = (H[0][0] * u + H[0][1] * v + H[0][2]) / denom
    y = (H[1][0] * u + H[1][1] * v + H[1][2]) / denom

    # Corrección de perspectiva (pelota tiene radio, no es punto en el suelo)
    X = x * (CAM_HEIGHT_CM - BALL_RADIUS_CM) / CAM_HEIGHT_CM
    Y = y * (CAM_HEIGHT_CM - BALL_RADIUS_CM) / CAM_HEIGHT_CM

    # Clamp al rango físico plausible
    X = max(-127, min(200, X))
    Y = max(-100, min(100, Y))

    Y_coded = int(Y) + 100   # ∈ [0, 200]
    X_int   = int(X)

    # Caso borde: si X=0 e Y=-100 (extremo del clamp), colisiona con el sentinel.
    # Ajustar X a 1 para distinguir detección real del sentinel.
    if X_int == 0 and Y_coded == 0:
        X_int = 1

    # ⚠️ CLAMP FINAL a uint8 — anti-crash en bytearray:
    X_int   = max(0, min(255, X_int))
    Y_coded = max(0, min(255, Y_coded))
    return X_int, Y_coded


def procesar_blob(blobs):
    """Procesa lista de blobs, retorna (X_uint8, Y_coded_uint8). Sentinel si lista vacía."""
    if not blobs:
        return SENTINEL_X, SENTINEL_Y_CODED
    blob = max(blobs, key=lambda b: b.pixels())
    return transformar(blob.cx(), blob.cy())


# ============================================================================
# LOOP PRINCIPAL
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

    # LEDs de diagnóstico:
    led_rojo.value(1 if naranja_blobs else 0)
    led_verde.value(1 if amarillo_blobs else 0)
    led_azul.value(1 if azul_blobs else 0)

    Xp,  Ypc  = procesar_blob(naranja_blobs)
    Xam, Yamc = procesar_blob(amarillo_blobs)
    Xaz, Yazc = procesar_blob(azul_blobs)

    # Todos los valores ya son uint8 por el clamp en transformar/procesar_blob.
    packet = bytearray([201, Xp, Ypc, 202, Xam, Yamc, 203, Xaz, Yazc])
    uart.write(packet)

    # NO print() en producción — consume ~3 ms y reduce fps.
