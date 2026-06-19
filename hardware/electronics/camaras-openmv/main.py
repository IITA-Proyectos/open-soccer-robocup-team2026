from pyb import UART
import sensor, image, time
import math
import pyb

uart = UART(3, 19200)

led_rojo = pyb.LED(1)
led_verde = pyb.LED(2)
led_azul = pyb.LED(3)

led_verde.on()
time.sleep(0.4)
led_verde.off()
time.sleep(0.4)
led_verde.on()
time.sleep(0.4)
led_verde.off()

sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.VGA)

sensor.set_hmirror(True)
sensor.set_vflip(True)

# ----- Color/brillo -----
# White balance y ganancia en AUTO (esto es lo que evita la imagen negra).
sensor.set_auto_whitebal(True)
sensor.set_auto_gain(True)

sensor.skip_frames(time=2000) 

sensor.set_auto_whitebal(False)
sensor.set_auto_gain(False)

sensor.set_auto_whitebal(False, rgb_gain_db=(2.803574, 0.000000, 5.535230)) 
sensor.set_auto_gain(False, gain_db=12.041200) 
sensor.set_auto_exposure(False, exposure_us=100328)
sensor.skip_frames(time=2000) 

clock = time.clock()

h = 8.0
r = 13.5/(2*math.pi)

naranja_threshold = (30, 61, 39, 70, 20, 50)
amarillo_threshold = (40, 65, 0, 20, 10, 30)
azul_threshold = (10, 30, 0, 30, -35, -10)

# ----- Máscara de esquinas superiores (triángulos) — tus valores de antes -----
MASK_TL_W = 220   # triángulo sup-IZQUIERDO: ancho desde la esquina (px)
MASK_TL_H = 50    # triángulo sup-IZQUIERDO: alto  desde el borde de arriba (px)
MASK_TR_W = 220   # triángulo sup-DERECHO:   ancho desde la esquina (px)
MASK_TR_H = 50    # triángulo sup-DERECHO:   alto  desde el borde de arriba (px)
MASK_COLOR = (0, 0, 0)   # negro: fuera de TODOS los thresholds LAB

SENTINEL_CODED = 255
COORD_OFFSET = 100
HEADER1, HEADER2, HEADER3 = 201, 202, 203
END_BYTE = 254


def crc8(data):
    c = 0
    for b in data:
        c ^= b
    return c & 0xFF


def transformarcoordenadas(u, v):
    H = [[ 7.54504107e-01, 1.54808424e-02,  -1.96304100e+02],
         [-1.40623499e-01, -2.05684020e-01,  2.30315983e+02],
         [-7.07447958e-03,  8.46088118e-02,  1.00000000e+00]]
    denominator = H[2][0]*u + H[2][1]*v + H[2][2]
    if abs(denominator) < 1e-6:
        return SENTINEL_CODED, SENTINEL_CODED
    x = (H[0][0]*u + H[0][1]*v + H[0][2]) / denominator
    y = (H[1][0]*u + H[1][1]*v + H[1][2]) / denominator
    X = x * (h - r) / h
    Y = y * (h - r) / h
    X = max(-100, min(100, X))
    Y = max(-100, min(100, Y))
    X_coded = max(0, min(200, round(X) + COORD_OFFSET))
    Y_coded = max(0, min(200, round(Y) + COORD_OFFSET))
    return X_coded, Y_coded


def enmascarar_esquinas(img):
    # Pinta de negro las dos esquinas triangulares de arriba (relleno por filas).
    w = img.width()
    for y in range(0, MASK_TL_H):
        span = int(MASK_TL_W * (1 - y / MASK_TL_H))
        if span > 0:
            img.draw_line(0, y, span, y, color=MASK_COLOR)
    for y in range(0, MASK_TR_H):
        span = int(MASK_TR_W * (1 - y / MASK_TR_H))
        if span > 0:
            img.draw_line(w - span, y, w, y, color=MASK_COLOR)


def procesar_blob(blobs, dibujar_color):
    if not blobs:
        return SENTINEL_CODED, SENTINEL_CODED
    largest_blob = max(blobs, key=lambda b: b.pixels())
    img.draw_rectangle(largest_blob.rect(), color=dibujar_color)
    img.draw_cross(largest_blob.cx(), largest_blob.cy(), color=(255, 255, 255))
    return transformarcoordenadas(largest_blob.cx(), largest_blob.cy())


while(True):
    clock.tick()
    img = sensor.snapshot()

    # Tapar las esquinas triangulares de arriba (falsos positivos fuera de cancha).
    enmascarar_esquinas(img)

    roi = (0, 0, img.width(), int(img.height() * 0.97))

    naranja_blobs = img.find_blobs([naranja_threshold], roi=roi, pixels_threshold=7, area_threshold=7, merge=True)
    azul_blobs = img.find_blobs([azul_threshold], roi=roi, pixels_threshold=600, area_threshold=600, merge=True)
    amarillo_blobs = img.find_blobs([amarillo_threshold], roi=roi, pixels_threshold=600, area_threshold=600, merge=True)

    led_rojo.off()
    if naranja_blobs:
        led_rojo.on()

    led_azul.off()
    if azul_blobs:
        led_azul.on()

    led_verde.off()
    if amarillo_blobs:
        led_verde.on()

    Xp, Yp = procesar_blob(naranja_blobs, (255, 0, 0))
    Xam, Yam = procesar_blob(amarillo_blobs, (0, 255, 0))
    Xaz, Yaz = procesar_blob(azul_blobs, (0, 0, 255))

    data = [HEADER1, Xp, Yp,
            HEADER2, Xam, Yam,
            HEADER3, Xaz, Yaz]
    packet = bytearray(data + [crc8(data), END_BYTE])

    uart.write(packet)
    print("Enviando:", list(packet))  
    
    # Imprime las 3 líneas LISTAS PARA PEGAR como calibración fija:
    #g = sensor.get_gain_db()
    #rgb = sensor.get_rgb_gain_db()
    #e = sensor.get_exposure_us()
    #print("=========== COPIA ESTAS 3 LINEAS ===========")
    #print("sensor.set_auto_gain(False, gain_db=%f)" % g)
    #print("sensor.set_auto_whitebal(False, rgb_gain_db=(%f, %f, %f))" % (rgb[0], rgb[1], rgb[2]))
    #print("sensor.set_auto_exposure(False, exposure_us=%d)" % e)
    #print("============================================")
