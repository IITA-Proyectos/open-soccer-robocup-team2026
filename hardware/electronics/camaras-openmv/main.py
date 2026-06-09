# main.py — Cámara Soccer 2026 (OpenMV) — DETECCIÓN (la que ANDA) + COMUNICACIÓN SEGURA (contrato v2)
#
# Qué es
# ------
# Es el script de cámara que FUNCIONA en banco (`mainopenmvcomvieja.py`:
# calibración LAB real, LEDs como indicador, recorte inferior, homografía de
# Elías 2026-06-07) con UN SOLO cambio: la **forma de codificar y enviar** los
# datos por UART pasa al **formato seguro de 11 bytes (contrato v2)** que el
# firmware del TOP sabe parsear (`cameras_runtime.cpp` / `cameras.h`).
#
# Todo lo demás se mantiene IGUAL que la versión que anda:
#   • UART = pyb.UART(3, 19200)   (mismo puerto y baud que el TOP espera)
#   • LEDs (rojo=pelota / verde=arco amarillo / azul=arco azul) como indicador.
#   • Umbrales LAB (calibración).
#   • Recorte inferior (ROI) para no detectar partes del robot.
#   • Homografía H (cm desde el centro del lente, atada a VGA).
#
# Qué cambió (SOLO la comunicación)
# ---------------------------------
#   Viejo : packet de 9 bytes [201,X,Y+100, 202,..., 203,...], X SIN codificar
#           (puede ser negativo → rompe bytearray), "no detectado" = 0/0, sin CRC.
#   Nuevo : packet de 11 bytes. X e Y codificados IGUAL (valor+100, rango [0,200]);
#           "no detectado" = 255,255 (inalcanzable desde una detección real);
#           al final CRC8 (XOR de los 9 bytes de datos) + END=254.
#
# Uso
# ---
#   • Va en AMBAS cámaras por ahora (el TOP distingue frontal/trasera por el
#     puerto UART, y rota 180° la trasera del lado del TOP, no acá).
#   • Copiar este archivo como main.py a la cámara (OpenMV IDE → guardar en la
#     placa como main.py, o copiarlo a la unidad de la cámara).
#
# La versión vieja (insegura) queda en `main-comunicacion-vieja.py` para pruebas.

from pyb import UART
import sensor, image, time
import math
import pyb

# ----- Configuración UART (igual que el viejo; el TOP espera 19200 en este puerto) -----
uart = UART(3, 19200)

# ----- LEDs (indicador de detección) ------
led_rojo = pyb.LED(1)    # Naranja / Pelota
led_verde = pyb.LED(2)   # Amarillo / Arco
led_azul = pyb.LED(3)    # Azul / Arco

# Parpadeo de arranque (LED verde).
led_verde.on()
time.sleep(0.4)
led_verde.off()
time.sleep(0.4)
led_verde.on()
time.sleep(0.4)
led_verde.off()

# ----- Inicializar cámara -----
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.VGA)   # H calibrada en VGA — NO cambiar sin recalibrar.

sensor.set_auto_whitebal(True)
sensor.set_auto_gain(True)
#sensor.set_auto_exposure(True)

sensor.skip_frames(time=2000)

#sensor.set_auto_exposure(False, exposure_us=37000)  # (opcional: exposición fija)

# invertir imagen (montaje 180°)
sensor.set_hmirror(True)
sensor.set_vflip(True)

clock = time.clock()

# ----- Constantes físicas -----
h = 18.7              # altura cámara (cm)
r = 13.5/(2*math.pi)  # radio pelota (cm)

# ----- Umbrales LAB (calibración) -----
naranja_threshold = (21, 67, 18, 79, -32, 127)    # Pelota naranja  -> header 201
amarillo_threshold = (17, 70, -27, 14, 38, 111)   # Arco amarillo   -> header 202
azul_threshold = (4, 38, -13, 57, -64, -4)        # Arco azul       -> header 203

# ===== Contrato v2 (formato seguro que parsea el TOP) — packet de 11 bytes =====
SENTINEL_CODED = 255    # X_coded==255 y Y_coded==255  ->  objeto NO detectado
COORD_OFFSET = 100      # coded = valor + 100, en AMBOS ejes (X igual que Y)
HEADER1, HEADER2, HEADER3 = 201, 202, 203
END_BYTE = 254          # fin de trama


def crc8(data):
    # XOR de los 9 bytes de datos (3 headers + 6 coords). Debe coincidir EXACTO
    # con cam_crc8() del TOP. Detecta bit-flips en el enlace UART.
    c = 0
    for b in data:
        c ^= b
    return c & 0xFF


# ----- Funciones -----

def transformarcoordenadas(u, v):
    # Matriz de transformación homográfica (Elías 2026-06-07, VGA).
    H = [[ 7.54504107e-01, 1.54808424e-02,  -1.96304100e+02],
         [-1.40623499e-01, -2.05684020e-01,  2.30315983e+02],
         [-7.07447958e-03,  8.46088118e-02,  1.00000000e+00]]

    denominator = H[2][0]*u + H[2][1]*v + H[2][2]
    if abs(denominator) < 1e-6:
        return SENTINEL_CODED, SENTINEL_CODED   # caso degenerado -> no detectado

    x = (H[0][0]*u + H[0][1]*v + H[0][2]) / denominator
    y = (H[1][0]*u + H[1][1]*v + H[1][2]) / denominator

    # Corrección de perspectiva (la pelota tiene radio, no es un punto en el suelo).
    X = x * (h - r) / h
    Y = y * (h - r) / h

    # Clamp físico [-100,100] en AMBOS ejes (X igual que Y).
    X = max(-100, min(100, X))
    Y = max(-100, min(100, Y))

    # Codificar AMBOS: coded = valor + 100  ->  [0,200]. 255 (sentinel) inalcanzable.
    X_coded = max(0, min(200, round(X) + COORD_OFFSET))
    Y_coded = max(0, min(200, round(Y) + COORD_OFFSET))
    return X_coded, Y_coded


def procesar_blob(blobs, dibujar_color):
    if not blobs:
        return SENTINEL_CODED, SENTINEL_CODED   # no detectado -> 255,255

    # Blob más grande (asumimos que es el arco o la pelota).
    largest_blob = max(blobs, key=lambda b: b.pixels())

    # Dibujos de ayuda en el IDE (no afectan lo que se envía).
    img.draw_rectangle(largest_blob.rect(), color=dibujar_color)
    img.draw_cross(largest_blob.cx(), largest_blob.cy(), color=(255, 255, 255))

    # Coordenadas CODIFICADAS (X_coded, Y_coded) listas para el packet.
    return transformarcoordenadas(largest_blob.cx(), largest_blob.cy())


# ----- Bucle principal -----

while(True):
    clock.tick()
    img = sensor.snapshot()

    # ROI: recorta el 3% de abajo (donde aparece el robot) — no busca ahí.
    roi = (0, 0, img.width(), int(img.height() * 0.97))

    # Detectar blobs (con el recorte inferior).
    naranja_blobs = img.find_blobs([naranja_threshold], roi=roi, pixels_threshold=7, area_threshold=7, merge=True)
    azul_blobs = img.find_blobs([azul_threshold], roi=roi, pixels_threshold=600, area_threshold=600, merge=True)
    amarillo_blobs = img.find_blobs([amarillo_threshold], roi=roi, pixels_threshold=600, area_threshold=600, merge=True)

    # Encender LEDs según detección.
    led_rojo.off()
    if naranja_blobs:
        led_rojo.on()

    led_azul.off()
    if azul_blobs:
        led_azul.on()

    led_verde.off()
    if amarillo_blobs:
        led_verde.on()

    # Procesar blobs -> coords CODIFICADAS (o 255,255 si no detecta).
    Xp, Yp = procesar_blob(naranja_blobs, (255, 0, 0))
    Xam, Yam = procesar_blob(amarillo_blobs, (0, 255, 0))
    Xaz, Yaz = procesar_blob(azul_blobs, (0, 0, 255))

    # ----------------------------
    # Armar paquete UART — contrato v2: 9 bytes de datos + CRC8 + END = 11 bytes.
    data = [HEADER1, Xp, Yp,
            HEADER2, Xam, Yam,
            HEADER3, Xaz, Yaz]
    packet = bytearray(data + [crc8(data), END_BYTE])

    uart.write(packet)
    print("Enviando:", list(packet))   # (para banco; se puede sacar en competencia)
